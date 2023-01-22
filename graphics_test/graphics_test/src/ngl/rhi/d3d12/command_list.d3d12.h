#pragma once

#include <iostream>
#include <vector>

#include "ngl/util/types.h"


#include "ngl/rhi/rhi.h"

#include "device.d3d12.h"
#include "shader.d3d12.h"
#include "rhi_util.d3d12.h"
#include "descriptor.d3d12.h"


namespace ngl
{
	namespace rhi
	{
		class TextureDep;
		class BufferDep;
		class RenderTargetViewDep;
		class DepthStencilViewDep;

		// CommandList
		class GraphicsCommandListDep : public RhiObjectBase
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

			// CommandListの標準Interfaceを取得.
			ID3D12GraphicsCommandList* GetD3D12GraphicsCommandList()
			{
				return p_command_list_;
			}
			// CommandListのDxr対応Interfaceを取得.
			ID3D12GraphicsCommandList4* GetD3D12GraphicsCommandListForDxr()
			{
				return p_command_list4_;
			}

			void Begin();
			void End();

			void SetRenderTargets(const RenderTargetViewDep** pp_rtv, int num_rtv, const DepthStencilViewDep* p_dsv);

			void SetViewports(u32 num, const  D3D12_VIEWPORT* p_viewports);
			void SetScissor(u32 num, const  D3D12_RECT* p_rects);

			void SetPipelineState(GraphicsPipelineStateDep* p_pso);
			void SetDescriptorSet(const GraphicsPipelineStateDep* p_pso, const DescriptorSetDep* p_desc_set);

			void SetPipelineState(ComputePipelineStateDep* p_pso);
			void SetDescriptorSet(const ComputePipelineStateDep* p_pso, const DescriptorSetDep* p_desc_set);

			void SetPrimitiveTopology(PrimitiveTopology topology);
			void SetVertexBuffers(u32 slot, u32 num, const D3D12_VERTEX_BUFFER_VIEW* p_views);
			void SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW* p_view);

			void DrawInstanced(u32 num_vtx, u32 num_instance, u32 offset_vtx, u32 offset_instance);
			void DrawIndexedInstanced(u32 index_count_per_instance, u32 instance_count, u32 start_index_location, s32  base_vertex_location, u32 start_instance_location);

			void Dispatch(u32 x, u32 y, u32 z);

			void ClearRenderTarget(const RenderTargetViewDep* p_rtv, float(color)[4]);
			void ClearDepthTarget(const DepthStencilViewDep* p_dsv, float depth, uint8_t stencil, bool clearDepth, bool clearStencil);

			// Barrier Swapchain, Texture, Buffer.
			void ResourceBarrier(SwapChainDep* p_swapchain, unsigned int buffer_index, ResourceState prev, ResourceState next);
			void ResourceBarrier(TextureDep* p_texture, ResourceState prev, ResourceState next);
			void ResourceBarrier(BufferDep* p_buffer, ResourceState prev, ResourceState next);
			// UAV同期Barrier.
			void ResourceUavBarrier(BufferDep* p_buffer);


		public:
			FrameCommandListDynamicDescriptorAllocatorInterface* GetFrameDescriptorInterface() { return &frame_desc_interface_; }
			FrameDescriptorHeapPageInterface* GetFrameSamplerDescriptorHeapInterface() { return &frame_desc_page_interface_for_sampler_; }

			DeviceDep* GetDevice() { return parent_device_; }

		private:
			DeviceDep* parent_device_	= nullptr;
			Desc		desc_ = {};

			// Cvb Srv Uav用.
			FrameCommandListDynamicDescriptorAllocatorInterface	frame_desc_interface_ = {};
			// Sampler用.
			FrameDescriptorHeapPageInterface	frame_desc_page_interface_for_sampler_ = {};

			CComPtr<ID3D12CommandAllocator>		p_command_allocator_;

			CComPtr<ID3D12GraphicsCommandList>	p_command_list_;
			// For Dxr Interface.
			CComPtr<ID3D12GraphicsCommandList4>	p_command_list4_;
		};
	}
}