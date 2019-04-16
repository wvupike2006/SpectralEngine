#pragma once
#include "D3D12CommandAllocator.h"
#include "frameBufferCount.h"
#include "D3D12GraphicsCommandList.h"
#include "Frustum.h"
#include "Array.h"
#include "Range.h"
#include "ResizingArray.h"
#include "FastIterationHashSet.h"
#include "ReflectionCamera.h"
#include "GraphicsEngine.h"
#include "RenderPassMessage.h"

template<class Camera_t, bool isStaticNumberOfCameras, std::size_t initialSize>
struct CameraStoragePicker;

template<class Camera_t, std::size_t initialSize>
struct CameraStoragePicker<Camera_t, true, initialSize>
{
	using iterator = typename Array<Camera_t, initialSize>::iterator;
	using const_iterator = typename Array<Camera_t, initialSize>::const_iterator;
	Array<Camera_t, initialSize> mCameras;
};

template<class Camera_t, std::size_t initialSize>
struct CameraStoragePicker<Camera_t, false, initialSize>
{
private:
	using Container = FastIterationHashMap<unsigned long, Camera_t, std::hash<unsigned long>, std::equal_to<unsigned long>, std::allocator<unsigned long>, unsigned long>;
public:
	using iterator = typename Container::iterator;
	using const_iterator = typename Container::const_iterator;
	Container mCameras;
};

//use std::tuple<std::integral_constant<unsigned int, value>, ...> for dependencies
//use std::tuple<std::integral_constant<D3D12_RESOURCE_STATES, value>, ...> for dependencyStates
template<class Camera_t, D3D12_RESOURCE_STATES state1, class Dependencies_t, class DependencyStates_t, unsigned int commandListsPerFrame1, D3D12_RESOURCE_STATES stateAfter1 = state1, bool isStaticNumberOfCameras = false, std::size_t initialSize = 0u>
class RenderSubPass : public CameraStoragePicker<Camera_t, isStaticNumberOfCameras, initialSize>
{
	using CameraStoragePicker_t = CameraStoragePicker<Camera_t, isStaticNumberOfCameras, initialSize>;
public:
	using Camera = Camera_t;
	using CameraIterator = typename CameraStoragePicker_t::iterator;
	using ConstCameraIterator = typename CameraStoragePicker_t::const_iterator;
	using Dependencies = Dependencies_t;
	constexpr static auto state = state1;
	constexpr static auto stateAfter = stateAfter1;
	using DependencyStates = DependencyStates_t;
	constexpr static auto commandListsPerFrame = commandListsPerFrame1;
	constexpr static bool isPresentSubPass = false;

	class AddCameraRequest : public RenderPassMessage
	{
		static void execute(RenderPassMessage& message, void* tr, void* gr)
		{
			AddCameraRequest& addRequest = static_cast<AddCameraRequest&>(message);
			auto& cameras = addRequest.subPass->mCameras;
			cameras.insert(addRequest.entity, std::move(addRequest.camera));
			addRequest.callback(addRequest, tr, gr);
		}
	public:
		RenderSubPass* subPass;
		unsigned long entity;
		void(*callback)(AddCameraRequest&, void* tr, void* gr);
		Camera camera;

		AddCameraRequest(unsigned long entity1, Camera&& camera1, void(*callback1)(AddCameraRequest&, void* tr, void* gr)) : RenderPassMessage{execute}, entity(entity1), camera(std::move(camera1)), callback(callback1) {}
	};

	class RemoveCamerasRequest : public RenderPassMessage
	{
		static void execute(RenderPassMessage& message, void* tr, void* gr)
		{
			RemoveCamerasRequest& removeRequest = static_cast<RemoveCamerasRequest&>(message);
			auto& cameras = removeRequest.subPass->mCameras;
			cameras.erase(removeRequest.entity);
			removeRequest.callback(removeRequest, tr, gr);
		}
	public:
		RenderSubPass* subPass;
		unsigned long entity;
		void(*callback)(RemoveCamerasRequest&, void* tr, void* gr);

		RemoveCamerasRequest(unsigned long entity1, void(*callback1)(RemoveCamerasRequest&, void* tr, void* gr)) : RenderPassMessage{execute}, entity(entity1), callback(callback1) {}
	};

	RenderSubPass() noexcept {}
	RenderSubPass(RenderSubPass<Camera_t, state1, Dependencies_t, DependencyStates_t, commandListsPerFrame1, stateAfter1, isStaticNumberOfCameras, initialSize>&& other) noexcept : CameraStoragePicker_t(std::move(static_cast<CameraStoragePicker_t&>(other))) {}

	void operator=(RenderSubPass<Camera_t, state1, Dependencies_t, DependencyStates_t, commandListsPerFrame1, stateAfter1, isStaticNumberOfCameras, initialSize>&& other) noexcept
	{
		this->mCameras = std::move(other.mCameras);
	}
	
	Range<CameraIterator> cameras() noexcept
	{
		return { this->mCameras.begin(), this->mCameras.end()};
	}

	Range<ConstCameraIterator> cameras() const noexcept
	{
		return { this->mCameras.begin(), this->mCameras.end() };
	}

	unsigned int cameraCount() const noexcept
	{
		return (unsigned int)this->mCameras.size();
	}

	CameraIterator findCamera(unsigned long entity) noexcept
	{
		return this->mCameras.find(entity);
	}

	/*
	Can be called from a primary or background thread
	*/
	template<class RenderPass, class ThreadResources, class GlobalResources, bool isStaticNumberOfCameras1 = isStaticNumberOfCameras>
	std::enable_if_t<!isStaticNumberOfCameras1, void> addCamera(AddCameraRequest& addCameraRequest, RenderPass& renderPass, ThreadResources& threadResources, GlobalResources& globalResources)
	{
		addCameraRequest.subPass = this;
		renderPass.addMessage(addCameraRequest, threadResources, globalResources);
	}

	/*
	Can be called from a primary or background thread
	*/
	template<class RenderPass, class ThreadResources, class GlobalResources, bool isStaticNumberOfCameras1 = isStaticNumberOfCameras>
	std::enable_if_t<!isStaticNumberOfCameras1, void> removeCamera(RemoveCamerasRequest& removeCamerasRequest, RenderPass& renderPass, ThreadResources& threadResources, GlobalResources& globalResources) noexcept
	{
		removeCamerasRequest.subPass = this;
		renderPass.addMessage(removeCamerasRequest, threadResources, globalResources);
	}

	bool isInView() const noexcept
	{
		for (const auto& camera : this->mCameras)
		{
			if (camera.isInView())
			{
				return true;
			}
		}
		return false;
	}

	class ThreadLocal
	{
	protected:
		struct PerFrameData
		{
			Array<D3D12CommandAllocator, commandListsPerFrame> commandAllocators;
			Array<D3D12GraphicsCommandList, commandListsPerFrame> commandLists;
		};
		Array<PerFrameData, frameBufferCount> perFrameDatas;

		void bindRootArguments(ID3D12RootSignature* rootSignature, ID3D12DescriptorHeap* mainDescriptorHeap) noexcept
		{
			auto textureDescriptorTable = mainDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
			for (ID3D12GraphicsCommandList* commandList : currentData->commandLists)
			{
				commandList->SetGraphicsRootSignature(rootSignature);
				commandList->SetDescriptorHeaps(1u, &mainDescriptorHeap);
				commandList->SetGraphicsRootDescriptorTable(4u, textureDescriptorTable);
			}
		}

		void resetCommandLists(uint32_t frameIndex)
		{
			currentData = &perFrameDatas[frameIndex];
			D3D12GraphicsCommandList* commandList = &currentData->commandLists[0u];
			for (ID3D12CommandAllocator* commandAllocator : currentData->commandAllocators)
			{
				auto result = commandAllocator->Reset();
				if (FAILED(result)) throw HresultException(result);

				result = (*commandList)->Reset(commandAllocator, nullptr);
				if (FAILED(result)) throw HresultException(result);
				++commandList;
			}
		}
		ThreadLocal() = delete;
	public:
		ThreadLocal(ThreadLocal&& other) noexcept
		{
			auto end = perFrameDatas.end();
			for (auto perFrameData1 = perFrameDatas.begin(), perFrameData2 = other.perFrameDatas.begin(); perFrameData1 != end; ++perFrameData1, ++perFrameData2)
			{
				auto& commandAllocators1 = perFrameData1->commandAllocators;
				auto& commandAllocators2 = perFrameData2->commandAllocators;
				auto end2 = commandAllocators1.end();
				for (auto commandAllocator1 = commandAllocators1.begin(), commandAllocator2 = commandAllocators2.begin(); commandAllocator1 != end2; ++commandAllocator1, ++commandAllocator2)
				{
					*commandAllocator1 = std::move(*commandAllocator2);
				}

				auto& commandLists1 = perFrameData1->commandLists;
				auto& commandLists2 = perFrameData2->commandLists;
				auto end3 = commandLists1.end();
				for (auto commandList1 = commandLists1.begin(), commandList2 = commandLists2.begin(); commandList1 != end3; ++commandList1, ++commandList2)
				{
					*commandList1 = std::move(*commandList2);
				}
			}
		}
		ThreadLocal(GraphicsEngine& graphicsEngine)
		{
			auto frameIndex = graphicsEngine.frameIndex;
			auto i = 0u;
			for (auto& perFrameData : perFrameDatas)
			{
				auto* commandList = &perFrameData.commandLists[0u];
				for (auto& allocator : perFrameData.commandAllocators)
				{
					allocator.~D3D12CommandAllocator();
					new(&allocator) D3D12CommandAllocator(graphicsEngine.graphicsDevice, D3D12_COMMAND_LIST_TYPE_DIRECT);
					commandList->~D3D12GraphicsCommandList();
					new(commandList) D3D12GraphicsCommandList(graphicsEngine.graphicsDevice, 0u, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr);
					(*commandList)->Close();

#ifdef _DEBUG
					std::wstring name = L"Direct command allocator ";
					name += std::to_wstring(i);
					allocator->SetName(name.c_str());

					name = L"Direct command list ";
					name += std::to_wstring(i);
					(*commandList)->SetName(name.c_str());
#endif // _DEBUG
					++commandList;
				}
				++i;
			}
			currentData = &perFrameDatas[frameIndex];
		}

		void operator=(ThreadLocal&& other) noexcept
		{
			this->~ThreadLocal();
			new(this) ThreadLocal(std::move(other));
		}

		void update1After(GraphicsEngine& graphicsEngine, RenderSubPass<Camera, state1, Dependencies_t, DependencyStates_t, commandListsPerFrame, stateAfter1, isStaticNumberOfCameras, initialSize>& renderSubPass,
			ID3D12RootSignature* rootSignature)
		{
			auto frameIndex = graphicsEngine.frameIndex;
			resetCommandLists(frameIndex);
			bindRootArguments(rootSignature, graphicsEngine.mainDescriptorHeap);
			auto cameras = renderSubPass.cameras();
			for (auto& camera : cameras)
			{
				if (camera.isInView())
				{
					camera.bind(frameIndex, &currentData->commandLists[0u].get(),
						&currentData->commandLists.data()[currentData->commandLists.size()].get());
				}
			}
		}

		void update1AfterFirstThread(GraphicsEngine& graphicsEngine, RenderSubPass<Camera, state1, Dependencies_t, DependencyStates_t, commandListsPerFrame, stateAfter1, isStaticNumberOfCameras, initialSize>& renderSubPass,
			ID3D12RootSignature* rootSignature, uint32_t barrierCount, const D3D12_RESOURCE_BARRIER* barriers)
		{
			auto frameIndex = graphicsEngine.frameIndex;
			resetCommandLists(frameIndex);
			bindRootArguments(rootSignature, graphicsEngine.mainDescriptorHeap);

			if(barrierCount != 0u) currentData->commandLists[0u]->ResourceBarrier(barrierCount, barriers);

			for (auto& camera : renderSubPass.cameras())
			{
				if (camera.isInView())
				{
					camera.bindFirstThread(frameIndex, &currentData->commandLists[0u].get(),
						&currentData->commandLists.data()[currentData->commandLists.size()].get());
				}
			}
		}

		void update1AfterFirstThread(GraphicsEngine& graphicsEngine, RenderSubPass<Camera, state1, Dependencies_t, DependencyStates_t, commandListsPerFrame, stateAfter1, isStaticNumberOfCameras, initialSize>& renderSubPass,
			ID3D12RootSignature* rootSignature)
		{
			auto frameIndex = graphicsEngine.frameIndex;
			resetCommandLists(frameIndex);
			bindRootArguments(rootSignature, graphicsEngine.mainDescriptorHeap);

			for (auto& camera : renderSubPass.cameras())
			{
				if (camera.isInView())
				{
					camera.bindFirstThread(frameIndex, &currentData->commandLists[0u].get(),
						&currentData->commandLists.data()[currentData->commandLists.size()].get());
				}
			}
		}

		void update2(ID3D12CommandList**& commandLists, unsigned int numThreads)
		{
			for (ID3D12GraphicsCommandList* commandList : currentData->commandLists)
			{
				auto result = commandList->Close();
				if (FAILED(result)) throw HresultException(result);
			}

			for (ID3D12GraphicsCommandList* commandList : currentData->commandLists)
			{
				*commandLists = commandList;
				commandLists += numThreads;
			}

		}

		template<class ThreadResources, class GlobalResources>
		void update2LastThread(ID3D12CommandList**& commandLists, unsigned int numThreads, RenderSubPass<Camera, state1, Dependencies_t, DependencyStates_t, commandListsPerFrame, stateAfter1, isStaticNumberOfCameras, initialSize>&,
			ThreadResources&, GlobalResources&)
		{
			update2(commandLists, numThreads);
		}

		ID3D12GraphicsCommandList* lastCommandList() noexcept
		{
			return currentData->commandLists[commandListsPerFrame - 1u];
		}

		ID3D12GraphicsCommandList* firstCommandList() noexcept
		{
			return currentData->commandLists[0];
		}

		PerFrameData* currentData;
	};
};

template<class Camera_t, D3D12_RESOURCE_STATES state1, class Dependencies_t, class DependencyStates_t, unsigned int commandListsPerFrame1, D3D12_RESOURCE_STATES stateAfter1 = state1>
class RenderMainSubPass : public RenderSubPass<Camera_t, state1, Dependencies_t, DependencyStates_t, commandListsPerFrame1, stateAfter1, true, 1u>
{
public:
	class ThreadLocal : public RenderSubPass<Camera_t, state1, Dependencies_t, DependencyStates_t, commandListsPerFrame1, stateAfter1, true, 1u>::ThreadLocal
	{
	public:
		ThreadLocal(GraphicsEngine& graphicsEngine) : RenderSubPass<Camera_t, state1, Dependencies_t, DependencyStates_t, commandListsPerFrame1, stateAfter1, true, 1u>::ThreadLocal(graphicsEngine) {}

		template<class ThreadResources, class GlobalResources>
		void update2LastThread(ID3D12CommandList**& commandLists, unsigned int numThreads, RenderMainSubPass<Camera_t, state1, Dependencies_t, DependencyStates_t, commandListsPerFrame, stateAfter1>& renderSubPass,
			ThreadResources&, GlobalResources&)
		{
			auto cameras = renderSubPass.cameras();
			auto camerasEnd = cameras.end();
			uint32_t barrierCount = 0u;
			D3D12_RESOURCE_BARRIER barriers[2];
			for (auto cam = cameras.begin(); cam != camerasEnd; ++cam)
			{
				assert(barrierCount != 2);
				auto& camera = *cam;
				barriers[barrierCount].Flags = D3D12_RESOURCE_BARRIER_FLAGS::D3D12_RESOURCE_BARRIER_FLAG_NONE;
				barriers[barrierCount].Type = D3D12_RESOURCE_BARRIER_TYPE::D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				barriers[barrierCount].Transition.pResource = camera.getImage();
				barriers[barrierCount].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				barriers[barrierCount].Transition.StateBefore = state;
				barriers[barrierCount].Transition.StateAfter = stateAfter;
				++barrierCount;
			}
			lastCommandList()->ResourceBarrier(barrierCount, barriers);
			update2(commandLists, numThreads);
		}
	};
};

template<class RenderSubPass_t>
class RenderSubPassGroup
{
	ResizingArray<RenderSubPass_t> mSubPasses;
	using SubPasses = decltype(mSubPasses);

	template<class Camera, class CameraIterator, class SubPassIterator>
	class Iterator
	{
		friend class RenderSubPassGroup<RenderSubPass_t>;
		SubPassIterator iterator;
		SubPassIterator iteratorEnd;
		CameraIterator current;
		Iterator(SubPassIterator iterator, SubPassIterator iteratorEnd) : iterator(iterator), iteratorEnd(iteratorEnd), current() 
		{
			if (iterator != iteratorEnd)
			{
				current = iterator->cameras().begin();
			}
		}
	public:
		using value_type = Camera;
		using difference_type = typename std::iterator_traits<Camera*>::difference_type;
		using pointer = typename std::iterator_traits<Camera*>::pointer;
		using reference = typename std::iterator_traits<Camera*>::reference;
		using iterator_category = std::forward_iterator_tag;
		Iterator& operator++() noexcept
		{
			++current;
			auto end = iterator->cameras().end();
			while (current == end)
			{
				++iterator;
				if (iterator == iteratorEnd)
				{
					break;
				}
				current = iterator->cameras().begin();
				end = iterator->cameras().end();
			}
			return *this;
		}

		Iterator operator++(int) noexcept
		{
			Iterator ret{ iterator, current };
			++(*this);
			return ret;
		}

		Camera operator*() noexcept
		{
			return *current;
		}

		Camera* operator->() noexcept
		{
			return &*current;
		}

		bool operator==(const SubPassIterator& other) noexcept
		{
			return iterator == other;
		}

		bool operator!=(const SubPassIterator& other) noexcept
		{
			return iterator != other;
		}
	};
public:
	using Camera = typename RenderSubPass_t::Camera;
	using SubPass = RenderSubPass_t;
	using Dependencies = typename RenderSubPass_t::Dependencies;
	constexpr static auto state = RenderSubPass_t::state;
	constexpr static auto stateAfter = RenderSubPass_t::stateAfter;
	using DependencyStates = typename RenderSubPass_t::DependencyStates;
	unsigned int commandListsPerFrame = 0u; // RenderSubPass_t::commandListsPerFrame * subPasses.size();
	using CameraIterator = Iterator<Camera, typename RenderSubPass_t::CameraIterator, typename SubPasses::iterator>;
	using ConstCameraIterator = Iterator<const Camera, typename RenderSubPass_t::ConstCameraIterator, typename SubPasses::const_iterator>;

	RenderSubPassGroup() = default;
	RenderSubPassGroup(RenderSubPassGroup&&) = default;

	Range<CameraIterator, typename SubPasses::iterator> cameras()
	{
		auto subPassBegin = mSubPasses.begin();
		auto subPassesEnd = mSubPasses.end();
		return { {subPassBegin, subPassesEnd}, subPassesEnd };
	}

	Range<ConstCameraIterator, typename SubPasses::const_iterator> cameras() const
	{
		auto subPassBegin = mSubPasses.begin();
		auto subPassesEnd = mSubPasses.end();
		return { { subPassBegin, subPassesEnd }, subPassesEnd };
	}

	unsigned int cameraCount() const noexcept
	{
		unsigned int count = 0u;
		for (auto& subPass : mSubPasses)
		{
			count += subPass.cameraCount();
		}
		return count;
	}

	Range<typename SubPasses::iterator> subPasses()
	{
		return {mSubPasses};
	}

	template<class RenderPass, class SubPasses, class GlobalResources>
	SubPass& addSubPass(GlobalResources& globalResources, RenderPass& renderPass, SubPasses subPasses)
	{
		std::lock_guard<decltype(globalResources.taskShedular.barrier())> lock(globalResources.taskShedular.barrier());
		auto& subPass = mSubPasses.emplace_back();
		auto end = subPasses.end();
		for (auto start = subPasses.begin(); start != end; ++start)
		{
			start->addSubPass(globalResources);
		}
		commandListsPerFrame += subPass.commandListsPerFrame;
		renderPass.updateBarrierCount();
		return *(mSubPasses.end() - 1u);
	}

	template<class SubPasses, class GlobalResources>
	void removeSubPass(GlobalResources& globalResources, SubPass* subPass, SubPasses subPasses)
	{
		using std::swap; using std::find;
		std::lock_guard<decltype(globalResources.taskShedular.barrier())> lock(globalResources.taskShedular.barrier());
		auto pos = find_if(mSubPasses.begin(), mSubPasses.end(), [subPass](const SubPass& sub) {return subPass == &sub; });
		commandListsPerFrame -= pos->commandListsPerFrame;
		auto index = std::distance(mSubPasses.begin(), pos);

		auto nearlyEnd = (mSubPasses.end() - 1);
		if (pos != nearlyEnd) { swap(*pos, *(mSubPasses.end() - 1)); }
		mSubPasses.pop_back();

		auto end = subPasses.end();
		for (auto start = subPasses.begin(); start != end; ++start)
		{
			start->removeSubPass(globalResources, index);
		}
	}

	bool isInView() const
	{
		for (auto& subPass : mSubPasses)
		{
			if (subPass.isInView())
			{
				return true;
			}
		}
		return false;
	}

	class ThreadLocal
	{
		ResizingArray<typename RenderSubPass_t::ThreadLocal> mSubPasses;
	public:
		ThreadLocal(GraphicsEngine& graphicsEngine) {}

		using SubPass = typename RenderSubPass_t::ThreadLocal;

		Range<typename ResizingArray<SubPass>::iterator> subPasses()
		{
			return { mSubPasses.begin(), mSubPasses.end() };
		}

		void update1After(GraphicsEngine& graphicsEngine, RenderSubPassGroup<RenderSubPass_t>& renderSubPassGruop,
			ID3D12RootSignature* rootSignature)
		{
			auto subPass = renderSubPassGruop.mSubPasses.begin();
			for (auto& subPassLocal : mSubPasses)
			{
				subPassLocal.update1After(graphicsEngine, *subPass, rootSignature);
				++subPass;
			}
		}

		void update1AfterFirstThread(GraphicsEngine& graphicsEngine, RenderSubPassGroup<RenderSubPass_t>& renderSubPassGroup,
			ID3D12RootSignature* rootSignature, uint32_t barrierCount, const D3D12_RESOURCE_BARRIER* barriers)
		{
			auto subPass = renderSubPassGroup.mSubPasses.begin();
			auto subPassLocal = mSubPasses.begin();
			subPassLocal->update1AfterFirstThread(graphicsEngine, *subPass, rootSignature, barrierCount, barriers);
			++subPassLocal;
			++subPass;
			auto end = mSubPasses.end();
			for (; subPassLocal != end; ++subPassLocal)
			{
				subPassLocal->update1AfterFirstThread(graphicsEngine, *subPass, rootSignature);
				++subPass;
			}
		}

		void update2(ID3D12CommandList**& commandLists, unsigned int numThreads)
		{
			for (auto& subPassLocal : mSubPasses)
			{
				subPassLocal.update2(commandLists, numThreads);
			}
		}

		template<class ThreadResources, class GlobalResources>
		void update2LastThread(ID3D12CommandList**& commandLists, unsigned int numThreads, RenderSubPassGroup<RenderSubPass_t>& renderSubPassGroup,
			ThreadResources& threadResources, GlobalResources& globalResources)
		{
			const auto end = mSubPasses.end();
			auto subPass = renderSubPassGroup.mSubPasses.begin();
			for (auto subPassLocal = mSubPasses.begin(); subPassLocal != end; ++subPassLocal, ++subPass)
			{
				subPassLocal->update2LastThread(commandLists, numThreads, *subPass, threadResources, globalResources);
			}
		}

		ID3D12GraphicsCommandList* lastCommandList()
		{
			auto lastSubPass = mSubPasses.end() - 1u;
			return lastSubPass->lastCommandList();
		}

		void addSubPass(GraphicsEngine& graphicsEngine)
		{
			mSubPasses.emplace_back(graphicsEngine);
		}

		void removeSubPass(std::size_t index)
		{
			using std::swap;
			swap(mSubPasses[index], *(mSubPasses.end() - 1));
			mSubPasses.pop_back();
		}
	};
};