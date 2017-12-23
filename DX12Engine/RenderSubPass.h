#pragma once
#include "D3D12CommandAllocator.h"
#include "frameBufferCount.h"
#include "D3D12GraphicsCommandList.h"
#include "for_each.h"
#include "Frustum.h"
#include <vector>
#include <array>
#include "SharedResources.h"
#include "BaseExecutor.h"
#include "Iterable.h"
#include "../Array/ResizingArray.h"

//use std::tuple<std::integral_constant<unsigned int, value>, ...> for dependencies
//use std::tuple<std::integral_constant<D3D12_RESOURCE_STATES, value>, ...> for dependencyStates
template<class Camera_t, D3D12_RESOURCE_STATES state1, class Dependencies_t, class DependencyStates_t, unsigned int commandListsPerFrame1>
class RenderSubPass
{
	std::vector<Camera_t*> mCameras;
public:
	using Camera = Camera_t;
	using CameraIterator = typename std::vector<Camera*>::iterator;
	using ConstCameraIterator = typename std::vector<Camera*>::const_iterator;
	using Dependencies = Dependencies_t;
	constexpr static auto state = state1;
	using DependencyStates = DependencyStates_t;
	constexpr static auto commandListsPerFrame = commandListsPerFrame1;

	RenderSubPass() noexcept {}
	RenderSubPass(RenderSubPass<Camera_t, state1, Dependencies_t, DependencyStates_t, commandListsPerFrame1>&& other) noexcept : mCameras(std::move(other.mCameras)) {}

	void operator=(RenderSubPass<Camera_t, state1, Dependencies_t, DependencyStates_t, commandListsPerFrame1>&& other) noexcept
	{
		mCameras = std::move(other.mCameras);
	}
	
	Iterable<CameraIterator> cameras() noexcept
	{
		return { mCameras.begin(), mCameras.end()};
	}

	Iterable<ConstCameraIterator> cameras() const noexcept
	{
		return { mCameras.begin(), mCameras.end() };
	}

	unsigned int cameraCount() const noexcept
	{
		return (unsigned int)mCameras.size();
	}

	template<class RenderPass>
	void addCamera(BaseExecutor* const executor, RenderPass& renderPass, Camera* const camera)
	{
		std::lock_guard<decltype(executor->sharedResources->syncMutex)> lock(executor->sharedResources->syncMutex);
		mCameras.push_back(camera);
		renderPass.updateBarrierCount();
	}

	void removeCamera(BaseExecutor* const executor, Camera* const camera) noexcept
	{
		std::lock_guard<decltype(executor->sharedResources->syncMutex)> lock(executor->sharedResources->syncMutex);
		auto cam = std::find(mCameras.begin(), mCameras.end(), camera);
		std::swap(*cam, *(mCameras.end() - 1u));
		mCameras.pop_back();
	}

	bool isInView(const BaseExecutor* executor) const noexcept
	{
		for (auto& camera : mCameras)
		{
			if (camera->isInView(executor))
			{
				return true;
			}
		}
		return false;
	}

	class ThreadLocal
	{
		struct PerFrameData
		{
			std::array<D3D12CommandAllocator, commandListsPerFrame> commandAllocators;
			std::array<D3D12GraphicsCommandList, commandListsPerFrame> commandLists;
		};
		std::array<PerFrameData, frameBufferCount> perFrameDatas;

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
			ID3D12GraphicsCommandList** commandList = &currentData->commandLists[0u].get();
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
		ThreadLocal(BaseExecutor* const executor)
		{
			auto sharedResources = executor->sharedResources;
			auto frameIndex = executor->sharedResources->graphicsEngine.frameIndex;
			auto i = 0u;
			for (auto& perFrameData : perFrameDatas)
			{
				auto* commandList = &perFrameData.commandLists[0u];
				for (auto& allocator : perFrameData.commandAllocators)
				{

					new(&allocator) D3D12CommandAllocator(sharedResources->graphicsEngine.graphicsDevice, D3D12_COMMAND_LIST_TYPE_DIRECT);
					new(commandList) D3D12GraphicsCommandList(sharedResources->graphicsEngine.graphicsDevice, 0u, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr);
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

		void update1After(BaseExecutor* const executor, RenderSubPass<Camera, state1, Dependencies_t, DependencyStates_t, commandListsPerFrame>& renderSubPass,
			ID3D12RootSignature* rootSignature)
		{
			auto frameIndex = executor->sharedResources->graphicsEngine.frameIndex;
			resetCommandLists(frameIndex);
			bindRootArguments(rootSignature, executor->sharedResources->graphicsEngine.mainDescriptorHeap);

			for (auto& camera : renderSubPass.cameras())
			{
				if (camera->isInView(executor))
				{
					camera->bind(executor->sharedResources, &currentData->commandLists[0u].get(),
						&currentData->commandLists.data()[currentData->commandLists.size()].get());
				}
			}
		}

		void update1AfterFirstThread(BaseExecutor* const executor, RenderSubPass<Camera, state1, Dependencies_t, DependencyStates_t, commandListsPerFrame>& renderSubPass,
			ID3D12RootSignature* rootSignature, uint32_t barrierCount, const D3D12_RESOURCE_BARRIER* barriers)
		{
			auto frameIndex = executor->sharedResources->graphicsEngine.frameIndex;
			resetCommandLists(frameIndex);
			bindRootArguments(rootSignature, executor->sharedResources->graphicsEngine.mainDescriptorHeap);

			currentData->commandLists[0u]->ResourceBarrier(barrierCount, barriers);

			for (auto& camera : renderSubPass.cameras())
			{
				if (camera->isInView(executor))
				{
					camera->bindFirstThread(executor->sharedResources, &currentData->commandLists[0u].get(),
						&currentData->commandLists.data()[currentData->commandLists.size()].get());
				}
			}
		}

		void update1AfterFirstThread(BaseExecutor* const executor, RenderSubPass<Camera, state1, Dependencies_t, DependencyStates_t, commandListsPerFrame>& renderSubPass,
			ID3D12RootSignature* rootSignature)
		{
			auto frameIndex = executor->sharedResources->graphicsEngine.frameIndex;
			resetCommandLists(frameIndex);
			bindRootArguments(rootSignature, executor->sharedResources->graphicsEngine.mainDescriptorHeap);

			for (auto& camera : renderSubPass.cameras())
			{
				if (camera->isInView(executor))
				{
					camera->bindFirstThread(executor->sharedResources, &currentData->commandLists[0u].get(),
						&currentData->commandLists.data()[currentData->commandLists.size()].get());
				}
			}
		}

		void update2(ID3D12CommandList** commandLists)
		{
			for (ID3D12GraphicsCommandList* commandList : currentData->commandLists)
			{
				auto result = commandList->Close();
				if (FAILED(result)) throw HresultException(result);
			}

			for (ID3D12GraphicsCommandList* commandList : currentData->commandLists)
			{
				*commandLists = commandList;
				++commandLists;
			}
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

template<class Camera_t, D3D12_RESOURCE_STATES state1, class Dependencies_t, class DependencyStates_t>
class RenderSubPass<Camera_t, state1, Dependencies_t, DependencyStates_t, 0u>
{
public:
	using Camera = Camera_t;
	using Dependencies = Dependencies_t;
	constexpr static auto state = state1;
	using DependencyStates = DependencyStates_t;
	constexpr static auto commandListsPerFrame = 0u;

	static bool isInView(const BaseExecutor* executor) noexcept
	{
		return true;
	}

	constexpr static unsigned int cameraCount() noexcept
	{
		return 0u;
	}

	constexpr static Iterable<Camera**> cameras() noexcept { return { nullptr, nullptr }; }

	
	class ThreadLocal
	{
	public:
		constexpr ThreadLocal(BaseExecutor* const executor) noexcept {}
		constexpr static void update2(ID3D12CommandList** commandLists) noexcept {}
		constexpr static void update1AfterFirstThread(BaseExecutor* const executor, RenderSubPass<Camera_t, state1, Dependencies_t, DependencyStates_t, 0u>& renderSubPassGroup,
			ID3D12RootSignature* rootSignature, uint32_t barrierCount, const D3D12_RESOURCE_BARRIER* barriers) noexcept {}
		constexpr static void update1After(BaseExecutor* const executor, RenderSubPass<Camera_t, state1, Dependencies_t, DependencyStates_t, 0u>& renderSubPassGruop,
			ID3D12RootSignature* rootSignature) noexcept {}
		constexpr static ID3D12GraphicsCommandList* lastCommandList() noexcept { return nullptr; }
	};
};

template<class RenderSubPass_t>
class RenderSubPassGroup
{
	ResizingArray<RenderSubPass_t> mSubPasses;
	//std::vector<RenderSubPass_t> mSubPasses;
	using SubPasses = decltype(mSubPasses);

	template<class Camera, class CameraIterator, class SubPassIterator>
	class Iterator
	{
		friend class RenderSubPassGroup<RenderSubPass_t>;
		SubPassIterator iterator;
		CameraIterator current;
		Iterator(SubPassIterator iterator, CameraIterator current) : iterator(iterator), current(current) {}
		Iterator(SubPassIterator iterator) : iterator(iterator), current(iterator->cameras().begin()) {}
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

		Camera* operator*() noexcept
		{
			return *current;
		}

		Camera* operator->() noexcept
		{
			return *current;
		}

		bool operator==(const Iterator& other) noexcept
		{
			return current == other.current;
		}

		bool operator!=(const Iterator& other) noexcept
		{
			return current != other.current;
		}
	};
public:
	using Camera = typename RenderSubPass_t::Camera;
	using SubPass = RenderSubPass_t;
	using Dependencies = typename RenderSubPass_t::Dependencies;
	constexpr static auto state = RenderSubPass_t::state;
	using DependencyStates = typename RenderSubPass_t::DependencyStates;
	unsigned int commandListsPerFrame = 0u; // RenderSubPass_t::commandListsPerFrame * subPasses.size();
	using CameraIterator = Iterator<Camera, typename RenderSubPass_t::CameraIterator, typename SubPasses::iterator>;
	using ConstCameraIterator = Iterator<const Camera, typename RenderSubPass_t::ConstCameraIterator, typename SubPasses::const_iterator>;

	Iterable<CameraIterator> cameras()
	{
		return { CameraIterator(mSubPasses.begin()), CameraIterator(mSubPasses.end()) };
	}

	Iterable<ConstCameraIterator> cameras() const
	{
		return { ConstCameraIterator(mSubPasses.begin()), ConstCameraIterator(mSubPasses.end()) };
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

	Iterable<typename SubPasses::iterator> subPasses()
	{
		return {mSubPasses};
	}

	template<class RenderPass, class SubPasses>
	SubPass& addSubPass(BaseExecutor* const executor, RenderPass& renderPass, SubPasses subPasses)
	{
		std::lock_guard<decltype(executor->sharedResources->syncMutex)> lock(executor->sharedResources->syncMutex);
		mSubPasses.emplace_back();
		auto end = subPasses.end();
		for (auto start = subPasses.begin(); start != end; ++start)
		{
			start->addSubPass(executor);
		}
		renderPass.updateBarrierCount();
		return *(mSubPasses.end() - 1u);
	}

	template<class SubPasses>
	void removeSubPass(BaseExecutor* const executor, SubPass* subPass, SubPasses subPasses)
	{
		using std::swap; using std::find;
		std::lock_guard<decltype(executor->sharedResources->syncMutex)> lock(executor->sharedResources->syncMutex);
		auto pos = find_if(mSubPasses.begin(), mSubPasses.end(), [subPass](const SubPass& sub) {return subPass == &sub; });
		auto index = std::distance(mSubPasses.begin(), pos);
		swap(*pos, *(mSubPasses.end() - 1));
		mSubPasses.pop_back();

		auto end = subPasses.end();
		for (auto start = subPasses.begin(); start != end; ++start)
		{
			start->removeSubPass(executor, index);
		}
	}

	bool isInView(const BaseExecutor* executor) const
	{
		for (auto& subPass : mSubPasses)
		{
			if (subPass.isInView(executor))
			{
				return true;
			}
		}
		return false;
	}

	class ThreadLocal
	{
		std::vector<typename RenderSubPass_t::ThreadLocal> mSubPasses;
	public:
		ThreadLocal(BaseExecutor* const executor) {}

		using SubPass = typename RenderSubPass_t::ThreadLocal;

		Iterable<typename std::vector<SubPass>::iterator> subPasses()
		{
			return { mSubPasses.begin(), mSubPasses.end() };
		}

		void update1After(BaseExecutor* const executor, RenderSubPassGroup<RenderSubPass_t>& renderSubPassGruop,
			ID3D12RootSignature* rootSignature)
		{
			auto subPass = renderSubPassGruop.mSubPasses.begin();
			for (auto& subPassLocal : mSubPasses)
			{
				subPassLocal.update1After(executor, *subPass, rootSignature);
				++subPass;
			}
		}

		void update1AfterFirstThread(BaseExecutor* const executor, RenderSubPassGroup<RenderSubPass_t>& renderSubPassGroup,
			ID3D12RootSignature* rootSignature, uint32_t barrierCount, const D3D12_RESOURCE_BARRIER* barriers)
		{
			auto subPass = renderSubPassGroup.mSubPasses.begin();
			auto subPassLocal = mSubPasses.begin();
			subPassLocal->update1AfterFirstThread(executor, *subPass, rootSignature, barrierCount, barriers);
			++subPassLocal;
			++subPass;
			auto end = mSubPasses.end();
			for (; subPassLocal != end; ++subPassLocal)
			{
				subPassLocal->update1AfterFirstThread(executor, *subPass, rootSignature);
				++subPass;
			}
		}

		void update2(ID3D12CommandList** commandLists)
		{
			for (auto& subPass : mSubPasses)
			{
				subPass.update2(commandLists);
				commandLists += RenderSubPass_t::commandListsPerFrame;
			}
		}

		ID3D12GraphicsCommandList* lastCommandList()
		{
			auto lastSubPass = --mSubPasses.end();
			return lastSubPass->lastCommandList();
		}

		void addSubPass(BaseExecutor* const executor)
		{
			mSubPasses.emplace_back(executor);
		}

		void removeSubPass(BaseExecutor* const executor, size_t index)
		{
			using std::swap;
			swap(mSubPasses[index], *(mSubPasses.end() - 1));
			mSubPasses.pop_back();
		}
	};
};