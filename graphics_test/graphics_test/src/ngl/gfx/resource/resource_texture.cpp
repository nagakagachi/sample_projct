
#include "resource_texture.h"

#include "ngl/resource/resource_manager.h"

namespace ngl
{
	namespace gfx
	{
		void ResTextureData::OnResourceRenderUpdate(rhi::DeviceDep* p_device, rhi::GraphicsCommandListDep* p_commandlist)
		{
			auto* p_d3d_commandlist = p_commandlist->GetD3D12GraphicsCommandList();

			// TODO. Texture Uploadが必要.



			// ここでコピーした場合はUpload用のピクセルデータは不要なのでメモリ解放する(ただしCommandでCopyのSrcとしている場合はメモリがGPUCommand実行まで生存している必要があるので遅延破棄が必要).

			// 仮で解放.
			this->upload_image_plane_array_ = {};
			this->upload_pixel_memory_ = {};
			
			
		}
	}
}