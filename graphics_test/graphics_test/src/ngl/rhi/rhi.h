#pragma once

#include <iostream>
#include <vector>
#include <assert.h>

#include "ngl/util/types.h"


namespace ngl
{
	namespace rhi
	{
		template<typename T0, typename T1>
		static constexpr bool check_bits(T0 v0, T1 v1)
		{
			return 0 != (v0 & v1);
		}

		enum class ResourceFormat
		{
			Format_UNKNOWN,
			Format_R32G32B32A32_TYPELESS,
			Format_R32G32B32A32_FLOAT,
			Format_R32G32B32A32_UINT,
			Format_R32G32B32A32_SINT,
			Format_R32G32B32_TYPELESS,
			Format_R32G32B32_FLOAT,
			Format_R32G32B32_UINT,
			Format_R32G32B32_SINT,
			Format_R16G16B16A16_TYPELESS,
			Format_R16G16B16A16_FLOAT,
			Format_R16G16B16A16_UNORM,
			Format_R16G16B16A16_UINT,
			Format_R16G16B16A16_SNORM,
			Format_R16G16B16A16_SINT,
			Format_R32G32_TYPELESS,
			Format_R32G32_FLOAT,
			Format_R32G32_UINT,
			Format_R32G32_SINT,
			Format_R32G8X24_TYPELESS,
			Format_D32_FLOAT_S8X24_UINT,
			Format_R32_FLOAT_X8X24_TYPELESS,
			Format_X32_TYPELESS_G8X24_UINT,
			Format_R10G10B10A2_TYPELESS,
			Format_R10G10B10A2_UNORM,
			Format_R10G10B10A2_UINT,
			Format_R11G11B10_FLOAT,
			Format_R8G8B8A8_TYPELESS,
			Format_R8G8B8A8_UNORM,
			Format_R8G8B8A8_UNORM_SRGB,
			Format_R8G8B8A8_UINT,
			Format_R8G8B8A8_SNORM,
			Format_R8G8B8A8_SINT,
			Format_R16G16_TYPELESS,
			Format_R16G16_FLOAT,
			Format_R16G16_UNORM,
			Format_R16G16_UINT,
			Format_R16G16_SNORM,
			Format_R16G16_SINT,
			Format_R32_TYPELESS,
			Format_D32_FLOAT,
			Format_R32_FLOAT,
			Format_R32_UINT,
			Format_R32_SINT,
			Format_R24G8_TYPELESS,
			Format_D24_UNORM_S8_UINT,
			Format_R24_UNORM_X8_TYPELESS,
			Format_X24_TYPELESS_G8_UINT,
			Format_R8G8_TYPELESS,
			Format_R8G8_UNORM,
			Format_R8G8_UINT,
			Format_R8G8_SNORM,
			Format_R8G8_SINT,
			Format_R16_TYPELESS,
			Format_R16_FLOAT,
			Format_D16_UNORM,
			Format_R16_UNORM,
			Format_R16_UINT,
			Format_R16_SNORM,
			Format_R16_SINT,
			Format_R8_TYPELESS,
			Format_R8_UNORM,
			Format_R8_UINT,
			Format_R8_SNORM,
			Format_R8_SINT,
			Format_A8_UNORM,
			Format_R1_UNORM,
			Format_R9G9B9E5_SHAREDEXP,
			Format_R8G8_B8G8_UNORM,
			Format_G8R8_G8B8_UNORM,
			Format_BC1_TYPELESS,
			Format_BC1_UNORM,
			Format_BC1_UNORM_SRGB,
			Format_BC2_TYPELESS,
			Format_BC2_UNORM,
			Format_BC2_UNORM_SRGB,
			Format_BC3_TYPELESS,
			Format_BC3_UNORM,
			Format_BC3_UNORM_SRGB,
			Format_BC4_TYPELESS,
			Format_BC4_UNORM,
			Format_BC4_SNORM,
			Format_BC5_TYPELESS,
			Format_BC5_UNORM,
			Format_BC5_SNORM,
			Format_B5G6R5_UNORM,
			Format_B5G5R5A1_UNORM,
			Format_B8G8R8A8_UNORM,
			Format_B8G8R8X8_UNORM,
			Format_R10G10B10_XR_BIAS_A2_UNORM,
			Format_B8G8R8A8_TYPELESS,
			Format_B8G8R8A8_UNORM_SRGB,
			Format_B8G8R8X8_TYPELESS,
			Format_B8G8R8X8_UNORM_SRGB,
			Format_BC6H_TYPELESS,
			Format_BC6H_UF16,
			Format_BC6H_SF16,
			Format_BC7_TYPELESS,
			Format_BC7_UNORM,
			Format_BC7_UNORM_SRGB,
			Format_AYUV,
			Format_Y410,
			Format_Y416,
			Format_NV12,
			Format_P010,
			Format_P016,
			Format_420_OPAQUE,
			Format_YUY2,
			Format_Y210,
			Format_Y216,
			Format_NV11,
			Format_AI44,
			Format_IA44,
			Format_P8,
			Format_A8P8,
			Format_B4G4R4A4_UNORM,
			_MAX,
		};

		// DepthFormatをSrv用に変換. それ以外はそのまま返却.
		inline ResourceFormat depthToColorFormat(ResourceFormat format)
		{
			switch (format)
			{
			case ResourceFormat::Format_D16_UNORM:
				return ResourceFormat::Format_R16_UNORM;
			case ResourceFormat::Format_D24_UNORM_S8_UINT:
				return ResourceFormat::Format_R24_UNORM_X8_TYPELESS;
			case ResourceFormat::Format_D32_FLOAT:
				return ResourceFormat::Format_R32_FLOAT;
			case ResourceFormat::Format_D32_FLOAT_S8X24_UINT:
				assert(false);
				return ResourceFormat::Format_UNKNOWN;
			default:
				return format;
			}
		}
		// DepthFormatなら真
		inline bool isDepthFormat(ResourceFormat format)
		{
			switch (format)
			{
			case ResourceFormat::Format_D16_UNORM:
				return true;
			case ResourceFormat::Format_D24_UNORM_S8_UINT:
				return true;
			case ResourceFormat::Format_D32_FLOAT:
				return true;
			case ResourceFormat::Format_D32_FLOAT_S8X24_UINT:
				assert(false);
				return false;
			default:
				return false;
			}
		}

		enum class ResourceState
		{
			Common,
			General,
			ConstatnBuffer,
			VertexBuffer,
			IndexBuffer,
			RenderTarget,
			ShaderRead,
			UnorderedAccess,
			DepthWrite,
			DepthRead,
			IndirectArgument,
			CopyDst,
			CopySrc,
			Present,
		};


		enum class ResourceHeapType
		{
			Default,
			Upload,
			Readback,
		};


		enum class ShaderStage
		{
			Vertex,
			Hull,
			Domain,
			Geometry,
			Pixel,
			Compute,

			_Max,
		};

		// ブレンド要素
		enum class BlendFactor
		{
			Zero,
			One,
			SrcColor,
			InvSrcColor,
			SrcAlpha,
			InvSrcAlpha,
			DestAlpha,
			InvDestAlpha,
			DestColor,
			InvDestColor,
			SrcAlphaSat,
			BlendFactor,
			InvBlendFactor,
			Src1Color,
			InvSrc1Color,
			Src1Alpha,
			InvSrc1Alpha,
		};	// enum BlendFactor

		// ブレンド関数
		enum class BlendOp
		{
			Add,
			Subtract,
			RevSubtract,
			Min,
			Max,
		};	// enum BlendOp

		// 比較関数
		enum class CompFunc
		{
			Never,
			Less,
			Equal,
			LessEqual,
			Greater,
			NotEqual,
			GreaterEqual,
			Always,
		};	// enum CompFunc

		// ステンシル関数
		enum class StencilOp
		{
			Keep,
			Zero,
			Replace,
			IncrSat,
			DecrSat,
			Invert,
			Incr,
			Decr,
		};	// enum StencilOp

		// ステンシル面方向
		enum class StencilFace
		{
			Front,
			Back,

			_Max
		};	// enum StencilFace

		// ポリゴン描画モード
		enum class FillMode
		{
			Wireframe,
			Solid,
		};	// enum FillMode

		// カリング面
		enum class CullingMode
		{
			None,
			Front,
			Back,
		};	// enum CullingMode

		// テクスチャフィルタ
		enum class TextureFilterMode
		{
			Min_Point_Mag_Point_Mip_Point,
			Min_Point_Mag_Point_Mip_Linear,
			Min_Point_Mag_Linear_Mip_Point,
			Min_Point_Mag_Linear_Mip_Linear,

			Min_Linear_Mag_Point_Mip_Point,
			Min_Linear_Mag_Point_Mip_Linear,
			Min_Linear_Mag_Linear_Mip_Point,
			Min_Linear_Mag_Linear_Mip_Linear,

			Anisotropic,

			Comp_Min_Point_Mag_Point_Mip_Point,
			Comp_Min_Point_Mag_Point_Mip_Linear,
			Comp_Min_Point_Mag_Linear_Mip_Point,
			Comp_Min_Point_Mag_Linear_Mip_Linear,

			Comp_Min_Linear_Mag_Point_Mip_Point,
			Comp_Min_Linear_Mag_Point_Mip_Linear,
			Comp_Min_Linear_Mag_Linear_Mip_Point,
			Comp_Min_Linear_Mag_Linear_Mip_Linear,

			Comp_Anisotropic,
		};	// enum TextureFilterMode

		// テクスチャラップモード
		enum class TextureAddressMode
		{
			Repeat,
			Mirror,
			Clamp,
			Border,
			MirrorOnce,
		};	// enum TextureAddressMode

		// 描画トポロジータイプ
		enum class PrimitiveTopologyType
		{
			Point,
			Line,
			Triangle,
			Patch,
		};	// enum PrimitiveTopologyType

		// 描画トポロジー
		enum class PrimitiveTopology
		{
			Undefined,
			PointList,
			LineList,
			LineStrip,
			TriangleList,
			TriangleStrip
		};

		struct SampleDesc
		{
			u32 count = 1;
			u32 quality = 0;
		};

		// レンダーターゲットブレンドステート
		struct RenderTargetBlendState
		{
			bool			blend_enable;
			BlendFactor		src_color_blend;
			BlendFactor		dst_color_blend;
			BlendOp			color_op;
			BlendFactor		src_alpha_blend;
			BlendFactor		dst_alpha_blend;
			BlendOp			alpha_op;
			u8				write_mask;
		};

		// ブレンドステート
		struct BlendState
		{
			bool					alpha_to_coverage_enable = false;
			bool					independent_blend_enable = false;
			RenderTargetBlendState target_blend_states[8];
		};

		// ラスタライザステート
		struct RasterizerState
		{
			FillMode		fill_mode = FillMode::Solid;
			CullingMode		cull_mode = CullingMode::Back;
			bool			front_counter_clockwise = false;
			int				depth_bias = 0;
			float			depth_bias_clamp = 0.0f;
			float			slope_scale_depth_bias = 0.0f;
			bool			depth_clip_enable = true;
			bool			multi_sample_enable = false;
			bool			antialiased_line_enable = false;
			u32				force_sample_count = 0;
			bool			conservative_raster = false;
		};

		struct DepthStencilOp
		{
			StencilOp		stencil_fail_op = StencilOp::Keep;
			StencilOp		stencil_depth_fail_op = StencilOp::Keep;
			StencilOp		stencil_pass_op = StencilOp::Keep;
			CompFunc		stencil_func = CompFunc::Always;
		};

		struct DepthStencilState 
		{
			bool					depth_enable = false;
			u32						depth_write_mask = ~0u;		// D3D12_DEPTH_WRITE_MASK 
			CompFunc				depth_func = ngl::rhi::CompFunc::Always;
			bool					stencil_enable = false;
			u8						stencil_read_mask = u8(~0u);
			u8						stencil_write_mask = u8(~0u);
			DepthStencilOp			front_face = {};
			DepthStencilOp			back_face = {};
		};

		struct StreamOutputDesc
		{
			// dummy
			u32				num_entries;
		};

		struct InputElement
		{
			// セマンティック名. インデックス無し.
			const char*		semantic_name;
			// セマンティックインデックス.
			u32				semantic_index;
			// 現状はD3Dのものを利用. 必要に応じて抽象化.
			ResourceFormat	format;
			u32			 stream_slot;
			// slotの1頂点情報内の対応する情報までのByteOffset
			u32			 element_offset;
		};

		struct InputLayout
		{
			const InputElement*		p_input_elements = nullptr;
			u32						num_elements = 0;
		};

	}
}