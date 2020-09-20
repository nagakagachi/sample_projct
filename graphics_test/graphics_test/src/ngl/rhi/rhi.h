#pragma once

#include <iostream>
#include <vector>

#include "ngl/util/types.h"


namespace ngl
{
	namespace rhi
	{

		enum class ResourceFormat
		{
			NGL_FORMAT_UNKNOWN,
			NGL_FORMAT_R32G32B32A32_TYPELESS,
			NGL_FORMAT_R32G32B32A32_FLOAT,
			NGL_FORMAT_R32G32B32A32_UINT,
			NGL_FORMAT_R32G32B32A32_SINT,
			NGL_FORMAT_R32G32B32_TYPELESS,
			NGL_FORMAT_R32G32B32_FLOAT,
			NGL_FORMAT_R32G32B32_UINT,
			NGL_FORMAT_R32G32B32_SINT,
			NGL_FORMAT_R16G16B16A16_TYPELESS,
			NGL_FORMAT_R16G16B16A16_FLOAT,
			NGL_FORMAT_R16G16B16A16_UNORM,
			NGL_FORMAT_R16G16B16A16_UINT,
			NGL_FORMAT_R16G16B16A16_SNORM,
			NGL_FORMAT_R16G16B16A16_SINT,
			NGL_FORMAT_R32G32_TYPELESS,
			NGL_FORMAT_R32G32_FLOAT,
			NGL_FORMAT_R32G32_UINT,
			NGL_FORMAT_R32G32_SINT,
			NGL_FORMAT_R32G8X24_TYPELESS,
			NGL_FORMAT_D32_FLOAT_S8X24_UINT,
			NGL_FORMAT_R32_FLOAT_X8X24_TYPELESS,
			NGL_FORMAT_X32_TYPELESS_G8X24_UINT,
			NGL_FORMAT_R10G10B10A2_TYPELESS,
			NGL_FORMAT_R10G10B10A2_UNORM,
			NGL_FORMAT_R10G10B10A2_UINT,
			NGL_FORMAT_R11G11B10_FLOAT,
			NGL_FORMAT_R8G8B8A8_TYPELESS,
			NGL_FORMAT_R8G8B8A8_UNORM,
			NGL_FORMAT_R8G8B8A8_UNORM_SRGB,
			NGL_FORMAT_R8G8B8A8_UINT,
			NGL_FORMAT_R8G8B8A8_SNORM,
			NGL_FORMAT_R8G8B8A8_SINT,
			NGL_FORMAT_R16G16_TYPELESS,
			NGL_FORMAT_R16G16_FLOAT,
			NGL_FORMAT_R16G16_UNORM,
			NGL_FORMAT_R16G16_UINT,
			NGL_FORMAT_R16G16_SNORM,
			NGL_FORMAT_R16G16_SINT,
			NGL_FORMAT_R32_TYPELESS,
			NGL_FORMAT_D32_FLOAT,
			NGL_FORMAT_R32_FLOAT,
			NGL_FORMAT_R32_UINT,
			NGL_FORMAT_R32_SINT,
			NGL_FORMAT_R24G8_TYPELESS,
			NGL_FORMAT_D24_UNORM_S8_UINT,
			NGL_FORMAT_R24_UNORM_X8_TYPELESS,
			NGL_FORMAT_X24_TYPELESS_G8_UINT,
			NGL_FORMAT_R8G8_TYPELESS,
			NGL_FORMAT_R8G8_UNORM,
			NGL_FORMAT_R8G8_UINT,
			NGL_FORMAT_R8G8_SNORM,
			NGL_FORMAT_R8G8_SINT,
			NGL_FORMAT_R16_TYPELESS,
			NGL_FORMAT_R16_FLOAT,
			NGL_FORMAT_D16_UNORM,
			NGL_FORMAT_R16_UNORM,
			NGL_FORMAT_R16_UINT,
			NGL_FORMAT_R16_SNORM,
			NGL_FORMAT_R16_SINT,
			NGL_FORMAT_R8_TYPELESS,
			NGL_FORMAT_R8_UNORM,
			NGL_FORMAT_R8_UINT,
			NGL_FORMAT_R8_SNORM,
			NGL_FORMAT_R8_SINT,
			NGL_FORMAT_A8_UNORM,
			NGL_FORMAT_R1_UNORM,
			NGL_FORMAT_R9G9B9E5_SHAREDEXP,
			NGL_FORMAT_R8G8_B8G8_UNORM,
			NGL_FORMAT_G8R8_G8B8_UNORM,
			NGL_FORMAT_BC1_TYPELESS,
			NGL_FORMAT_BC1_UNORM,
			NGL_FORMAT_BC1_UNORM_SRGB,
			NGL_FORMAT_BC2_TYPELESS,
			NGL_FORMAT_BC2_UNORM,
			NGL_FORMAT_BC2_UNORM_SRGB,
			NGL_FORMAT_BC3_TYPELESS,
			NGL_FORMAT_BC3_UNORM,
			NGL_FORMAT_BC3_UNORM_SRGB,
			NGL_FORMAT_BC4_TYPELESS,
			NGL_FORMAT_BC4_UNORM,
			NGL_FORMAT_BC4_SNORM,
			NGL_FORMAT_BC5_TYPELESS,
			NGL_FORMAT_BC5_UNORM,
			NGL_FORMAT_BC5_SNORM,
			NGL_FORMAT_B5G6R5_UNORM,
			NGL_FORMAT_B5G5R5A1_UNORM,
			NGL_FORMAT_B8G8R8A8_UNORM,
			NGL_FORMAT_B8G8R8X8_UNORM,
			NGL_FORMAT_R10G10B10_XR_BIAS_A2_UNORM,
			NGL_FORMAT_B8G8R8A8_TYPELESS,
			NGL_FORMAT_B8G8R8A8_UNORM_SRGB,
			NGL_FORMAT_B8G8R8X8_TYPELESS,
			NGL_FORMAT_B8G8R8X8_UNORM_SRGB,
			NGL_FORMAT_BC6H_TYPELESS,
			NGL_FORMAT_BC6H_UF16,
			NGL_FORMAT_BC6H_SF16,
			NGL_FORMAT_BC7_TYPELESS,
			NGL_FORMAT_BC7_UNORM,
			NGL_FORMAT_BC7_UNORM_SRGB,
			NGL_FORMAT_AYUV,
			NGL_FORMAT_Y410,
			NGL_FORMAT_Y416,
			NGL_FORMAT_NV12,
			NGL_FORMAT_P010,
			NGL_FORMAT_P016,
			NGL_FORMAT_420_OPAQUE,
			NGL_FORMAT_YUY2,
			NGL_FORMAT_Y210,
			NGL_FORMAT_Y216,
			NGL_FORMAT_NV11,
			NGL_FORMAT_AI44,
			NGL_FORMAT_IA44,
			NGL_FORMAT_P8,
			NGL_FORMAT_A8P8,
			NGL_FORMAT_B4G4R4A4_UNORM,
			_MAX
		};

		enum class ResourceState
		{
			COMMON,
			GENERAL,
			CONSTANT_BUFFER,
			VERTEX_BUFFER,
			INDEX_BUFFER,
			RENDER_TARGET,
			SHADER_READ,
			UNORDERED_ACCESS,
			DEPTH_WRITE,
			DEPTH_READ,
			INDIRECT_ARGUMENT,
			COPY_DST,
			COPY_SRC,
			PRESENT,
		};


		enum class ResourceHeapType
		{
			DEFAULT,
			UPLOAD,
			READBACK,
		};

		enum class ShaderStage
		{
			VERTEX,
			PIXEL,
			MESH,			// 未実装
			AMPLIFICATION,	// 未実装
		};

		// ブレンド要素
		enum class BlendFactor
		{
			BLEND_FACTOR_ZERO,
			BLEND_FACTOR_ONE,
			BLEND_FACTOR_SRC_COLOR,
			BLEND_FACTOR_INV_SRC_COLOR,
			BLEND_FACTOR_SRC_ALPHA,
			BLEND_FACTOR_INV_SRC_ALPHA,
			BLEND_FACTOR_DEST_ALPHA,
			BLEND_FACTOR_INV_DEST_ALPHA,
			BLEND_FACTOR_DEST_COLOR,
			BLEND_FACTOR_INV_DEST_COLOR,
			BLEND_FACTOR_SRC_ALPHA_SAT,
			BLEND_FACTOR_BLEND_FACTOR,
			BLEND_FACTOR_INV_BLEND_FACTOR,
			BLEND_FACTOR_SRC1_COLOR,
			BLEND_FACTOR_INV_SRC1_COLOR,
			BLEND_FACTOR_SRC1_ALPHA,
			BLEND_FACTOR_INV_SRC1_ALPHA,
		};	// enum BlendFactor

		// ブレンド関数
		enum class BlendOp
		{
			BLEND_OP_ADD,
			BLEND_OP_SUBTRACT,
			BLEND_OP_REV_SUBTRACT,
			BLEND_OP_MIN,
			BLEND_OP_MAX,
		};	// enum BlendOp

		// 比較関数
		enum class CompFunc
		{
			COMP_FUNC_NEVER,
			COMP_FUNC_LESS,
			COMP_FUNC_EQUAL,
			COMP_FUNC_LESS_EQUAL,
			COMP_FUNC_GREATER,
			COMP_FUNC_NOT_EQUAL,
			COMP_FUNC_GREATER_EQUAL,
			COMP_FUNC_ALWAYS,
		};	// enum CompFunc

		// ステンシル関数
		enum class StencilOp
		{
			STENCIL_OP_KEEP,
			STENCIL_OP_ZERO,
			STENCIL_OP_REPLACE,
			STENCIL_OP_INCR_SAT,
			STENCIL_OP_DECR_SAT,
			STENCIL_OP_INVERT,
			STENCIL_OP_INCR,
			STENCIL_OP_DECR,
		};	// enum StencilOp

		// ステンシル面方向
		enum class StencilFace
		{
			STENCIL_FACE_FRONT,
			STENCIL_FACE_BACK,

			STENCIL_FACE_MAX
		};	// enum StencilFace

		// ポリゴン描画モード
		enum class FillMode
		{
			FILL_WIREFRAME,
			FILL_SOLID,
		};	// enum FillMode

		// カリング面
		enum class CullingMode
		{
			CULL_NONE,
			CULL_FRONT,
			CULL_BACK,
		};	// enum CullingMode

		// テクスチャフィルタ
		enum class TexFilter
		{
			FILTER_MIN_MAG_MIP_POINT,
			FILTER_MIN_MAG_POINT_MIP_LINEAR,
			FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT,
			FILTER_MIN_POINT_MAG_MIP_LINEAR,
			FILTER_MIN_LINEAR_MAG_MIP_POINT,
			FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
			FILTER_MIN_MAG_LINEAR_MIP_POINT,
			FILTER_MIN_MAG_MIP_LINEAR,
			FILTER_ANISOTROPIC,
			FILTER_COMP_MIN_MAG_MIP_POINT,
			FILTER_COMP_MIN_MAG_POINT_MIP_LINEAR,
			FILTER_COMP_MIN_POINT_MAG_LINEAR_MIP_POINT,
			FILTER_COMP_MIN_POINT_MAG_MIP_LINEAR,
			FILTER_COMP_MIN_LINEAR_MAG_MIP_POINT,
			FILTER_COMP_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
			FILTER_COMP_MIN_MAG_LINEAR_MIP_POINT,
			FILTER_COMP_MIN_MAG_MIP_LINEAR,
			FILTER_COMP_ANISOTROPIC,
		};	// enum TexFilter

		// テクスチャラップモード
		enum class TexWrap
		{
			TEXTURE_WRAP_REPEAT,
			TEXTURE_WRAP_MIRROR,
			TEXTURE_WRAP_CLAMP,
			TEXTURE_WRAP_BORDER,
			TEXTURE_WRAP_MIRROR_ONCE,
		};	// enum TexWrap

		// 描画トポロジー
		enum class PrimitiveTopologyType
		{
			PRIMITIVE_TOPOLOGY_POINT,
			PRIMITIVE_TOPOLOGY_LINE,
			PRIMITIVE_TOPOLOGY_TRIANGLE,
			PRIMITIVE_TOPOLOGY_PATCH,
		};	// enum PrimitiveTopologyType


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
			FillMode		fill_mode = FillMode::FILL_SOLID;
			CullingMode		cull_mode = CullingMode::CULL_BACK;
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
			StencilOp		stencil_fail_op;
			StencilOp		stencil_depth_fail_op;
			StencilOp		stencil_pass_op;
			CompFunc		stencil_func;
		};

		struct DepthStencilState 
		{
			bool					depth_enable;
			u32						depth_write_mask;		// D3D12_DEPTH_WRITE_MASK 
			CompFunc				depth_func;
			bool					stencil_enable;
			u8						stencil_read_mask;
			u8						stencil_write_mask;
			DepthStencilOp			front_face;
			DepthStencilOp			back_face;
		};

		struct StreamOutputDesc
		{
			// dummy
			u32				num_entries;
		};

		struct InputElement
		{
			const char*		semantic_name;
			u32				semantic_index;
			// 現状はD3Dのものを利用. 必要に応じて抽象化.
			ResourceFormat	format;
			u32			 stream_slot;
			// slotの1頂点情報内の対応する情報までのByteOffset
			u32			 element_offset;
		};

		struct InputLayout
		{
			const InputElement*		p_input_elements;
			u32						num_elements;
		};

	}
}