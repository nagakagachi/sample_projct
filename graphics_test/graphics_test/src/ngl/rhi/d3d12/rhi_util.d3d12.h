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
		using ResourceViewName = ngl::text::FixedString<32>;

		enum class RootParameterType : u16
		{
			ConstantBuffer,
			ShaderResource,
			UnorderedAccess,
			Sampler,

			_Max
		};

		// このシステムではRootSignature固定のため各リソースタイプはシェーダステージ毎にレジスタ0から固定数でバインドする.
		// 以下は各リソースタイプの固定数.
		static const u32 k_cbv_table_size = 16;
		static const u32 k_srv_table_size = 48;
		static const u32 k_uav_table_size = 16;
		static const u32 k_sampler_table_size = 16;

		static constexpr u32 RootParameterTableSize(RootParameterType type)
		{
			const u32 type_size[] =
			{
				k_cbv_table_size,
				k_srv_table_size,
				k_uav_table_size,
				k_sampler_table_size,

				0,
			};
			static_assert(std::size(type_size) - 1 == static_cast<size_t>(RootParameterType::_Max), "");
			return type_size[static_cast<u32>(type)];
		}


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