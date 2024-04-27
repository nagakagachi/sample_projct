﻿#pragma once

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
			enum ECreateMode
			{
				FROM_FILE,			// resourceファイルのロードで生成.
				FROM_DESC			// descに指定した設定とメモリから生成.
			};
			struct FromDescData
			{
				rhi::ETextureType		type = rhi::ETextureType::Texture2D;
				rhi::EResourceFormat	format = rhi::EResourceFormat::Format_UNKNOWN;
				ngl::u32				width = 1;
				ngl::u32				height = 1;
				ngl::u32				depth = 1;
				ngl::u32				mip_count = 1;
				ngl::u32				array_size = 1;
				
				std::vector<u8>									upload_pixel_memory_ = {};
				std::vector<rhi::TextureUploadSubresourceInfo>	upload_subresource_info_array = {};
			};
			struct LoadDesc
			{
				ECreateMode		mode = ECreateMode::FROM_FILE;// default.

				FromDescData	from_desc = {}; // for FROM_DESC.
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
