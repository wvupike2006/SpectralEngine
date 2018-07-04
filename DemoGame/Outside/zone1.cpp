#include "zone1.h"
#include <D3D12GraphicsEngine.h>
#include <HresultException.h>
#include <TextureManager.h>
#include <MeshManager.h>
#include <Vector2.h>
#include <D3D12Resource.h>
#include <d3d12.h>
#include <Mesh.h>
#include "../ThreadResources.h"
#include "../GlobalResources.h"
#include "../PipelineStateObjects.h"
#include "../TextureNames.h"
#include "../MeshNames.h"
#include <TemplateFloat.h>
#include <D3D12DescriptorHeap.h>
#include "Array.h"
#include <Vector4.h>
#include <ReflectionCamera.h>
#include <Range.h>

#include <Light.h>
#include <PointLight.h>

#include "../Models/BathModel2.h"
#include "../Models/GroundModel2.h"
#include "../Models/WallModel2.h"
#include "../Models/WaterModel2.h"
#include "../Models/IceModel2.h"
#include "../Models/FireModel.h"

namespace
{
	class HDResources
	{
		constexpr static unsigned char numMeshes = 8u;
		constexpr static unsigned char numTextures = 9u;
		constexpr static unsigned char numComponents = 2u + numTextures + numMeshes;
		constexpr static unsigned int numRenderTargetTextures = 1u;
		constexpr static unsigned int numPointLights = 4u;


		static void componentUploaded(Zone<ThreadResources, GlobalResources>& zone, ThreadResources& executor, GlobalResources& globalResources)
		{
			zone.componentUploaded(executor, numComponents);
		}
	public:
		Mesh* squareWithPos;
		Mesh* cubeWithPos;
		D3D12Resource perObjectConstantBuffers;
		uint8_t* perObjectConstantBuffersCpuAddress;
		Array<D3D12Resource, numRenderTargetTextures> renderTargetTextures;
		D3D12_RESOURCE_STATES waterRenderTargetTextureState;
		D3D12DescriptorHeap renderTargetTexturesDescriptorHeap;
		unsigned int shaderResourceViews[numRenderTargetTextures];
		Array<ReflectionCamera*, numRenderTargetTextures> reflectionCameras;
		unsigned int reflectionCameraBackBuffers[numRenderTargetTextures];

		Light light;
		PointLight pointLights[numPointLights];
		LightConstantBuffer* pointLightConstantBufferCpuAddress;
		D3D12_GPU_VIRTUAL_ADDRESS pointLightConstantBufferGpuAddress;

		GroundModel2 groundModel;
		WallModel2 wallModel;
		BathModel2 bathModel1;
		WaterModel2 waterModel;
		IceModel2 iceModel;
		FireModel<templateFloat(55.0f), templateFloat(2.0f), templateFloat(64.0f)> fireModel1;
		FireModel<templateFloat(53.0f), templateFloat(2.0f), templateFloat(64.0f)> fireModel2;

		//AudioObject3dClass sound3D1;

		HDResources(ThreadResources& threadResources, GlobalResources& globalResources, Zone<ThreadResources, GlobalResources>& zone) :
			light({ 0.1f, 0.1f, 0.1f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, -0.894427191f, 0.447213595f }),

			perObjectConstantBuffers(globalResources.graphicsEngine.graphicsDevice, []()
		{
			D3D12_HEAP_PROPERTIES heapProperties;
			heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
			heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProperties.CreationNodeMask = 1u;
			heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			heapProperties.VisibleNodeMask = 1u;
			return heapProperties;
		}(), D3D12_HEAP_FLAGS::D3D12_HEAP_FLAG_NONE, []()
		{
			D3D12_RESOURCE_DESC resourceDesc;
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
			resourceDesc.Width = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
			resourceDesc.Height = 1u;
			resourceDesc.DepthOrArraySize = 1u;
			resourceDesc.MipLevels = 1u;
			resourceDesc.Format = DXGI_FORMAT::DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc = { 1u, 0u };
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			return resourceDesc;
		}(), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr),

			renderTargetTexturesDescriptorHeap(globalResources.graphicsEngine.graphicsDevice, []()
		{
			D3D12_DESCRIPTOR_HEAP_DESC descriptorheapDesc;
			descriptorheapDesc.NumDescriptors = numRenderTargetTextures;
			descriptorheapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; //RTVs aren't shader visable
			descriptorheapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			descriptorheapDesc.NodeMask = 0u;
			return descriptorheapDesc;
		}())

		{
#ifndef NDEBUG
			renderTargetTexturesDescriptorHeap->SetName(L"Zone 1 Render Target texture descriptor heap");
#endif
			pointLights[0u] = PointLight{ Vector4{ 20.0f, 2.0f, 2.0f, 1.0f }, Vector4{ 52.0f, 5.0f, 76.0f, 1.f } };
			pointLights[1u] = PointLight{ Vector4{ 2.0f, 20.0f, 2.0f, 1.0f }, Vector4{ 76.0f, 5.0f, 76.0f, 1.f } };
			pointLights[2u] = PointLight{ Vector4{ 2.0f, 2.0f, 20.0f, 1.0f },Vector4{ 52.0f, 5.0f, 52.0f, 1.f } };
			pointLights[3u] = PointLight{ Vector4{ 20.0f, 20.0f, 20.0f, 1.0f }, Vector4{ 76.0f, 5.0f, 52.0f, 1.f } };

			D3D12_RANGE readRange{ 0u, 0u };
			HRESULT hr = perObjectConstantBuffers->Map(0u, &readRange, reinterpret_cast<void**>(&perObjectConstantBuffersCpuAddress));
			if (FAILED(hr)) throw HresultException(hr);
			auto PerObjectConstantBuffersGpuAddress = perObjectConstantBuffers->GetGPUVirtualAddress();
			uint8_t* cpuConstantBuffer = perObjectConstantBuffersCpuAddress;

			D3D12_HEAP_PROPERTIES heapProperties;
			heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
			heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			heapProperties.VisibleNodeMask = 0u;
			heapProperties.CreationNodeMask = 0u;

			D3D12_RESOURCE_DESC resourcesDesc;
			resourcesDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			resourcesDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
			resourcesDesc.Width = globalResources.window.width();
			resourcesDesc.Height = globalResources.window.height();
			resourcesDesc.DepthOrArraySize = 1u;
			resourcesDesc.MipLevels = 1u;
			resourcesDesc.Format = DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM;
			resourcesDesc.SampleDesc = { 1u, 0u };
			resourcesDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			resourcesDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

			D3D12_CLEAR_VALUE clearValue;
			clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			clearValue.Color[0] = 0.0f;
			clearValue.Color[1] = 0.0f;
			clearValue.Color[2] = 0.0f;
			clearValue.Color[3] = 1.0f;

			for (auto i = 0u; i < numRenderTargetTextures; ++i)
			{
				new(&renderTargetTextures[i]) D3D12Resource(globalResources.graphicsEngine.graphicsDevice, heapProperties, D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES, resourcesDesc,
					D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearValue);

#ifdef _DEBUG
				std::wstring name = L" zone1 render to texture ";
				name += std::to_wstring(i);
				renderTargetTextures[i]->SetName(name.c_str());
#endif // _DEBUG
			}

			{
				D3D12_RENDER_TARGET_VIEW_DESC HDRenderTargetViewDesc;
				HDRenderTargetViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				HDRenderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
				HDRenderTargetViewDesc.Texture2D.MipSlice = 0u;
				HDRenderTargetViewDesc.Texture2D.PlaneSlice = 0u;

				D3D12_SHADER_RESOURCE_VIEW_DESC HDSHaderResourceViewDesc;
				HDSHaderResourceViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				HDSHaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				HDSHaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				HDSHaderResourceViewDesc.Texture2D.MipLevels = 1u;
				HDSHaderResourceViewDesc.Texture2D.MostDetailedMip = 0u;
				HDSHaderResourceViewDesc.Texture2D.PlaneSlice = 0u;
				HDSHaderResourceViewDesc.Texture2D.ResourceMinLODClamp = 0.0f;

				auto shaderResourceViewCpuDescriptorHandle = globalResources.graphicsEngine.mainDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
				auto srvSize = globalResources.graphicsEngine.constantBufferViewAndShaderResourceViewAndUnordedAccessViewDescriptorSize;
				auto tempRenderTargetTexturesCpuDescriptorHandle = renderTargetTexturesDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

				D3D12_SHADER_RESOURCE_VIEW_DESC backBufferSrvDesc;
				backBufferSrvDesc.Format = DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM;
				backBufferSrvDesc.ViewDimension = D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_TEXTURE2D;
				backBufferSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				backBufferSrvDesc.Texture2D.MipLevels = 1u;
				backBufferSrvDesc.Texture2D.MostDetailedMip = 0u;
				backBufferSrvDesc.Texture2D.PlaneSlice = 0u;
				backBufferSrvDesc.Texture2D.ResourceMinLODClamp = 0u;

				for (auto i = 0u; i < numRenderTargetTextures; ++i)
				{
					shaderResourceViews[i] = globalResources.graphicsEngine.descriptorAllocator.allocate();
					globalResources.graphicsEngine.graphicsDevice->CreateShaderResourceView(renderTargetTextures[i], &HDSHaderResourceViewDesc, shaderResourceViewCpuDescriptorHandle + srvSize * shaderResourceViews[i]);

					globalResources.graphicsEngine.graphicsDevice->CreateRenderTargetView(renderTargetTextures[i], &HDRenderTargetViewDesc, tempRenderTargetTexturesCpuDescriptorHandle);

					reflectionCameraBackBuffers[i] = globalResources.graphicsEngine.descriptorAllocator.allocate();
					globalResources.graphicsEngine.graphicsDevice->CreateShaderResourceView(renderTargetTextures[i], &backBufferSrvDesc,
						shaderResourceViewCpuDescriptorHandle + reflectionCameraBackBuffers[i] * globalResources.graphicsEngine.constantBufferViewAndShaderResourceViewAndUnordedAccessViewDescriptorSize);

					tempRenderTargetTexturesCpuDescriptorHandle.ptr += globalResources.graphicsEngine.renderTargetViewDescriptorSize;
				}
			}

			PerObjectConstantBuffersGpuAddress += ReflectionCamera::totalConstantBufferRequired;
			cpuConstantBuffer += ReflectionCamera::totalConstantBufferRequired;

			new(&bathModel1) BathModel2(PerObjectConstantBuffersGpuAddress, cpuConstantBuffer);

			new(&groundModel) GroundModel2(PerObjectConstantBuffersGpuAddress, cpuConstantBuffer);

			new(&wallModel) WallModel2(PerObjectConstantBuffersGpuAddress, cpuConstantBuffer);

			new(&waterModel) WaterModel2(PerObjectConstantBuffersGpuAddress, cpuConstantBuffer, shaderResourceViews[0],
				globalResources.warpTextureDescriptorIndex);

			new(&iceModel) IceModel2(PerObjectConstantBuffersGpuAddress, cpuConstantBuffer, globalResources.warpTextureDescriptorIndex);

			new(&fireModel1) FireModel<templateFloat(55.0f), templateFloat(2.0f), templateFloat(64.0f)>(PerObjectConstantBuffersGpuAddress,
				cpuConstantBuffer);

			new(&fireModel2) FireModel<templateFloat(53.0f), templateFloat(2.0f), templateFloat(64.0f)>(PerObjectConstantBuffersGpuAddress,
				cpuConstantBuffer);

			TextureManager& textureManager = globalResources.textureManager;
			textureManager.loadTexture(threadResources, globalResources, TextureNames::ground01, { &zone, [](void* context, void* tr, void* gr, unsigned int textureID)
			{
				ThreadResources& threadResources = *reinterpret_cast<ThreadResources*>(tr);
				GlobalResources& globalResources = *reinterpret_cast<GlobalResources*>(gr);
				auto& zone = *reinterpret_cast<Zone<ThreadResources, GlobalResources>*>(context);
				auto resources = ((HDResources*)zone.newData);
				resources->groundModel.setDiffuseTexture(textureID, resources->perObjectConstantBuffersCpuAddress, resources->perObjectConstantBuffers->GetGPUVirtualAddress());
				componentUploaded(zone, threadResources, globalResources);
			} });
			textureManager.loadTexture(threadResources, globalResources, TextureNames::wall01, { &zone, [](void* context, void* tr, void* gr, unsigned int textureID)
			{
				ThreadResources& threadResources = *reinterpret_cast<ThreadResources*>(tr);
				GlobalResources& globalResources = *reinterpret_cast<GlobalResources*>(gr);
				auto& zone = *reinterpret_cast<Zone<ThreadResources, GlobalResources>*>(context);
				auto resources = ((HDResources*)zone.newData);
				resources->wallModel.setDiffuseTexture(textureID, resources->perObjectConstantBuffersCpuAddress, resources->perObjectConstantBuffers->GetGPUVirtualAddress());
				componentUploaded(zone, threadResources, globalResources);
			} });
			textureManager.loadTexture(threadResources, globalResources, TextureNames::marble01, { &zone, [](void* context, void* tr, void* gr, unsigned int textureID)
			{
				ThreadResources& threadResources = *reinterpret_cast<ThreadResources*>(tr);
				GlobalResources& globalResources = *reinterpret_cast<GlobalResources*>(gr);
				auto& zone = *reinterpret_cast<Zone<ThreadResources, GlobalResources>*>(context);
				auto resources = ((HDResources*)zone.newData);
				resources->bathModel1.setDiffuseTexture(textureID, resources->perObjectConstantBuffersCpuAddress, resources->perObjectConstantBuffers->GetGPUVirtualAddress());
				componentUploaded(zone, threadResources, globalResources);
			} });
			textureManager.loadTexture(threadResources, globalResources, TextureNames::water01, { &zone, [](void* context, void* tr, void* gr, unsigned int textureID)
			{
				ThreadResources& threadResources = *reinterpret_cast<ThreadResources*>(tr);
				GlobalResources& globalResources = *reinterpret_cast<GlobalResources*>(gr);
				auto& zone = *reinterpret_cast<Zone<ThreadResources, GlobalResources>*>(context);
				auto resources = ((HDResources*)zone.newData);
				resources->waterModel.setNormalTexture(textureID, resources->perObjectConstantBuffersCpuAddress, resources->perObjectConstantBuffers->GetGPUVirtualAddress());
				componentUploaded(zone, threadResources, globalResources);
			} });
			textureManager.loadTexture(threadResources, globalResources, TextureNames::ice01, { &zone, [](void* context, void* tr, void* gr, unsigned int textureID)
			{
				ThreadResources& threadResources = *reinterpret_cast<ThreadResources*>(tr);
				GlobalResources& globalResources = *reinterpret_cast<GlobalResources*>(gr);
				auto& zone = *reinterpret_cast<Zone<ThreadResources, GlobalResources>*>(context);
				auto resources = ((HDResources*)zone.newData);
				resources->iceModel.setDiffuseTexture(textureID, resources->perObjectConstantBuffersCpuAddress, resources->perObjectConstantBuffers->GetGPUVirtualAddress());
				componentUploaded(zone, threadResources, globalResources);
			} });
			textureManager.loadTexture(threadResources, globalResources, TextureNames::icebump01, { &zone, [](void* context, void* tr, void* gr, unsigned int textureID)
			{
				ThreadResources& threadResources = *reinterpret_cast<ThreadResources*>(tr);
				GlobalResources& globalResources = *reinterpret_cast<GlobalResources*>(gr);
				auto& zone = *reinterpret_cast<Zone<ThreadResources, GlobalResources>*>(context);
				auto resources = ((HDResources*)zone.newData);
				resources->iceModel.setNormalTexture(textureID, resources->perObjectConstantBuffersCpuAddress, resources->perObjectConstantBuffers->GetGPUVirtualAddress());
				componentUploaded(zone, threadResources, globalResources);
			} });
			textureManager.loadTexture(threadResources, globalResources, TextureNames::firenoise01, { &zone, [](void* context, void* tr, void* gr, unsigned int textureID)
			{
				ThreadResources& threadResources = *reinterpret_cast<ThreadResources*>(tr);
				GlobalResources& globalResources = *reinterpret_cast<GlobalResources*>(gr);
				auto& zone = *reinterpret_cast<Zone<ThreadResources, GlobalResources>*>(context);
				auto resources = ((HDResources*)zone.newData);
				resources->fireModel1.setNormalTexture(textureID, resources->perObjectConstantBuffersCpuAddress, resources->perObjectConstantBuffers->GetGPUVirtualAddress());
				resources->fireModel2.setNormalTexture(textureID, resources->perObjectConstantBuffersCpuAddress, resources->perObjectConstantBuffers->GetGPUVirtualAddress());
				componentUploaded(zone, threadResources, globalResources);
			} });
			textureManager.loadTexture(threadResources, globalResources, TextureNames::fire01, { &zone, [](void* context, void* tr, void* gr, unsigned int textureID)
			{
				ThreadResources& threadResources = *reinterpret_cast<ThreadResources*>(tr);
				GlobalResources& globalResources = *reinterpret_cast<GlobalResources*>(gr);
				auto& zone = *reinterpret_cast<Zone<ThreadResources, GlobalResources>*>(context);
				auto resources = ((HDResources*)zone.newData);
				resources->fireModel1.setDiffuseTexture(textureID, resources->perObjectConstantBuffersCpuAddress, resources->perObjectConstantBuffers->GetGPUVirtualAddress());
				resources->fireModel2.setDiffuseTexture(textureID, resources->perObjectConstantBuffersCpuAddress, resources->perObjectConstantBuffers->GetGPUVirtualAddress());
				componentUploaded(zone, threadResources, globalResources);
			} });
			textureManager.loadTexture(threadResources, globalResources, TextureNames::firealpha01, { &zone, [](void* context, void* tr, void* gr, unsigned int textureID)
			{
				ThreadResources& threadResources = *reinterpret_cast<ThreadResources*>(tr);
				GlobalResources& globalResources = *reinterpret_cast<GlobalResources*>(gr);
				auto& zone = *reinterpret_cast<Zone<ThreadResources, GlobalResources>*>(context);
				auto resources = ((HDResources*)zone.newData);
				resources->fireModel1.setAlphaTexture(textureID, resources->perObjectConstantBuffersCpuAddress, resources->perObjectConstantBuffers->GetGPUVirtualAddress());
				resources->fireModel2.setAlphaTexture(textureID, resources->perObjectConstantBuffersCpuAddress, resources->perObjectConstantBuffers->GetGPUVirtualAddress());
				componentUploaded(zone, threadResources, globalResources);
			} });

			MeshManager& meshManager = globalResources.meshManager;
			meshManager.loadMeshWithPositionTextureNormal(MeshNames::bath, &zone, threadResources, globalResources, [](void* context, void* tr, void* gr, Mesh* mesh)
			{
				ThreadResources& threadResources = *reinterpret_cast<ThreadResources*>(tr);
				GlobalResources& globalResources = *reinterpret_cast<GlobalResources*>(gr);
				auto& zone = *reinterpret_cast<Zone<ThreadResources, GlobalResources>*>(context);
				auto resources = ((HDResources*)zone.newData);
				resources->bathModel1.mesh = mesh;
				componentUploaded(zone, threadResources, globalResources);
			});
			meshManager.loadMeshWithPositionTextureNormal(MeshNames::plane1, &zone, threadResources, globalResources, [](void* context, void* tr, void* gr, Mesh* mesh)
			{
				ThreadResources& threadResources = *reinterpret_cast<ThreadResources*>(tr);
				GlobalResources& globalResources = *reinterpret_cast<GlobalResources*>(gr);
				auto& zone = *reinterpret_cast<Zone<ThreadResources, GlobalResources>*>(context);
				auto resources = ((HDResources*)zone.newData);
				resources->groundModel.mesh = mesh;
				componentUploaded(zone, threadResources, globalResources);
			});
			meshManager.loadMeshWithPositionTextureNormal(MeshNames::wall, &zone, threadResources, globalResources, [](void* context, void* tr, void* gr, Mesh* mesh)
			{
				ThreadResources& threadResources = *reinterpret_cast<ThreadResources*>(tr);
				GlobalResources& globalResources = *reinterpret_cast<GlobalResources*>(gr);
				auto& zone = *reinterpret_cast<Zone<ThreadResources, GlobalResources>*>(context);
				auto resources = ((HDResources*)zone.newData);
				resources->wallModel.mesh = mesh;
				componentUploaded(zone, threadResources, globalResources);
			});
			meshManager.loadMeshWithPositionTexture(MeshNames::water, &zone, threadResources, globalResources, [](void* context, void* tr, void* gr, Mesh* mesh)
			{
				ThreadResources& threadResources = *reinterpret_cast<ThreadResources*>(tr);
				GlobalResources& globalResources = *reinterpret_cast<GlobalResources*>(gr);
				auto& zone = *reinterpret_cast<Zone<ThreadResources, GlobalResources>*>(context);
				auto resources = ((HDResources*)zone.newData);
				resources->waterModel.mesh = mesh;
				componentUploaded(zone, threadResources, globalResources);
			});
			meshManager.loadMeshWithPositionTexture(MeshNames::cube, &zone, threadResources, globalResources, [](void* context, void* tr, void* gr, Mesh* mesh)
			{
				ThreadResources& threadResources = *reinterpret_cast<ThreadResources*>(tr);
				GlobalResources& globalResources = *reinterpret_cast<GlobalResources*>(gr);
				auto& zone = *reinterpret_cast<Zone<ThreadResources, GlobalResources>*>(context);
				auto resources = ((HDResources*)zone.newData);
				resources->iceModel.mesh = mesh;
				componentUploaded(zone, threadResources, globalResources);
			});
			meshManager.loadMeshWithPositionTexture(MeshNames::square, &zone, threadResources, globalResources, [](void* context, void* tr, void* gr, Mesh* mesh)
			{
				ThreadResources& threadResources = *reinterpret_cast<ThreadResources*>(tr);
				GlobalResources& globalResources = *reinterpret_cast<GlobalResources*>(gr);
				auto& zone = *reinterpret_cast<Zone<ThreadResources, GlobalResources>*>(context);
				auto resources = ((HDResources*)zone.newData);
				resources->fireModel1.mesh = mesh;
				resources->fireModel2.mesh = mesh;
				componentUploaded(zone, threadResources, globalResources);
			});
			meshManager.loadMeshWithPosition(MeshNames::aabb, &zone, threadResources, globalResources, [](void* context, void* tr, void* gr, Mesh* mesh)
			{
				ThreadResources& threadResources = *reinterpret_cast<ThreadResources*>(tr);
				GlobalResources& globalResources = *reinterpret_cast<GlobalResources*>(gr);
				auto& zone = *reinterpret_cast<Zone<ThreadResources, GlobalResources>*>(context);
				auto resources = ((HDResources*)zone.newData);
				resources->cubeWithPos = mesh;
				componentUploaded(zone, threadResources, globalResources);
			});
			meshManager.loadMeshWithPosition(MeshNames::squareWithPos, &zone, threadResources, globalResources, [](void* context, void* tr, void* gr, Mesh* mesh)
			{
				ThreadResources& threadResources = *reinterpret_cast<ThreadResources*>(tr);
				GlobalResources& globalResources = *reinterpret_cast<GlobalResources*>(gr);
				auto& zone = *reinterpret_cast<Zone<ThreadResources, GlobalResources>*>(context);
				auto resources = ((HDResources*)zone.newData);
				resources->squareWithPos = mesh;
				componentUploaded(zone, threadResources, globalResources);
			});
			
			threadResources.taskShedular.update1NextQueue().concurrentPush({ &zone, [](void* context, ThreadResources& threadResources, GlobalResources& globalResources)
			{
				auto& zone = *reinterpret_cast<Zone<ThreadResources, GlobalResources>*>(context);
				const auto resource = reinterpret_cast<HDResources*>(zone.newData);

				auto tempRenderTargetTexturesCpuDescriptorHandle = resource->renderTargetTexturesDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
				auto depthStencilHandle = globalResources.graphicsEngine.depthStencilDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
				Transform transform = globalResources.mainCamera().transform().reflection(resource->waterModel.reflectionHeight());
				uint8_t* cpuConstantBuffer = resource->perObjectConstantBuffersCpuAddress;
				D3D12_GPU_VIRTUAL_ADDRESS PerObjectConstantBuffersGpuAddress = resource->perObjectConstantBuffers->GetGPUVirtualAddress();

				for (auto i = 0u; i < numRenderTargetTextures; ++i)
				{
					uint32_t backBuffers[frameBufferCount];
					for (auto j = 0u; j < frameBufferCount; ++j)
					{
						backBuffers[j] = resource->reflectionCameraBackBuffers[i];
					}

					resource->reflectionCameras[i] = &globalResources.renderPass.renderToTextureSubPass().addCamera(globalResources, globalResources.renderPass, ReflectionCamera(
						resource->renderTargetTextures[i], tempRenderTargetTexturesCpuDescriptorHandle, depthStencilHandle, globalResources.window.width(), globalResources.window.height(), PerObjectConstantBuffersGpuAddress,
						cpuConstantBuffer, 0.25f * 3.141f, transform, backBuffers));

					tempRenderTargetTexturesCpuDescriptorHandle.ptr += globalResources.graphicsEngine.renderTargetViewDescriptorSize;
				}

				componentUploaded(zone, threadResources, globalResources);
			} });

			constexpr uint64_t pointLightConstantBufferAlignedSize = (sizeof(LightConstantBuffer) + (uint64_t)D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - (uint64_t)1u) & 
				~((uint64_t)D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - (uint64_t)1u);
			pointLightConstantBufferCpuAddress = reinterpret_cast<LightConstantBuffer*>(cpuConstantBuffer);
			cpuConstantBuffer += frameBufferCount * pointLightConstantBufferAlignedSize;

			pointLightConstantBufferGpuAddress = PerObjectConstantBuffersGpuAddress;
			PerObjectConstantBuffersGpuAddress += frameBufferCount * pointLightConstantBufferAlignedSize;
			//sound3D1.Play(DSBPLAY_LOOPING);
		}

		~HDResources() {}

		static void create(void* context, ThreadResources& threadResources, GlobalResources& globalResources)
		{
			Zone<ThreadResources, GlobalResources>& zone = *reinterpret_cast<Zone<ThreadResources, GlobalResources>*>(context);
			zone.newData = malloc(sizeof(HDResources));
			new(zone.newData) HDResources(threadResources, globalResources, zone);
			componentUploaded(zone, threadResources, globalResources);
		}

		void update1(ThreadResources& threadResources, GlobalResources& globalResources)
		{
			const auto frameIndex = globalResources.graphicsEngine.frameIndex;
			const auto& cameraPos = globalResources.mainCamera().position();
			const auto frameTime = globalResources.timer.frameTime();
			waterModel.update(globalResources.graphicsEngine, globalResources.timer);
			fireModel1.update(frameIndex, cameraPos, frameTime);
			fireModel2.update(frameIndex, cameraPos, frameTime);
			for (auto& camera : reflectionCameras)
			{
				camera->update(globalResources.graphicsEngine.frameIndex, globalResources.mainCamera().transform().reflection(waterModel.reflectionHeight()).toMatrix());
			}

			auto rotationMatrix = DirectX::XMMatrixTranslation(-64.0f, -5.0f, -64.0f) * DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), frameTime) * DirectX::XMMatrixTranslation(64.0f, 5.0f, 64.0f);

			constexpr uint64_t pointLightConstantBufferAlignedSize = (sizeof(LightConstantBuffer) + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1ull) & ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1ull);
			auto pointLightConstantBuffer = reinterpret_cast<LightConstantBuffer*>(reinterpret_cast<uint8_t*>(pointLightConstantBufferCpuAddress) + frameIndex * pointLightConstantBufferAlignedSize);

			pointLightConstantBuffer->ambientLight = light.ambientLight;
			pointLightConstantBuffer->directionalLight = light.directionalLight;
			pointLightConstantBuffer->lightDirection = light.direction;

			pointLightConstantBuffer->pointLightCount = numPointLights;
			for (auto i = 0u; i < numPointLights; ++i)
			{
				auto temp = DirectX::XMVector4Transform(DirectX::XMVectorSet(pointLights[i].position.x(), pointLights[i].position.y(),
					pointLights[i].position.z(), pointLights[i].position.w()), rotationMatrix);
				DirectX::XMFLOAT4 temp2;
				DirectX::XMStoreFloat4(&temp2, temp);
				pointLights[i].position.x() = temp2.x; pointLights[i].position.y() = temp2.y; pointLights[i].position.z() = temp2.z; pointLights[i].position.w() = temp2.w;
				pointLightConstantBuffer->pointLights[i] = pointLights[i];
			}
		}

		void renderToTexture(ThreadResources& threadResources, GlobalResources& globalResources)
		{
			const auto frameIndex = globalResources.graphicsEngine.frameIndex;

			auto depthStencilHandle = globalResources.graphicsEngine.depthStencilDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			if (waterModel.isInView(globalResources.mainCamera().frustum()))
			{
				auto& camera = *reflectionCameras[0];
				const auto& frustum = camera.frustum();
				const auto commandList = threadResources.renderPass.renderToTextureSubPass().firstCommandList();
				auto warpTextureCpuDescriptorHandle = globalResources.warpTextureCpuDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
				auto backBufferRenderTargetCpuHandle = camera.getRenderTargetView();

				commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

				constexpr uint64_t pointLightConstantBufferAlignedSize = (sizeof(LightConstantBuffer) + (size_t)D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - (size_t)1u) & ~((size_t)D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - (size_t)1u);
				commandList->SetGraphicsRootConstantBufferView(1u, pointLightConstantBufferGpuAddress + frameIndex * pointLightConstantBufferAlignedSize);

				D3D12_RESOURCE_BARRIER copyStartBarriers[2u];
				copyStartBarriers[0u].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				copyStartBarriers[0u].Flags = D3D12_RESOURCE_BARRIER_FLAGS::D3D12_RESOURCE_BARRIER_FLAG_NONE;
				copyStartBarriers[0u].Transition.pResource = globalResources.warpTexture;
				copyStartBarriers[0u].Transition.StateBefore = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				copyStartBarriers[0u].Transition.StateAfter = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RENDER_TARGET;
				copyStartBarriers[0u].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

				copyStartBarriers[1u].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				copyStartBarriers[1u].Flags = D3D12_RESOURCE_BARRIER_FLAGS::D3D12_RESOURCE_BARRIER_FLAG_NONE;
				copyStartBarriers[1u].Transition.pResource = camera.getImage();
				copyStartBarriers[1u].Transition.StateBefore = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RENDER_TARGET;
				copyStartBarriers[1u].Transition.StateAfter = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				copyStartBarriers[1u].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

				D3D12_RESOURCE_BARRIER copyEndBarriers[2u];
				copyEndBarriers[0u].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				copyEndBarriers[0u].Flags = D3D12_RESOURCE_BARRIER_FLAGS::D3D12_RESOURCE_BARRIER_FLAG_NONE;
				copyEndBarriers[0u].Transition.pResource = globalResources.warpTexture;
				copyEndBarriers[0u].Transition.StateBefore = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RENDER_TARGET;
				copyEndBarriers[0u].Transition.StateAfter = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				copyEndBarriers[0u].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

				copyEndBarriers[1u].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				copyEndBarriers[1u].Flags = D3D12_RESOURCE_BARRIER_FLAGS::D3D12_RESOURCE_BARRIER_FLAG_NONE;
				copyEndBarriers[1u].Transition.pResource = copyStartBarriers[1u].Transition.pResource;
				copyEndBarriers[1u].Transition.StateBefore = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				copyEndBarriers[1u].Transition.StateAfter = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RENDER_TARGET;
				copyEndBarriers[1u].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

				if (wallModel.isInView(frustum))
				{
					commandList->SetPipelineState(globalResources.pipelineStateObjects.directionalLight);
					commandList->SetGraphicsRootConstantBufferView(2u, wallModel.vsBufferGpu());
					commandList->SetGraphicsRootConstantBufferView(3u, wallModel.psBufferGpu());
					commandList->IASetVertexBuffers(0u, 1u, &wallModel.mesh->vertexBufferView);
					commandList->IASetIndexBuffer(&wallModel.mesh->indexBufferView);
					commandList->DrawIndexedInstanced(wallModel.mesh->indexCount, 1u, 0u, 0, 0u);
				}
				if (iceModel.isInView(frustum))
				{
					commandList->ResourceBarrier(2u, copyStartBarriers);
					commandList->OMSetRenderTargets(1u, &warpTextureCpuDescriptorHandle, TRUE, &depthStencilHandle);
					commandList->SetPipelineState(globalResources.pipelineStateObjects.copy);
					commandList->SetGraphicsRootConstantBufferView(2u, iceModel.vsAabbGpu());
					commandList->IASetVertexBuffers(0u, 1u, &cubeWithPos->vertexBufferView);
					commandList->IASetIndexBuffer(&cubeWithPos->indexBufferView);
					commandList->DrawIndexedInstanced(cubeWithPos->indexCount, 1u, 0u, 0, 0u);

					commandList->ResourceBarrier(2u, copyEndBarriers);
					commandList->OMSetRenderTargets(1u, &backBufferRenderTargetCpuHandle, TRUE, &depthStencilHandle);
					commandList->SetPipelineState(globalResources.pipelineStateObjects.glass);
					commandList->SetGraphicsRootConstantBufferView(2u, iceModel.vsConstantBufferGPU(frameIndex));
					commandList->SetGraphicsRootConstantBufferView(3u, iceModel.psConstantBufferGPU());
					commandList->IASetVertexBuffers(0u, 1u, &iceModel.mesh->vertexBufferView);
					commandList->IASetIndexBuffer(&iceModel.mesh->indexBufferView);
					commandList->DrawIndexedInstanced(iceModel.mesh->indexCount, 1u, 0u, 0, 0u);
				}
			}
		}

		void update2(ThreadResources& threadResources, GlobalResources& globalResources)
		{
			renderToTexture(threadResources, globalResources);

			const auto frameIndex = globalResources.graphicsEngine.frameIndex;
			const auto opaqueDirectCommandList = threadResources.renderPass.colorSubPass().opaqueCommandList();
			const auto transparantCommandList = threadResources.renderPass.colorSubPass().transparentCommandList();
			auto depthStencilHandle = globalResources.graphicsEngine.depthStencilDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			auto backBufferRenderTargetCpuHandle = globalResources.mainCamera().getRenderTargetView(frameIndex);
			auto warpTextureCpuDescriptorHandle = globalResources.warpTextureCpuDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

			D3D12_RESOURCE_BARRIER copyStartBarriers[2u];
			copyStartBarriers[0u].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			copyStartBarriers[0u].Flags = D3D12_RESOURCE_BARRIER_FLAGS::D3D12_RESOURCE_BARRIER_FLAG_NONE;
			copyStartBarriers[0u].Transition.pResource = globalResources.warpTexture;
			copyStartBarriers[0u].Transition.StateBefore = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			copyStartBarriers[0u].Transition.StateAfter = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RENDER_TARGET;
			copyStartBarriers[0u].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

			copyStartBarriers[1u].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			copyStartBarriers[1u].Flags = D3D12_RESOURCE_BARRIER_FLAGS::D3D12_RESOURCE_BARRIER_FLAG_NONE;
			copyStartBarriers[1u].Transition.pResource = globalResources.window.getBuffer(frameIndex);
			copyStartBarriers[1u].Transition.StateBefore = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RENDER_TARGET;
			copyStartBarriers[1u].Transition.StateAfter = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			copyStartBarriers[1u].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

			D3D12_RESOURCE_BARRIER copyEndBarriers[2u];
			copyEndBarriers[0u].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			copyEndBarriers[0u].Flags = D3D12_RESOURCE_BARRIER_FLAGS::D3D12_RESOURCE_BARRIER_FLAG_NONE;
			copyEndBarriers[0u].Transition.pResource = globalResources.warpTexture;
			copyEndBarriers[0u].Transition.StateBefore = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RENDER_TARGET;
			copyEndBarriers[0u].Transition.StateAfter = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			copyEndBarriers[0u].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

			copyEndBarriers[1u].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			copyEndBarriers[1u].Flags = D3D12_RESOURCE_BARRIER_FLAGS::D3D12_RESOURCE_BARRIER_FLAG_NONE;
			copyEndBarriers[1u].Transition.pResource = copyStartBarriers[1u].Transition.pResource;
			copyEndBarriers[1u].Transition.StateBefore = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			copyEndBarriers[1u].Transition.StateAfter = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RENDER_TARGET;
			copyEndBarriers[1u].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

			opaqueDirectCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			constexpr uint64_t pointLightConstantBufferAlignedSize = (sizeof(LightConstantBuffer) + (uint64_t)D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - (uint64_t)1u) & ~((uint64_t)D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - (uint64_t)1u);
			opaqueDirectCommandList->SetGraphicsRootConstantBufferView(1u, pointLightConstantBufferGpuAddress + frameIndex * pointLightConstantBufferAlignedSize);
			const auto& frustum = globalResources.mainCamera().frustum();

			if (groundModel.isInView(frustum))
			{
				opaqueDirectCommandList->SetPipelineState(globalResources.pipelineStateObjects.pointLight);

				opaqueDirectCommandList->SetGraphicsRootConstantBufferView(2u, groundModel.vsBufferGpu());
				opaqueDirectCommandList->SetGraphicsRootConstantBufferView(3u, groundModel.psBufferGpu());
				opaqueDirectCommandList->IASetVertexBuffers(0u, 1u, &groundModel.mesh->vertexBufferView);
				opaqueDirectCommandList->IASetIndexBuffer(&groundModel.mesh->indexBufferView);
				opaqueDirectCommandList->DrawIndexedInstanced(groundModel.mesh->indexCount, 1u, 0u, 0, 0u);
			}

			opaqueDirectCommandList->SetPipelineState(globalResources.pipelineStateObjects.directionalLight);
			if (wallModel.isInView(frustum))
			{
				opaqueDirectCommandList->SetGraphicsRootConstantBufferView(2u, wallModel.vsBufferGpu());
				opaqueDirectCommandList->SetGraphicsRootConstantBufferView(3u, wallModel.psBufferGpu());
				opaqueDirectCommandList->IASetVertexBuffers(0u, 1u, &wallModel.mesh->vertexBufferView);
				opaqueDirectCommandList->IASetIndexBuffer(&wallModel.mesh->indexBufferView);
				opaqueDirectCommandList->DrawIndexedInstanced(wallModel.mesh->indexCount, 1u, 0u, 0, 0u);
			}

			if (bathModel1.isInView(frustum))
			{
				opaqueDirectCommandList->SetGraphicsRootConstantBufferView(2u, bathModel1.vsBufferGpu());
				opaqueDirectCommandList->SetGraphicsRootConstantBufferView(3u, bathModel1.psBufferGpu());
				opaqueDirectCommandList->IASetVertexBuffers(0u, 1u, &bathModel1.mesh->vertexBufferView);
				opaqueDirectCommandList->IASetIndexBuffer(&bathModel1.mesh->indexBufferView);
				opaqueDirectCommandList->DrawIndexedInstanced(bathModel1.mesh->indexCount, 1u, 0u, 0, 0u);
			}

			transparantCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			if (waterModel.isInView(frustum))
			{
				transparantCommandList->ResourceBarrier(2u, copyStartBarriers);
				transparantCommandList->OMSetRenderTargets(1u, &warpTextureCpuDescriptorHandle, TRUE, &depthStencilHandle);
				transparantCommandList->SetPipelineState(globalResources.pipelineStateObjects.copy);
				transparantCommandList->SetGraphicsRootConstantBufferView(2u, waterModel.vsAabbGpu());
				transparantCommandList->IASetVertexBuffers(0u, 1u, &squareWithPos->vertexBufferView);
				transparantCommandList->IASetIndexBuffer(&squareWithPos->indexBufferView);
				transparantCommandList->DrawIndexedInstanced(squareWithPos->indexCount, 1u, 0u, 0, 0u);

				transparantCommandList->ResourceBarrier(2u, copyEndBarriers);
				transparantCommandList->OMSetRenderTargets(1u, &backBufferRenderTargetCpuHandle, TRUE, &depthStencilHandle);
				transparantCommandList->SetPipelineState(globalResources.pipelineStateObjects.waterWithReflectionTexture);
				transparantCommandList->SetGraphicsRootConstantBufferView(2u, waterModel.vsConstantBufferGPU(frameIndex));
				transparantCommandList->SetGraphicsRootConstantBufferView(3u, waterModel.psConstantBufferGPU());
				transparantCommandList->IASetVertexBuffers(0u, 1u, &waterModel.mesh->vertexBufferView);
				transparantCommandList->IASetIndexBuffer(&waterModel.mesh->indexBufferView);
				transparantCommandList->DrawIndexedInstanced(waterModel.mesh->indexCount, 1u, 0u, 0, 0u);
			}


			if (iceModel.isInView(frustum))
			{
				transparantCommandList->ResourceBarrier(2u, copyStartBarriers);
				transparantCommandList->OMSetRenderTargets(1u, &warpTextureCpuDescriptorHandle, TRUE, &depthStencilHandle);
				transparantCommandList->SetPipelineState(globalResources.pipelineStateObjects.copy);
				transparantCommandList->SetGraphicsRootConstantBufferView(2u, iceModel.vsAabbGpu());
				transparantCommandList->IASetVertexBuffers(0u, 1u, &cubeWithPos->vertexBufferView);
				transparantCommandList->IASetIndexBuffer(&cubeWithPos->indexBufferView);
				transparantCommandList->DrawIndexedInstanced(cubeWithPos->indexCount, 1u, 0u, 0, 0u);

				transparantCommandList->ResourceBarrier(2u, copyEndBarriers);
				transparantCommandList->OMSetRenderTargets(1u, &backBufferRenderTargetCpuHandle, TRUE, &depthStencilHandle);
				transparantCommandList->SetPipelineState(globalResources.pipelineStateObjects.glass);
				transparantCommandList->SetGraphicsRootConstantBufferView(2u, iceModel.vsConstantBufferGPU(frameIndex));
				transparantCommandList->SetGraphicsRootConstantBufferView(3u, iceModel.psConstantBufferGPU());
				transparantCommandList->IASetVertexBuffers(0u, 1u, &iceModel.mesh->vertexBufferView);
				transparantCommandList->IASetIndexBuffer(&iceModel.mesh->indexBufferView);
				transparantCommandList->DrawIndexedInstanced(iceModel.mesh->indexCount, 1u, 0u, 0, 0u);
			}
			
			if (fireModel1.isInView(frustum) || fireModel2.isInView(frustum))
			{
				transparantCommandList->SetPipelineState(globalResources.pipelineStateObjects.fire);

				transparantCommandList->IASetVertexBuffers(0u, 1u, &fireModel1.mesh->vertexBufferView);
				transparantCommandList->IASetIndexBuffer(&fireModel1.mesh->indexBufferView);

				if (fireModel1.isInView(frustum))
				{
					transparantCommandList->SetGraphicsRootConstantBufferView(2u, fireModel1.vsConstantBufferGPU(frameIndex));
					transparantCommandList->SetGraphicsRootConstantBufferView(3u, fireModel1.psConstantBufferGPU());
					transparantCommandList->DrawIndexedInstanced(fireModel1.mesh->indexCount, 1u, 0u, 0, 0u);
				}

				if (fireModel2.isInView(frustum))
				{
					transparantCommandList->SetGraphicsRootConstantBufferView(2u, fireModel2.vsConstantBufferGPU(frameIndex));
					transparantCommandList->SetGraphicsRootConstantBufferView(3u, fireModel2.psConstantBufferGPU());
					transparantCommandList->DrawIndexedInstanced(fireModel2.mesh->indexCount, 1u, 0u, 0, 0u);
				}
			}
		}

		void destruct(ThreadResources& threadResources, GlobalResources& globalResources)
		{
			auto& textureManager = globalResources.textureManager;
			auto& meshManager = globalResources.meshManager;
			auto& graphicsEngine = globalResources.graphicsEngine;
			for (auto i = 0u; i < numRenderTargetTextures; ++i)
			{
				graphicsEngine.descriptorAllocator.deallocate(shaderResourceViews[i]);
				graphicsEngine.descriptorAllocator.deallocate(reflectionCameraBackBuffers[i]);

				threadResources.taskShedular.update1NextQueue().concurrentPush({ reflectionCameras[i], [](void* context, ThreadResources& threadResources, GlobalResources& globalResources)
				{
					auto camera = reinterpret_cast<ReflectionCamera*>(context);
					globalResources.renderPass.renderToTextureSubPass().removeCamera(globalResources, *camera);
				} });
			}

			textureManager.unloadTexture(TextureNames::ground01, graphicsEngine);
			textureManager.unloadTexture(TextureNames::wall01, graphicsEngine);
			textureManager.unloadTexture(TextureNames::marble01, graphicsEngine);
			textureManager.unloadTexture(TextureNames::water01, graphicsEngine);
			textureManager.unloadTexture(TextureNames::ice01, graphicsEngine);
			textureManager.unloadTexture(TextureNames::icebump01, graphicsEngine);
			textureManager.unloadTexture(TextureNames::firenoise01, graphicsEngine);
			textureManager.unloadTexture(TextureNames::fire01, graphicsEngine);
			textureManager.unloadTexture(TextureNames::firealpha01, graphicsEngine);

			meshManager.unloadMesh(MeshNames::bath);
			meshManager.unloadMesh(MeshNames::plane1);
			meshManager.unloadMesh(MeshNames::wall);
			meshManager.unloadMesh(MeshNames::water);
			meshManager.unloadMesh(MeshNames::cube);
			meshManager.unloadMesh(MeshNames::square);

			perObjectConstantBuffers->Unmap(0u, nullptr);
		}
	};

	class MDResources
	{
		constexpr static unsigned int numComponents = 3u;
		constexpr static unsigned int numMeshes = 1u;
		constexpr static unsigned int numTextures = 1u;


		static void callback(Zone<ThreadResources, GlobalResources>& zone, ThreadResources& threadResources, GlobalResources& globalResources)
		{
			zone.componentUploaded(threadResources, numComponents);
		}
	public:
		D3D12Resource perObjectConstantBuffers;
		uint8_t* perObjectConstantBuffersCpuAddress;

		BathModel2 bathModel;
		Light light;


		MDResources(ThreadResources& threadResources, GlobalResources& globalResources, Zone<ThreadResources, GlobalResources>& zone) :
			light(DirectX::XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f), DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), DirectX::XMFLOAT3(0.0f, -0.894427191f, 0.447213595f)),
			perObjectConstantBuffers(globalResources.graphicsEngine.graphicsDevice, []()
		{
			D3D12_HEAP_PROPERTIES heapProperties;
			heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
			heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProperties.CreationNodeMask = 1u;
			heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			heapProperties.VisibleNodeMask = 1u;
			return heapProperties;
		}(), D3D12_HEAP_FLAGS::D3D12_HEAP_FLAG_NONE, []()
		{
			D3D12_RESOURCE_DESC resourceDesc;
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
			resourceDesc.Width = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
			resourceDesc.Height = 1u;
			resourceDesc.DepthOrArraySize = 1u;
			resourceDesc.MipLevels = 1u;
			resourceDesc.Format = DXGI_FORMAT::DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc = { 1u, 0u };
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			return resourceDesc;
		}(), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr)
		{
			D3D12_RANGE readRange{ 0u, 0u };
			HRESULT hr = perObjectConstantBuffers->Map(0u, &readRange, reinterpret_cast<void**>(&perObjectConstantBuffersCpuAddress));
			if (FAILED(hr)) throw HresultException(hr);
			auto PerObjectConstantBuffersGpuAddress = perObjectConstantBuffers->GetGPUVirtualAddress();
			auto cpuConstantBuffer = perObjectConstantBuffersCpuAddress;

			new(&bathModel) BathModel2(PerObjectConstantBuffersGpuAddress, cpuConstantBuffer);

			TextureManager& textureManager = globalResources.textureManager;
			textureManager.loadTexture(threadResources, globalResources, TextureNames::marble01, { &zone, [](void* context, void* tr, void* gr, unsigned int textureID) 
			{
				ThreadResources& threadResources = *reinterpret_cast<ThreadResources*>(tr);
				GlobalResources& globalResources = *reinterpret_cast<GlobalResources*>(gr);
				auto& zone = *reinterpret_cast<Zone<ThreadResources, GlobalResources>*>(context);
				auto resources = ((MDResources*)zone.newData);
				resources->bathModel.setDiffuseTexture(textureID, resources->perObjectConstantBuffersCpuAddress, resources->perObjectConstantBuffers->GetGPUVirtualAddress());
				callback(zone, threadResources, globalResources);
			} });

			MeshManager& meshManager = globalResources.meshManager;
			meshManager.loadMeshWithPositionTextureNormal(MeshNames::bath, &zone, threadResources, globalResources, [](void* context, void* tr, void* gr, Mesh* mesh)
			{
				ThreadResources& threadResources = *reinterpret_cast<ThreadResources*>(tr);
				GlobalResources& globalResources = *reinterpret_cast<GlobalResources*>(gr);
				auto& zone = *reinterpret_cast<Zone<ThreadResources, GlobalResources>*>(context);
				auto resources = ((MDResources*)zone.newData);
				resources->bathModel.mesh = mesh;
				callback(zone, threadResources, globalResources);
			});
		}

		void destruct(ThreadResources& threadResources, GlobalResources& globalResources)
		{
			auto& textureManager = globalResources.textureManager;
			auto& meshManager = globalResources.meshManager;
			auto& graphicsEngine = globalResources.graphicsEngine;

			textureManager.unloadTexture(TextureNames::marble01, graphicsEngine);

			meshManager.unloadMesh(MeshNames::bath);

			perObjectConstantBuffers->Unmap(0u, nullptr);
		}

		~MDResources() {}

		void update2(ThreadResources& threadResources, GlobalResources& globalResources)
		{
			uint32_t frameIndex = globalResources.graphicsEngine.frameIndex;
			const auto commandList = threadResources.renderPass.colorSubPass().opaqueCommandList();

			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			if (bathModel.isInView(globalResources.mainCamera().frustum()))
			{
				commandList->SetPipelineState(globalResources.pipelineStateObjects.directionalLight);

				commandList->SetGraphicsRootConstantBufferView(2u, bathModel.vsBufferGpu());
				commandList->SetGraphicsRootConstantBufferView(3u, bathModel.psBufferGpu());
				commandList->IASetVertexBuffers(0u, 1u, &bathModel.mesh->vertexBufferView);
				commandList->IASetIndexBuffer(&bathModel.mesh->indexBufferView);
				commandList->DrawIndexedInstanced(bathModel.mesh->indexCount, 1u, 0u, 0, 0u);
			}
		}

		void update1(ThreadResources& threadResources, GlobalResources& globalResources) {}

		static void create(void* context, ThreadResources& threadResources, GlobalResources& globalResources)
		{
			Zone<ThreadResources, GlobalResources>& zone = *reinterpret_cast<Zone<ThreadResources, GlobalResources>*>(context);
			zone.newData = malloc(sizeof(MDResources));
			new(zone.newData) MDResources(threadResources, globalResources, zone);
			callback(zone, threadResources, globalResources);
		}
	};

	static void update2(void* context, ThreadResources& threadResources, GlobalResources& globalResources);
	static void update1(void* context, ThreadResources& threadResources, GlobalResources& globalResources)
	{
		Zone<ThreadResources, GlobalResources>& zone = *reinterpret_cast<Zone<ThreadResources, GlobalResources>*>(context);

		zone.lastUsedData = zone.currentData;
		zone.lastUsedState = zone.currentState;

		switch (zone.currentState)
		{
		case 0u:
			reinterpret_cast<HDResources*>(zone.currentData)->update1(threadResources, globalResources);
			threadResources.taskShedular.update2NextQueue().push({ &zone, update2 });
			break;
		case 1u:
			reinterpret_cast<MDResources*>(zone.currentData)->update1(threadResources, globalResources);
			threadResources.taskShedular.update2NextQueue().push({ &zone, update2 });
			break;
		}
	}

	static void update2(void* context, ThreadResources& threadResources, GlobalResources& globalResources)
	{
		Zone<ThreadResources, GlobalResources>& zone = *reinterpret_cast<Zone<ThreadResources, GlobalResources>*>(context);

		switch (zone.lastUsedState)
		{
		case 0u:
			reinterpret_cast<HDResources*>(zone.lastUsedData)->update2(threadResources, globalResources);
			break;
		case 1u:
			reinterpret_cast<MDResources*>(zone.lastUsedData)->update2(threadResources, globalResources);
			break;
		}

		threadResources.taskShedular.update1NextQueue().push({ &zone, update1});
	}

	struct Zone1Functions
	{
		static void createNewStateData(Zone<ThreadResources, GlobalResources>& zone, ThreadResources& threadResources, GlobalResources& globalResources)
		{
			switch (zone.newState)
			{
			case 0u:
				threadResources.taskShedular.backgroundQueue().push({ &zone, &HDResources::create });
				break;
			case 1u:
				threadResources.taskShedular.backgroundQueue().push({ &zone, &MDResources::create });
				break;
			}
		}

		static void deleteOldStateData(Zone<ThreadResources, GlobalResources>& zone, ThreadResources& threadResources, GlobalResources& globalResources)
		{
			switch (zone.oldState)
			{
			case 0u:
				threadResources.taskShedular.backgroundQueue().push({ &zone, [](void* context, ThreadResources& threadResources, GlobalResources& globalResources)
				{
					Zone<ThreadResources, GlobalResources>& zone = *reinterpret_cast<Zone<ThreadResources, GlobalResources>*>(context);
					auto resource = reinterpret_cast<HDResources*>(zone.oldData);
					resource->destruct(threadResources, globalResources);
					resource->~HDResources();
					free(resource);
					zone.finishedDeletingOldState(threadResources);
				} });
				break;
			case 1u:
				threadResources.taskShedular.backgroundQueue().push({ &zone, [](void* context, ThreadResources& threadResources, GlobalResources& globalResources)
				{
					Zone<ThreadResources, GlobalResources>& zone = *reinterpret_cast<Zone<ThreadResources, GlobalResources>*>(context);
					auto resource = reinterpret_cast<MDResources*>(zone.oldData);
					resource->destruct(threadResources, globalResources);
					resource->~MDResources();
					free(resource);
					zone.finishedDeletingOldState(threadResources);
				} });
				break;
			}
		}

		static void loadConnectedAreas(Zone<ThreadResources, GlobalResources>& zone, ThreadResources& threadResources, GlobalResources& globalResources, float distance, Area::VisitedNode* loadedAreas)
		{
			globalResources.areas.cave.load(threadResources, globalResources, Vector2{ 0.0f, 91.0f }, distance, loadedAreas);
		}
		static bool changeArea(Zone<ThreadResources, GlobalResources>& zone, ThreadResources& threadResources, GlobalResources& globalResources)
		{
			auto& playerPosition = globalResources.playerPosition.location.position;
			if ((playerPosition.x - 74.0f) * (playerPosition.x - 74.0f) + (playerPosition.y + 30.0f) * (playerPosition.y + 30.0f) + (playerPosition.z - 51.0f) * (playerPosition.z - 51.0f) < 202.0f)
			{
				globalResources.areas.cave.setAsCurrentArea(threadResources, globalResources);
				return true;
			}
			return false;
		}

		static void start(Zone<ThreadResources, GlobalResources>& zone, ThreadResources& threadResources, GlobalResources& globalResources)
		{
			threadResources.taskShedular.update1NextQueue().push({ &zone, update1 });
		}
	};
}

Zone<ThreadResources, GlobalResources> Zone1()
{
	return Zone<ThreadResources, GlobalResources>::create<Zone1Functions>(2u);
}