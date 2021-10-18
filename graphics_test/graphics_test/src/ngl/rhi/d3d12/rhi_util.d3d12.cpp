

#include "rhi_util.d3d12.h"

namespace ngl
{
	namespace rhi
	{
		DXGI_FORMAT ConvertResourceFormat(ResourceFormat v)
		{
			static DXGI_FORMAT table[] = 
			{
				DXGI_FORMAT_UNKNOWN,
				DXGI_FORMAT_R32G32B32A32_TYPELESS,
				DXGI_FORMAT_R32G32B32A32_FLOAT,
				DXGI_FORMAT_R32G32B32A32_UINT,
				DXGI_FORMAT_R32G32B32A32_SINT,
				DXGI_FORMAT_R32G32B32_TYPELESS,
				DXGI_FORMAT_R32G32B32_FLOAT,
				DXGI_FORMAT_R32G32B32_UINT,
				DXGI_FORMAT_R32G32B32_SINT,
				DXGI_FORMAT_R16G16B16A16_TYPELESS,
				DXGI_FORMAT_R16G16B16A16_FLOAT,
				DXGI_FORMAT_R16G16B16A16_UNORM,
				DXGI_FORMAT_R16G16B16A16_UINT,
				DXGI_FORMAT_R16G16B16A16_SNORM,
				DXGI_FORMAT_R16G16B16A16_SINT,
				DXGI_FORMAT_R32G32_TYPELESS,
				DXGI_FORMAT_R32G32_FLOAT,
				DXGI_FORMAT_R32G32_UINT,
				DXGI_FORMAT_R32G32_SINT,
				DXGI_FORMAT_R32G8X24_TYPELESS,
				DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
				DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,
				DXGI_FORMAT_X32_TYPELESS_G8X24_UINT,
				DXGI_FORMAT_R10G10B10A2_TYPELESS,
				DXGI_FORMAT_R10G10B10A2_UNORM,
				DXGI_FORMAT_R10G10B10A2_UINT,
				DXGI_FORMAT_R11G11B10_FLOAT,
				DXGI_FORMAT_R8G8B8A8_TYPELESS,
				DXGI_FORMAT_R8G8B8A8_UNORM,
				DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
				DXGI_FORMAT_R8G8B8A8_UINT,
				DXGI_FORMAT_R8G8B8A8_SNORM,
				DXGI_FORMAT_R8G8B8A8_SINT,
				DXGI_FORMAT_R16G16_TYPELESS,
				DXGI_FORMAT_R16G16_FLOAT,
				DXGI_FORMAT_R16G16_UNORM,
				DXGI_FORMAT_R16G16_UINT,
				DXGI_FORMAT_R16G16_SNORM,
				DXGI_FORMAT_R16G16_SINT,
				DXGI_FORMAT_R32_TYPELESS,
				DXGI_FORMAT_D32_FLOAT,
				DXGI_FORMAT_R32_FLOAT,
				DXGI_FORMAT_R32_UINT,
				DXGI_FORMAT_R32_SINT,
				DXGI_FORMAT_R24G8_TYPELESS,
				DXGI_FORMAT_D24_UNORM_S8_UINT,
				DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
				DXGI_FORMAT_X24_TYPELESS_G8_UINT,
				DXGI_FORMAT_R8G8_TYPELESS,
				DXGI_FORMAT_R8G8_UNORM,
				DXGI_FORMAT_R8G8_UINT,
				DXGI_FORMAT_R8G8_SNORM,
				DXGI_FORMAT_R8G8_SINT,
				DXGI_FORMAT_R16_TYPELESS,
				DXGI_FORMAT_R16_FLOAT,
				DXGI_FORMAT_D16_UNORM,
				DXGI_FORMAT_R16_UNORM,
				DXGI_FORMAT_R16_UINT,
				DXGI_FORMAT_R16_SNORM,
				DXGI_FORMAT_R16_SINT,
				DXGI_FORMAT_R8_TYPELESS,
				DXGI_FORMAT_R8_UNORM,
				DXGI_FORMAT_R8_UINT,
				DXGI_FORMAT_R8_SNORM,
				DXGI_FORMAT_R8_SINT,
				DXGI_FORMAT_A8_UNORM,
				DXGI_FORMAT_R1_UNORM,
				DXGI_FORMAT_R9G9B9E5_SHAREDEXP,
				DXGI_FORMAT_R8G8_B8G8_UNORM,
				DXGI_FORMAT_G8R8_G8B8_UNORM,
				DXGI_FORMAT_BC1_TYPELESS,
				DXGI_FORMAT_BC1_UNORM,
				DXGI_FORMAT_BC1_UNORM_SRGB,
				DXGI_FORMAT_BC2_TYPELESS,
				DXGI_FORMAT_BC2_UNORM,
				DXGI_FORMAT_BC2_UNORM_SRGB,
				DXGI_FORMAT_BC3_TYPELESS,
				DXGI_FORMAT_BC3_UNORM,
				DXGI_FORMAT_BC3_UNORM_SRGB,
				DXGI_FORMAT_BC4_TYPELESS,
				DXGI_FORMAT_BC4_UNORM,
				DXGI_FORMAT_BC4_SNORM,
				DXGI_FORMAT_BC5_TYPELESS,
				DXGI_FORMAT_BC5_UNORM,
				DXGI_FORMAT_BC5_SNORM,
				DXGI_FORMAT_B5G6R5_UNORM,
				DXGI_FORMAT_B5G5R5A1_UNORM,
				DXGI_FORMAT_B8G8R8A8_UNORM,
				DXGI_FORMAT_B8G8R8X8_UNORM,
				DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM,
				DXGI_FORMAT_B8G8R8A8_TYPELESS,
				DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
				DXGI_FORMAT_B8G8R8X8_TYPELESS,
				DXGI_FORMAT_B8G8R8X8_UNORM_SRGB,
				DXGI_FORMAT_BC6H_TYPELESS,
				DXGI_FORMAT_BC6H_UF16,
				DXGI_FORMAT_BC6H_SF16,
				DXGI_FORMAT_BC7_TYPELESS,
				DXGI_FORMAT_BC7_UNORM,
				DXGI_FORMAT_BC7_UNORM_SRGB,
				DXGI_FORMAT_AYUV,
				DXGI_FORMAT_Y410,
				DXGI_FORMAT_Y416,
				DXGI_FORMAT_NV12,
				DXGI_FORMAT_P010,
				DXGI_FORMAT_P016,
				DXGI_FORMAT_420_OPAQUE,
				DXGI_FORMAT_YUY2,
				DXGI_FORMAT_Y210,
				DXGI_FORMAT_Y216,
				DXGI_FORMAT_NV11,
				DXGI_FORMAT_AI44,
				DXGI_FORMAT_IA44,
				DXGI_FORMAT_P8,
				DXGI_FORMAT_A8P8,
				DXGI_FORMAT_B4G4R4A4_UNORM
			};
			static_assert(std::size(table) == static_cast<size_t>(ResourceFormat::_MAX), "");
			
			// 現状はDXGIと一対一の対応
			return table[static_cast<u32>(v)];
		}

		D3D12_RESOURCE_STATES ConvertResourceState(ResourceState v)
		{
			D3D12_RESOURCE_STATES ret = {};
			switch (v)
			{
			case ResourceState::Common:
			{
				ret = D3D12_RESOURCE_STATE_COMMON;
				break;
			}
			case ResourceState::General:
			{
				ret = D3D12_RESOURCE_STATE_GENERIC_READ;
				break;
			}
			case ResourceState::ConstatnBuffer:
			{
				ret = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
				break;
			}
			case ResourceState::VertexBuffer:
			{
				ret = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
				break;
			}
			case ResourceState::IndexBuffer:
			{
				ret = D3D12_RESOURCE_STATE_INDEX_BUFFER;
				break;
			}
			case ResourceState::RenderTarget:
			{
				ret = D3D12_RESOURCE_STATE_RENDER_TARGET;
				break;
			}
			case ResourceState::ShaderRead:
			{
				ret = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				break;
			}
			case ResourceState::UnorderedAccess:
			{
				ret = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
				break;
			}
			case ResourceState::DepthWrite:
			{
				ret = D3D12_RESOURCE_STATE_DEPTH_WRITE;
				break;
			}
			case ResourceState::DepthRead:
			{
				ret = D3D12_RESOURCE_STATE_DEPTH_READ;
				break;
			}
			case ResourceState::IndirectArgument:
			{
				ret = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
				break;
			}
			case ResourceState::CopyDst:
			{
				ret = D3D12_RESOURCE_STATE_COPY_DEST;
				break;
			}
			case ResourceState::CopySrc:
			{
				ret = D3D12_RESOURCE_STATE_COPY_SOURCE;
				break;
			}
			case ResourceState::Present:
			{
				ret = D3D12_RESOURCE_STATE_PRESENT;
				break;
			}
			default:
			{
				std::cout << "ERROR : Invalid Resource State" << std::endl;
			}
			}
			return ret;
		}

		D3D12_BLEND_OP	ConvertBlendOp(BlendOp v)
		{
			switch (v)
			{
			case BlendOp::Add:
			{
				return D3D12_BLEND_OP_ADD;
			}
			case BlendOp::Subtract:
			{
				return D3D12_BLEND_OP_SUBTRACT;
			}
			case BlendOp::RevSubtract:
			{
				return D3D12_BLEND_OP_REV_SUBTRACT;
			}
			case BlendOp::Min:
			{
				return D3D12_BLEND_OP_MIN;
			}
			case BlendOp::Max:
			{
				return D3D12_BLEND_OP_MAX;
			}
			default:
			{
				return D3D12_BLEND_OP_ADD;
			}
			}
		}

		D3D12_BLEND	ConvertBlendFactor(BlendFactor v)
		{
			switch (v)
			{
				case BlendFactor::Zero:
				{
					return D3D12_BLEND_ZERO;
				}
				case BlendFactor::One:
				{
					return D3D12_BLEND_ONE;
				}
				case BlendFactor::SrcColor:
				{
					return D3D12_BLEND_SRC_COLOR;
				}
				case BlendFactor::InvSrcColor:
				{
					return D3D12_BLEND_INV_SRC_COLOR;
				}
				case BlendFactor::SrcAlpha:
				{
					return D3D12_BLEND_SRC_ALPHA;
				}
				case BlendFactor::InvSrcAlpha:
				{
					return D3D12_BLEND_INV_SRC_ALPHA;
				}
				case BlendFactor::DestAlpha:
				{
					return D3D12_BLEND_DEST_ALPHA;
				}
				case BlendFactor::InvDestAlpha:
				{
					return D3D12_BLEND_INV_DEST_ALPHA;
				}
				case BlendFactor::DestColor:
				{
					return D3D12_BLEND_DEST_COLOR;
				}
				case BlendFactor::InvDestColor:
				{
					return D3D12_BLEND_INV_DEST_COLOR;
				}
				case BlendFactor::SrcAlphaSat:
				{
					return D3D12_BLEND_SRC_ALPHA_SAT;
				}
				case BlendFactor::BlendFactor:
				{
					return D3D12_BLEND_BLEND_FACTOR;
				}
				case BlendFactor::InvBlendFactor:
				{
					return D3D12_BLEND_INV_BLEND_FACTOR;
				}
				case BlendFactor::Src1Color:
				{
					return D3D12_BLEND_SRC1_COLOR;
				}
				case BlendFactor::InvSrc1Color:
				{
					return D3D12_BLEND_INV_SRC1_COLOR;
				}
				case BlendFactor::Src1Alpha:
				{
					return D3D12_BLEND_SRC1_ALPHA;
				}
				case BlendFactor::InvSrc1Alpha:
				{
					return D3D12_BLEND_INV_SRC1_ALPHA;
				}
				default:
				{
					return D3D12_BLEND_ONE;
				}
			}
		}


		D3D12_CULL_MODE	ConvertCullMode(CullingMode v)
		{
			switch (v)
			{
			case CullingMode::Front:
				return D3D12_CULL_MODE_FRONT;

			case CullingMode::Back:
				return D3D12_CULL_MODE_BACK;

			default:
				return D3D12_CULL_MODE_NONE;
			}
		}

		D3D12_FILL_MODE ConvertFillMode(FillMode v)
		{
			switch (v)
			{
			case FillMode::Wireframe:
				return D3D12_FILL_MODE_WIREFRAME;

			case FillMode::Solid:
				return D3D12_FILL_MODE_SOLID;

			default:
				return D3D12_FILL_MODE_SOLID;
			}
		}

		D3D12_STENCIL_OP ConvertStencilOp(StencilOp v)
		{
			switch (v)
			{
			case StencilOp::Keep:
				return D3D12_STENCIL_OP_KEEP;

			case StencilOp::Zero:
				return D3D12_STENCIL_OP_ZERO;

			case StencilOp::Replace:
				return D3D12_STENCIL_OP_REPLACE;

			case StencilOp::IncrSat:
				return D3D12_STENCIL_OP_INCR_SAT;

			case StencilOp::DecrSat:
				return D3D12_STENCIL_OP_DECR_SAT;

			case StencilOp::Invert:
				return D3D12_STENCIL_OP_INVERT;

			case StencilOp::Incr:
				return D3D12_STENCIL_OP_INCR;

			case StencilOp::Decr:
				return D3D12_STENCIL_OP_DECR;

			default:
				return D3D12_STENCIL_OP_KEEP;
			}
		}

		D3D12_COMPARISON_FUNC ConvertComparisonFunc(CompFunc v)
		{
			switch (v)
			{
				case CompFunc::Never:
				return D3D12_COMPARISON_FUNC_NEVER;

				case CompFunc::Less:
				return D3D12_COMPARISON_FUNC_LESS;

				case CompFunc::Equal:
				return D3D12_COMPARISON_FUNC_EQUAL;

				case CompFunc::LessEqual:
				return D3D12_COMPARISON_FUNC_LESS_EQUAL;

				case CompFunc::Greater:
				return D3D12_COMPARISON_FUNC_GREATER;

				case CompFunc::NotEqual:
				return D3D12_COMPARISON_FUNC_NOT_EQUAL;

				case CompFunc::GreaterEqual:
				return D3D12_COMPARISON_FUNC_GREATER_EQUAL;

				case CompFunc::Always:
				return D3D12_COMPARISON_FUNC_ALWAYS;

			default:
				return D3D12_COMPARISON_FUNC_ALWAYS;
			}
		}

		D3D12_PRIMITIVE_TOPOLOGY_TYPE ConvertPrimitiveTopologyType(PrimitiveTopologyType v)
		{
			switch (v)
			{
			case PrimitiveTopologyType::Point:
				return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;

			case PrimitiveTopologyType::Line:
				return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;

			case PrimitiveTopologyType::Triangle:
				return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

			case PrimitiveTopologyType::Patch:
				return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;

			default:
				return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
			}
		}

		D3D_PRIMITIVE_TOPOLOGY ConvertPrimitiveTopology(ngl::rhi::PrimitiveTopology v)
		{
			switch (v)
			{
			case PrimitiveTopology::PointList:
				return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
			case PrimitiveTopology::LineList:
				return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
			case PrimitiveTopology::LineStrip:
				return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
			case PrimitiveTopology::TriangleList:
				return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			case PrimitiveTopology::TriangleStrip:
				return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
			default:
				assert(false);
				return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
			}
		}

		D3D12_FILTER ConvertTextureFilter(TextureFilterMode v)
		{
			static constexpr D3D12_FILTER table[] =
			{
				D3D12_FILTER_MIN_MAG_MIP_POINT,
				D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR,
				D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT,
				D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR,
				D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT,
				D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
				D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT,
				D3D12_FILTER_MIN_MAG_MIP_LINEAR,
				D3D12_FILTER_ANISOTROPIC,

				D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT,
				D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR,
				D3D12_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT,
				D3D12_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR,
				D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT,
				D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
				D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
				D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
				D3D12_FILTER_COMPARISON_ANISOTROPIC
			};
			assert(std::size(table) > static_cast<size_t>(v));
			return table[static_cast<size_t>(v)];
		}

		D3D12_TEXTURE_ADDRESS_MODE ConvertTextureAddressMode(TextureAddressMode v)
		{
			static constexpr D3D12_TEXTURE_ADDRESS_MODE table[] =
			{
				D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				D3D12_TEXTURE_ADDRESS_MODE_MIRROR,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				D3D12_TEXTURE_ADDRESS_MODE_BORDER,
				D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE
			};
			assert(std::size(table) > static_cast<size_t>(v));
			return table[static_cast<size_t>(v)];
		}

	}
}