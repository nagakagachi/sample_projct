
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
			
		}
	}
}