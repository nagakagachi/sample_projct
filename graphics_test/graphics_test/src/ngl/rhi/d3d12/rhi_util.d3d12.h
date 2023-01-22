#pragma once

#include "ngl/rhi/rhi.h"


#include <iostream>
#include <vector>
#include <unordered_map>


#include "ngl/util/types.h"
#include "ngl/text/hash_text.h"

#include <d3d12.h>
#if 1
#include <dxgi1_6.h>
#else
#include <dxgi1_4.h>
#endif

#include <atlbase.h>

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

		D3D_PRIMITIVE_TOPOLOGY ConvertPrimitiveTopology(PrimitiveTopology v);

		D3D12_FILTER ConvertTextureFilter(TextureFilterMode v);

		D3D12_TEXTURE_ADDRESS_MODE ConvertTextureAddressMode(TextureAddressMode v);



		constexpr uint32_t align_to(uint32_t alignment, uint32_t value)
		{
			return (((value + alignment - 1) / alignment) * alignment);
		}

	}
}