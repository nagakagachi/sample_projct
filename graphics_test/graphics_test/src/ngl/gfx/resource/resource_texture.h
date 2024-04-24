#pragma once

#include <vector>

#include "ngl/math/math.h"
#include "ngl/util/noncopyable.h"
#include "ngl/util/singleton.h"


#include "ngl/rhi/d3d12/device.d3d12.h"
#include "ngl/rhi/d3d12/resource.d3d12.h"
#include "ngl/rhi/d3d12/resource_view.d3d12.h"
#include "ngl/rhi/d3d12/command_list.d3d12.h"

#include "ngl/resource/resource.h"


namespace ngl
{
	namespace res
	{
		class IResourceRenderUpdater;
	}
	
	namespace gfx
	{
		class ResTexture : public res::Resource
		{
			NGL_RES_MEMBER_DECLARE(ResTextureData)

		public:
			struct LoadDesc
			{
				int dummy;
			};
			
			ResTexture()
			{
			}
			~ResTexture()
			{
				std::cout << "[ResTextureData] Destruct " << this << std::endl;
			}

			bool IsNeedRenderUpdate() const override { return true; }
			void OnResourceRenderUpdate(rhi::DeviceDep* p_device, rhi::GraphicsCommandListDep* p_commandlist) override;

			// 読み込んだイメージから生成したTextureやそのView等.
			rhi::RefTextureDep			ref_texture_ = {};
			rhi::RefSrvDep				ref_view_ = {};

			
			// Upload data.
			std::vector<u8> upload_pixel_memory_ = {};
			std::vector<rhi::TextureUploadSubresourceInfo> upload_subresource_info_array;
		};
	}
}
