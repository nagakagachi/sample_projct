#pragma once

#include "ngl/rhi/rhi.h"

namespace ngl
{
	namespace rhi
	{
		enum class TextureType : u32
		{
			Texture1D,              ///< 1D texture. Can be bound as render-target, shader-resource and UAV
			Texture2D,              ///< 2D texture. Can be bound as render-target, shader-resource and UAV
			Texture3D,              ///< 3D texture. Can be bound as render-target, shader-resource and UAV
			TextureCube,            ///< Texture-cube. Can be bound as render-target, shader-resource and UAV
			//Texture2DMultisample,   ///< 2D multi-sampled texture. Can be bound as render-target, shader-resource and UAV
		};

		struct ResourceBindFlag
		{
			static constexpr u32 None = (1 << 0);///< The resource will not be bound the pipeline. Use this to create a staging resource
			static constexpr u32 ConstantBuffer = (1 << 1);///< The resource will be bound as a constant-buffer
			static constexpr u32 VertexBuffer = (1 << 2);///< The resource will be bound as a vertex-buffer
			static constexpr u32 IndexBuffer = (1 << 3);///< The resource will be bound as a index-buffer
			static constexpr u32 ShaderResource = (1 << 4);///< The resource will be bound as a shader-resource
			static constexpr u32 UnorderedAccess = (1 << 5);///< The resource will be bound as an UAV
			static constexpr u32 RenderTarget = (1 << 6);///< The resource will be bound as a render-target
			static constexpr u32 DepthStencil = (1 << 7);///< The resource will be bound as a depth-stencil buffer
			static constexpr u32 IndirectArg = (1 << 8);///< The resource will be bound as an indirect argument buffer
			//static constexpr u32 AccelerationStructure					= 0x80000000,  ///< The resource will be bound as an acceleration structure
		};

	}
}