#include "ReflectionCamera.h"
#include "CameraUtil.h"

ReflectionCamera::ReflectionCamera(ID3D12Resource* image, D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView, D3D12_CPU_DESCRIPTOR_HANDLE depthSencilView,
	unsigned int width, unsigned int height, D3D12_GPU_VIRTUAL_ADDRESS& constantBufferGpuAddress1, uint8_t*& constantBufferCpuAddress1, float fieldOfView,
	const Transform& target, uint32_t* backBufferTextures) :
	mWidth(width),
	mHeight(height),
	mImage(image),
	renderTargetView(renderTargetView),
	depthSencilView(depthSencilView)
{
	constantBufferCpuAddress = reinterpret_cast<CameraConstantBuffer*>(constantBufferCpuAddress1);
	constantBufferCpuAddress1 += bufferSizePS * frameBufferCount;
	constantBufferGpuAddress = constantBufferGpuAddress1;
	constantBufferGpuAddress1 += bufferSizePS * frameBufferCount;

	mLocation.position = target.position;
	mLocation.rotation = target.rotation;

	DirectX::XMMATRIX mViewMatrix = mLocation.toMatrix();

	const float screenAspect = static_cast<float>(width) / static_cast<float>(height);
	mProjectionMatrix = DirectX::XMMatrixPerspectiveFovLH(fieldOfView, screenAspect, screenNear, screenDepth);

	for (auto i = 0u; i < frameBufferCount; ++i)
	{
		auto constantBuffer = reinterpret_cast<CameraConstantBuffer*>(reinterpret_cast<unsigned char*>(constantBufferCpuAddress) + i * bufferSizePS);
		constantBuffer->viewProjectionMatrix = mViewMatrix * mProjectionMatrix;
		constantBuffer->cameraPosition = mLocation.position;
		constantBuffer->screenWidth = (float)width;
		constantBuffer->screenHeight = (float)height;
		constantBuffer->backBufferTexture = backBufferTextures[i];
	}

	mFrustum.update(mProjectionMatrix, mViewMatrix, screenNear, screenDepth);
}

ReflectionCamera::~ReflectionCamera() {}

void ReflectionCamera::update(uint32_t frameIndex, const DirectX::XMMATRIX& mViewMatrix)
{
	const auto constantBuffer = reinterpret_cast<CameraConstantBuffer*>(reinterpret_cast<unsigned char*>(constantBufferCpuAddress) + frameIndex * bufferSizePS);
	constantBuffer->viewProjectionMatrix = mViewMatrix * mProjectionMatrix;;
	constantBuffer->cameraPosition = mLocation.position;

	mFrustum.update(mProjectionMatrix, mViewMatrix, screenNear, screenDepth);
}

void ReflectionCamera::bind(uint32_t frameIndex, ID3D12GraphicsCommandList** first, ID3D12GraphicsCommandList** end)
{
	auto constantBufferGPU = constantBufferGpuAddress + bufferSizePS * frameIndex;
	CameraUtil::bind(first, end, CameraUtil::getViewPort(mWidth, mHeight), CameraUtil::getScissorRect(mWidth, mHeight), constantBufferGPU, &renderTargetView, &depthSencilView);
}

void ReflectionCamera::bindFirstThread(uint32_t frameIndex, ID3D12GraphicsCommandList** first, ID3D12GraphicsCommandList** end)
{
	bind(frameIndex, first, end);
	auto commandList = *first;
	constexpr float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	commandList->ClearRenderTargetView(renderTargetView, clearColor, 0u, nullptr);
	commandList->ClearDepthStencilView(depthSencilView, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0u, nullptr);
}