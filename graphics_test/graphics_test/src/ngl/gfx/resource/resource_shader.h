#pragma once

#include <vector>

#include "ngl/math/math.h"
#include "ngl/util/noncopyable.h"
#include "ngl/util/singleton.h"


#include "ngl/rhi/d3d12/device.d3d12.h"
#include "ngl/rhi/d3d12/shader.d3d12.h"

#include "ngl/resource/resource.h"

namespace ngl
{
	namespace res
	{
		class IResourceRenderUpdater;
	}


	namespace gfx
	{
		// Mesh Resource 実装.
		class ResShader : public res::Resource
		{
			NGL_RES_MEMBER_DECLARE(ResShader)

		public:
			struct LoadDesc
			{
				const char* entry_point_name = nullptr;
				// シェーダステージ.
				rhi::EShaderStage		stage = rhi::EShaderStage::Vertex;
				// シェーダモデル文字列.
				// "4_0", "5_0", "5_1" etc.
				const char* shader_model_version = nullptr;
			};

			ResShader()
			{
			}
			~ResShader()
			{
#if defined(_DEBUG)
				//std::cout << "[ResShader] Destruct " << this << std::endl;
#endif
			}

			void OnResourceRenderUpdate(rhi::DeviceDep* p_device, rhi::GraphicsCommandListDep* p_commandlist) override
			{
			}

			rhi::ShaderDep data_ = {};
		};
	}
}
