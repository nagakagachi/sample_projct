#pragma once

#include <iostream>
#include <vector>

#include "ngl/util/types.h"


#include "ngl/rhi/rhi.h"
#include "rhi.d3d12.h"
#include "rhi_util.d3d12.h"
#include "rhi_descriptor.d3d12.h"


namespace ngl
{
	namespace rhi
	{
		// CommandList
		class GraphicsCommandListDep
		{
		public:
			struct Desc
			{
				u32		dummy = 0;
			};

			GraphicsCommandListDep();
			~GraphicsCommandListDep();

			bool Initialize(DeviceDep* p_device, const Desc& desc);
			void Finalize();

			ID3D12GraphicsCommandList* GetD3D12GraphicsCommandList()
			{
				return p_command_list_;
			}

			void Begin();
			void End();

			void SetRenderTargetSingle(RenderTargetViewDep* p_rtv);
			void ClearRenderTarget(RenderTargetViewDep* p_rtv, float(color)[4]);

			// SwapChainに対してBarrier
			void ResourceBarrier(SwapChainDep* p_swapchain, unsigned int buffer_index, ResourceState prev, ResourceState next);
		private:
			DeviceDep* parent_device_	= nullptr;
			Desc		desc_ = {};

			FrameDescriptorInterface frame_desc_interface_ = {};

			CComPtr<ID3D12CommandAllocator>		p_command_allocator_;
			CComPtr<ID3D12GraphicsCommandList>	p_command_list_;
		};
	}
}