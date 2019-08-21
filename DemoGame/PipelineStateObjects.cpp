#include "PipelineStateObjects.h"
#include <d3d12.h>
#include <IOCompletionQueue.h>
#include <makeArray.h>

#include "Resources/PipelineStateObjects/Generated/BasicPso.h"
#include "Resources/PipelineStateObjects/Generated/CopyPso.h"
#include "Resources/PipelineStateObjects/Generated/DirectionalLightPso.h"
#include "Resources/PipelineStateObjects/Generated/DirectionalLightVtPso.h"
#include "Resources/PipelineStateObjects/Generated/DirectionalLightVtTwoSidedPso.h"
#include "Resources/PipelineStateObjects/Generated/FirePso.h"
#include "Resources/PipelineStateObjects/Generated/GlassPso.h"
#include "Resources/PipelineStateObjects/Generated/PointLightPso.h"
#include "Resources/PipelineStateObjects/Generated/TextPso.h"
#include "Resources/PipelineStateObjects/Generated/VtDebugDrawPso.h"
#include "Resources/PipelineStateObjects/Generated/VtFeedbackPso.h"
#include "Resources/PipelineStateObjects/Generated/VtFeedbackWithNormalsPso.h"
#include "Resources/PipelineStateObjects/Generated/VtFeedbackWithNormalsTwoSidedPso.h"
#include "Resources/PipelineStateObjects/Generated/WaterNoReflectionTexturePso.h"
#include "Resources/PipelineStateObjects/Generated/WaterWithReflectionTexturePso.h"


PipelineStateObjects::PipelineStateObjects(AsynchronousFileManager& asynchronousFileManager, ID3D12Device& grphicsDevice, RootSignatures& rootSignatures, PipelineLoader& pipelineLoader
#ifndef NDEBUG
	, bool isWarp
#endif
)
{
	new(&pipelineLoader.impl) PipelineLoaderImpl(
		{ &PipelineStateObjectDescs::Text::desc, &PipelineStateObjectDescs::DirectionalLight::desc, &PipelineStateObjectDescs::DirectionalLightVt::desc, &PipelineStateObjectDescs::DirectionalLightVtTwoSided::desc, &PipelineStateObjectDescs::PointLight::desc,
		&PipelineStateObjectDescs::WaterWithReflectionTexture::desc, &PipelineStateObjectDescs::WaterNoReflectionTexture::desc, &PipelineStateObjectDescs::Glass::desc, &PipelineStateObjectDescs::Basic::desc, &PipelineStateObjectDescs::Fire::desc,
		&PipelineStateObjectDescs::Copy::desc, &PipelineStateObjectDescs::VtFeedback::desc, &PipelineStateObjectDescs::VtFeedbackWithNormals::desc, &PipelineStateObjectDescs::VtFeedbackWithNormalsTwoSided::desc, &PipelineStateObjectDescs::VtDebugDraw::desc },
		{ &text, &directionalLight, &directionalLightVt, &directionalLightVtTwoSided, &pointLight, &waterWithReflectionTexture, &waterNoReflectionTexture, &glass, &basic, &fire, &copy, &vtFeedback, &vtFeedbackWithNormals, &vtFeedbackWithNormalsTwoSided, &vtDebugDraw },
		asynchronousFileManager, grphicsDevice, pipelineLoader, *rootSignatures.rootSignature
#ifndef NDEBUG
		, isWarp
#endif
	);
}

PipelineStateObjects::PipelineLoaderImpl::PipelineLoaderImpl(GraphicsPipelineStateDesc* const(&graphicsPipelineStateDescs)[numberOfComponents], D3D12PipelineState* const(&output)[numberOfComponents],
	AsynchronousFileManager& asynchronousFileManager, ID3D12Device& grphicsDevice, PipelineLoader& pipelineLoader, ID3D12RootSignature& rootSignature
#ifndef NDEBUG
	, bool isWarp
#endif
) :
	psoLoaders{ makeArray<numberOfComponents>([&](std::size_t i)
		{
			return ShaderLoader{*graphicsPipelineStateDescs[i], asynchronousFileManager, grphicsDevice, componentLoaded, &pipelineLoader, *output[i]};
		}) }
{
	for (auto graphicsPipelineStateDesc : graphicsPipelineStateDescs)
	{
		graphicsPipelineStateDesc->pRootSignature = &rootSignature;
#ifndef NDEBUG
		if (isWarp)
		{
			graphicsPipelineStateDesc->Flags = D3D12_PIPELINE_STATE_FLAG_TOOL_DEBUG;
		}
#endif
	}
	for (auto& loader : psoLoaders)
	{
		PsoLoader::loadPsoWithVertexAndPixelShaders(loader, asynchronousFileManager);
	}
}

void PipelineStateObjects::PipelineLoaderImpl::componentLoaded(PsoLoader::PsoWithVertexAndPixelShaderRequest& request1, D3D12PipelineState pso, void* tr)
{
	auto& request = static_cast<ShaderLoader&>(request1);
	request.piplineStateObject = std::move(pso);
	auto pipelineLoader = request.pipelineLoader;
	if(pipelineLoader->impl.numberOfcomponentsLoaded.fetch_add(1u, std::memory_order_acq_rel) == (numberOfComponents - 1u))
	{
		pipelineLoader->impl.~PipelineLoaderImpl();
		pipelineLoader->loadingFinished(*pipelineLoader, tr);
	}
}