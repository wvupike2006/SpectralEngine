#pragma once
#include <D3D12PipelineState.h>
#include <atomic>
#include <AsynchronousFileManager.h>
#include <Array.h>
#include <PsoLoader.h>
#include "RootSignatures.h"

class PipelineStateObjects
{
public:
	class PipelineLoader;
	class PipelineLoaderImpl
	{
	public:
		class ShaderLoader : public PsoLoader::PsoWithVertexAndPixelShaderRequest
		{
		public:
			PipelineLoader* pipelineLoader;
			D3D12PipelineState& piplineStateObject;

			ShaderLoader(GraphicsPipelineStateDesc& graphicsPipelineStateDesc, AsynchronousFileManager& asynchronousFileManager, ID3D12Device& grphicsDevice,
				void(*psoLoadedCallback1)(PsoLoader::PsoWithVertexAndPixelShaderRequest& request, D3D12PipelineState pso, void* tr, void* gr),
				PipelineLoader* pipelineLoader1, D3D12PipelineState& piplineStateObject1) :
				PsoLoader::PsoWithVertexAndPixelShaderRequest(graphicsPipelineStateDesc, asynchronousFileManager, grphicsDevice, psoLoadedCallback1),
				pipelineLoader(pipelineLoader1),
				piplineStateObject(piplineStateObject1)
				{}
		};
	
		constexpr static unsigned int numberOfComponents = 15u;
		std::atomic<unsigned int> numberOfcomponentsLoaded = 0u;

		Array<ShaderLoader, numberOfComponents> shaderLoaders;

		static void componentLoaded(PsoLoader::PsoWithVertexAndPixelShaderRequest& request, D3D12PipelineState pso, void* tr, void* gr);

		PipelineLoaderImpl(GraphicsPipelineStateDesc* const(&graphicsPipelineStateDescs)[numberOfComponents], D3D12PipelineState* const(&output)[numberOfComponents],
			AsynchronousFileManager& asynchronousFileManager, ID3D12Device& grphicsDevice, PipelineLoader& pipelineLoader, ID3D12RootSignature& rootSignature);
	};

	class PipelineLoader
	{
		friend class PipelineLoaderImpl;
		friend class PipelineStateObjects;
		union
		{
			PipelineLoaderImpl impl;
		};
		void(*loadingFinished)(PipelineLoader& pipelineLoader, void* tr, void* gr);

	public:
		PipelineLoader(void(*loadingFinished)(PipelineLoader& pipelineLoader, void* tr, void* gr)) :
			loadingFinished(loadingFinished) {}

		~PipelineLoader() {}
	};

	PipelineStateObjects(AsynchronousFileManager& asynchronousFileManager, ID3D12Device& graphicsDevice, RootSignatures& rootSignatures, PipelineLoader& pipelineLoader);
	~PipelineStateObjects() {}

	D3D12PipelineState text;
	D3D12PipelineState directionalLight;
	D3D12PipelineState directionalLightVt;
	D3D12PipelineState directionalLightVtTwoSided;
	D3D12PipelineState pointLight;
	D3D12PipelineState waterWithReflectionTexture;
	D3D12PipelineState waterNoReflectionTexture;
	D3D12PipelineState glass;
	D3D12PipelineState basic;
	D3D12PipelineState fire;
	D3D12PipelineState copy;
	D3D12PipelineState vtFeedback;
	D3D12PipelineState vtFeedbackWithNormals;
	D3D12PipelineState vtFeedbackWithNormalsTwoSided;
	D3D12PipelineState vtDebugDraw;
};