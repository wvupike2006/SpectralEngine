#pragma once
#include <GraphicsPipelineStateDesc.h>

namespace PipelineStateObjectDescs
{
	class PointLight
	{
		static constexpr D3D12_SO_DECLARATION_ENTRY* soDeclarations = nullptr;
		static constexpr UINT numSoDeclarations = UINT{0ul};

		static constexpr UINT* bufferStrides = nullptr;
		static constexpr UINT numBufferStrides = UINT{0ul};

		static constexpr D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] =
		{
			D3D12_INPUT_ELEMENT_DESC{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, UINT{0ul}, UINT{0ul}, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, UINT{0ul}},
			D3D12_INPUT_ELEMENT_DESC{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, UINT{0ul}, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, UINT{0ul}},
			D3D12_INPUT_ELEMENT_DESC{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, UINT{0ul}, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, UINT{0ul}},
		};
		static constexpr UINT numInputElementDescs = UINT{3ul};
	public:
		static inline GraphicsPipelineStateDesc desc =
		{
			L"../DemoGame/CompiledShaders/LightVS.cso",
			L"../DemoGame/CompiledShaders/PointLightPS.cso",
			D3D12_STREAM_OUTPUT_DESC{soDeclarations, numSoDeclarations, bufferStrides, numBufferStrides, 0},
			D3D12_BLEND_DESC
			{
				FALSE,
				FALSE,
				{
					D3D12_RENDER_TARGET_BLEND_DESC{FALSE, FALSE, D3D12_BLEND_SRC_COLOR, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD, D3D12_BLEND_ONE, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD, D3D12_LOGIC_OP_CLEAR, D3D12_COLOR_WRITE_ENABLE_ALL},
					D3D12_RENDER_TARGET_BLEND_DESC{FALSE, FALSE, D3D12_BLEND_SRC_COLOR, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD, D3D12_BLEND_ONE, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD, D3D12_LOGIC_OP_CLEAR, D3D12_COLOR_WRITE_ENABLE_ALL},
					D3D12_RENDER_TARGET_BLEND_DESC{FALSE, FALSE, D3D12_BLEND_SRC_COLOR, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD, D3D12_BLEND_ONE, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD, D3D12_LOGIC_OP_CLEAR, D3D12_COLOR_WRITE_ENABLE_ALL},
					D3D12_RENDER_TARGET_BLEND_DESC{FALSE, FALSE, D3D12_BLEND_SRC_COLOR, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD, D3D12_BLEND_ONE, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD, D3D12_LOGIC_OP_CLEAR, D3D12_COLOR_WRITE_ENABLE_ALL},
					D3D12_RENDER_TARGET_BLEND_DESC{FALSE, FALSE, D3D12_BLEND_SRC_COLOR, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD, D3D12_BLEND_ONE, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD, D3D12_LOGIC_OP_CLEAR, D3D12_COLOR_WRITE_ENABLE_ALL},
					D3D12_RENDER_TARGET_BLEND_DESC{FALSE, FALSE, D3D12_BLEND_SRC_COLOR, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD, D3D12_BLEND_ONE, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD, D3D12_LOGIC_OP_CLEAR, D3D12_COLOR_WRITE_ENABLE_ALL},
					D3D12_RENDER_TARGET_BLEND_DESC{FALSE, FALSE, D3D12_BLEND_SRC_COLOR, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD, D3D12_BLEND_ONE, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD, D3D12_LOGIC_OP_CLEAR, D3D12_COLOR_WRITE_ENABLE_ALL},
					D3D12_RENDER_TARGET_BLEND_DESC{FALSE, FALSE, D3D12_BLEND_SRC_COLOR, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD, D3D12_BLEND_ONE, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD, D3D12_LOGIC_OP_CLEAR, D3D12_COLOR_WRITE_ENABLE_ALL},
				}
			},
			UINT{0xffffffff},
			D3D12_RASTERIZER_DESC{D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_BACK, FALSE, 0, 0, 0, TRUE, FALSE, FALSE, UINT{0ul}, D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF},
			D3D12_DEPTH_STENCIL_DESC
			{
				TRUE,
				D3D12_DEPTH_WRITE_MASK_ALL,
				D3D12_COMPARISON_FUNC_LESS,
				FALSE,
				UINT8{0u},
				UINT8{0u},
				D3D12_DEPTH_STENCILOP_DESC{D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS},
				D3D12_DEPTH_STENCILOP_DESC{D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS}
			},
			D3D12_INPUT_LAYOUT_DESC{inputElementDescs, numInputElementDescs},
			D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
			D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
			UINT{1ul},
			{DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN},
			DXGI_FORMAT_D32_FLOAT,
			DXGI_SAMPLE_DESC{UINT{1ul}, UINT{0ul}}
		};
	};
};
