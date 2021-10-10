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
		class TextureDep;
		class RenderTargetViewDep;
		class DepthStencilViewDep;

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

			void SetRenderTargets(const RenderTargetViewDep** pp_rtv, int num_rtv, const DepthStencilViewDep* p_dsv);

			void SetViewports(u32 num, const  D3D12_VIEWPORT* p_viewports);
			void SetScissor(u32 num, const  D3D12_RECT* p_rects);

			void SetPipelineState(GraphicsPipelineStateDep* p_pso);
			void SetDescriptorSet(const GraphicsPipelineStateDep* p_pso, const DescriptorSetDep* p_desc_set);

			void SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY topology);
			void SetVertexBuffers(u32 slot, u32 num, const D3D12_VERTEX_BUFFER_VIEW* p_views);
			void SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW* p_view);

			void DrawInstanced(u32 num_vtx, u32 num_instance, u32 offset_vtx, u32 offset_instance);
			void DrawIndexedInstanced(u32 index_count_per_instance, u32 instance_count, u32 start_index_location, s32  base_vertex_location, u32 start_instance_location);



			void ClearRenderTarget(const RenderTargetViewDep* p_rtv, float(color)[4]);
			void ClearDepthTarget(const DepthStencilViewDep* p_dsv, float depth, uint8_t stencil, bool clearDepth, bool clearStencil);

			// Barrier
			void ResourceBarrier(SwapChainDep* p_swapchain, unsigned int buffer_index, ResourceState prev, ResourceState next);
			void ResourceBarrier(TextureDep* p_texture, ResourceState prev, ResourceState next);

		public:
			// 検証中は直接利用するかもしれないので取得関数追加
			FrameDescriptorInterface* GetFrameDescriptorInterface() { return &frame_desc_interface_; }

		private:
			DeviceDep* parent_device_	= nullptr;
			Desc		desc_ = {};

			// Cvb Srv Uav用.
			FrameDescriptorInterface			frame_desc_interface_ = {};
			// Sampler用.
			FrameDescriptorHeapPageInterface	frame_desc_page_interface_for_sampler_ = {};

			CComPtr<ID3D12CommandAllocator>		p_command_allocator_;
			CComPtr<ID3D12GraphicsCommandList>	p_command_list_;
		};
	}
}