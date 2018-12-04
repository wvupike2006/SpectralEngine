#include "TextureManager.h"
#include "DDSFileLoader.h"
#include "TextureIndexOutOfRangeException.h"
#include <d3d12.h>
#include "D3D12GraphicsEngine.h"

TextureManager::TextureManager() {}
TextureManager::~TextureManager() {}

void TextureManager::unloadTexture(const wchar_t* filename, D3D12GraphicsEngine& graphicsEngine)
{
	unsigned int descriptorIndex = std::numeric_limits<unsigned int>::max();
	ID3D12Resource* resource = nullptr;
	{
		std::lock_guard<std::mutex> lock(mutex);
		auto texurePtr = textures.find(filename);
		auto& texture = texurePtr->second;
		texture.numUsers -= 1u;
		if (texture.numUsers == 0u)
		{
			descriptorIndex = texture.descriptorIndex;
			resource = texture.resource.steal();
			textures.erase(texurePtr);
		}
	}
	if (descriptorIndex != std::numeric_limits<unsigned int>::max())
	{
		graphicsEngine.descriptorAllocator.deallocate(descriptorIndex);
		resource->Release();
	}
}

ID3D12Resource* TextureManager::createTexture(const TextureStreamingRequest& uploadRequest, TextureManager& textureManager, D3D12GraphicsEngine& graphicsEngine, const wchar_t* filename)
{
	D3D12_HEAP_PROPERTIES textureHeapProperties;
	textureHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	textureHeapProperties.CreationNodeMask = 1u;
	textureHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	textureHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	textureHeapProperties.VisibleNodeMask = 1u;

	D3D12_RESOURCE_DESC textureDesc;
	textureDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	textureDesc.DepthOrArraySize = std::max(uploadRequest.depth, uploadRequest.arraySize);
	textureDesc.Dimension = uploadRequest.dimension;
	textureDesc.Flags = D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_NONE;
	textureDesc.Format = uploadRequest.format;
	textureDesc.Height = uploadRequest.height;
	textureDesc.Layout = D3D12_TEXTURE_LAYOUT::D3D12_TEXTURE_LAYOUT_UNKNOWN;
	textureDesc.MipLevels = uploadRequest.mipLevels;
	textureDesc.SampleDesc.Count = 1u;
	textureDesc.SampleDesc.Quality = 0u;
	textureDesc.Width = uploadRequest.width;

	D3D12Resource resource(graphicsEngine.graphicsDevice, textureHeapProperties, D3D12_HEAP_FLAGS::D3D12_HEAP_FLAG_NONE, textureDesc,
		D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON);

#ifndef NDEBUG
	std::wstring name = L"texture ";
	name += filename;
	resource->SetName(name.c_str());
#endif // NDEBUG

	auto discriptorIndex = graphicsEngine.descriptorAllocator.allocate();
	D3D12_CPU_DESCRIPTOR_HANDLE discriptorHandle = graphicsEngine.mainDescriptorHeap->GetCPUDescriptorHandleForHeapStart() +
		discriptorIndex * graphicsEngine.constantBufferViewAndShaderResourceViewAndUnordedAccessViewDescriptorSize;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = uploadRequest.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	switch(uploadRequest.dimension)
	{
	case D3D12_RESOURCE_DIMENSION::D3D12_RESOURCE_DIMENSION_TEXTURE3D:
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_TEXTURE3D;
		srvDesc.Texture3D.MipLevels = uploadRequest.mipLevels;
		srvDesc.Texture3D.MostDetailedMip = 0u;
		srvDesc.Texture3D.ResourceMinLODClamp = 0u;
		break;
	case D3D12_RESOURCE_DIMENSION::D3D12_RESOURCE_DIMENSION_TEXTURE2D:
		if(uploadRequest.arraySize > 1u)
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srvDesc.Texture2DArray.MipLevels = uploadRequest.mipLevels;
			srvDesc.Texture2DArray.MostDetailedMip = 0u;
			srvDesc.Texture2DArray.ResourceMinLODClamp = 0u;
			srvDesc.Texture2DArray.ArraySize = uploadRequest.arraySize;
			srvDesc.Texture2DArray.FirstArraySlice = 0u;
			srvDesc.Texture2DArray.PlaneSlice = 0u;
		}
		else
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = uploadRequest.mipLevels;
			srvDesc.Texture2D.MostDetailedMip = 0u;
			srvDesc.Texture2D.ResourceMinLODClamp = 0u;
			srvDesc.Texture2D.PlaneSlice = 0u;
		}
		break;
	case D3D12_RESOURCE_DIMENSION::D3D12_RESOURCE_DIMENSION_TEXTURE1D:
		if(uploadRequest.arraySize > 1u)
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
			srvDesc.Texture1DArray.MipLevels = uploadRequest.mipLevels;
			srvDesc.Texture1DArray.MostDetailedMip = 0u;
			srvDesc.Texture1DArray.ResourceMinLODClamp = 0u;
			srvDesc.Texture1DArray.ArraySize = uploadRequest.arraySize;
			srvDesc.Texture1DArray.FirstArraySlice = 0u;
		}
		else
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_TEXTURE1D;
			srvDesc.Texture1D.MipLevels = uploadRequest.mipLevels;
			srvDesc.Texture1D.MostDetailedMip = 0u;
			srvDesc.Texture1D.ResourceMinLODClamp = 0u;
		}
		break;
	}
	graphicsEngine.graphicsDevice->CreateShaderResourceView(resource, &srvDesc, discriptorHandle);

	ID3D12Resource* res = resource;
	{
		std::lock_guard<decltype(textureManager.mutex)> lock(textureManager.mutex);
		Texture& texture = textureManager.textures[filename];
		texture.resource = std::move(resource);
		texture.descriptorIndex = discriptorIndex;
	}
	return res;
}

void TextureManager::loadTextureFromMemory(StreamingManager& streamingManager, const unsigned char* buffer, File file, TextureStreamingRequest& uploadRequest,
	void(*streamResource)(StreamingRequest* request, void* threadResources, void* globalResources), void(*textureUploaded)(StreamingRequest* request, void*, void*))
{
	const DDSFileLoader::DdsHeaderDx12& header = *reinterpret_cast<const DDSFileLoader::DdsHeaderDx12*>(buffer);
	bool valid = DDSFileLoader::validateDdsHeader(header);
	if(!valid) throw false;
	uploadRequest.streamResource = streamResource;
	uploadRequest.resourceUploaded = textureUploaded;
	uploadRequest.file = file;
	uploadRequest.width = header.width;
	uploadRequest.height = header.height;
	uploadRequest.format = header.dxgiFormat;
	uploadRequest.mipLevels = (uint16_t)header.mipMapCount;
	uploadRequest.dimension = (D3D12_RESOURCE_DIMENSION)header.dimension;
	if(header.miscFlag & 0x4L)
	{
		uploadRequest.arraySize = (uint16_t)(header.arraySize * 6u); //The texture is a cubemap
	}
	else
	{
		uploadRequest.arraySize = (uint16_t)header.arraySize;
	}
	uploadRequest.depth = (uint16_t)header.depth;
	uploadRequest.resourceSize = (unsigned long)DDSFileLoader::alignedResourceSize(uploadRequest.width, uploadRequest.height, uploadRequest.depth,
		uploadRequest.mipLevels, uploadRequest.arraySize, uploadRequest.format);

	streamingManager.addUploadRequest(&uploadRequest);
}

void TextureManager::textureUploadedHelper(const wchar_t* filename, void* tr, void* gr)
{
	unsigned int descriptorIndex;
	TextureStreamingRequest* requests;
	{
		std::lock_guard<decltype(mutex)> lock(mutex);
		auto& texture = textures[filename];
		texture.loaded = true;
		descriptorIndex = texture.descriptorIndex;

		auto requestsPos = uploadRequests.find(filename);
		assert(requestsPos != uploadRequests.end() && "A texture is loading with no requests for it");
		requests = std::move(requestsPos->second);
		uploadRequests.erase(requestsPos);
	}

	for(TextureStreamingRequest* current = requests; current != nullptr; current = current->nextTextureRequest)
	{
		current->textureLoaded(*current, tr, gr, descriptorIndex);
	}
}