#pragma once

#include <iostream>
#include <vector>

#include "ngl/rhi/rhi.h"
#include "ngl/util/types.h"
#include <d3d12.h>


namespace ngl
{
	namespace rhi
	{
		DXGI_FORMAT ConvertResourceFormat(ResourceFormat v);

		D3D12_RESOURCE_STATES ConvertResourceState(ResourceState v);

		D3D12_BLEND_OP	ConvertBlendOp(BlendOp v);

		D3D12_BLEND	ConvertBlendFactor(BlendFactor v);

		D3D12_CULL_MODE	ConvertCullMode(CullingMode v);

		D3D12_FILL_MODE ConvertFillMode(FillMode v);

		D3D12_STENCIL_OP ConvertStencilOp(StencilOp v);

		D3D12_COMPARISON_FUNC ConvertComparisonFunc(CompFunc v);

		D3D12_PRIMITIVE_TOPOLOGY_TYPE ConvertPrimitiveTopologyType(PrimitiveTopologyType v);
	}
}