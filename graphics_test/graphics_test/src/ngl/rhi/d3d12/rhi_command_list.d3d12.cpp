

#include "rhi_command_list.d3d12.h"

namespace ngl
{
	namespace rhi
	{
		// -------------------------------------------------------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------------------------------------------------------
		GraphicsCommandListDep::GraphicsCommandListDep()
		{
		}
		GraphicsCommandListDep::~GraphicsCommandListDep()
		{
			Finalize();
		}
		bool GraphicsCommandListDep::Initialize(DeviceDep* p_device, const Desc& desc)
		{
			if (!p_device)
				return false;

			desc_ = desc;
			parent_device_ = p_device;

			// Command Allocator
			if (FAILED(p_device->GetD3D12Device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&p_command_allocator_))))
			{
				std::cout << "ERROR: Create Command Allocator" << std::endl;
				return false;
			}

			// Command List
			if (FAILED(p_device->GetD3D12Device()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, p_command_allocator_, nullptr, IID_PPV_ARGS(&p_command_list_))))
			{
				std::cout << "ERROR: Create Command List" << std::endl;
				return false;
			}

			// 初回クローズ. これがないと初回フレームの開始時ResetでComError発生.
			p_command_list_->Close();

			
			// フレームでのDescriptor確保用インターフェイス初期化
			FrameDescriptorInterface::Desc fdi_desc = {};
			fdi_desc.stack_size = 2000;// スタックサイズは適当.
			if (!frame_desc_interface_.Initialize(parent_device_->GetFrameDescriptorManager(), fdi_desc))
			{
				std::cout << "ERROR: Create FrameDescriptorInterface" << std::endl;
				return false;
			}

			return true;
		}
		void GraphicsCommandListDep::Finalize()
		{
			p_command_list_ = nullptr;
			p_command_allocator_ = nullptr;
		}

		void GraphicsCommandListDep::Begin()
		{
			// アロケータリセット
			p_command_allocator_->Reset();
			// コマンドリストリセット
			p_command_list_->Reset(p_command_allocator_, nullptr);

			// 新しいフレームのためのFrameDescriptorの準備.
			// インデックスはDeviceから取得するようにした.
			frame_desc_interface_.ReadyToNewFrame(parent_device_->GetFrameBufferIndex());


			// テスト
#ifdef _DEBUG
		//	D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle;
		//	D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle;
		//	frame_desc_interface_.Allocate(16, cpu_handle, gpu_handle);
#endif
		}
		void GraphicsCommandListDep::End()
		{
			p_command_list_->Close();
		}

		void GraphicsCommandListDep::SetRenderTargetSingle(RenderTargetViewDep* p_rtv)
		{
			auto rtv = p_rtv->GetD3D12DescriptorHandle();
			p_command_list_->OMSetRenderTargets(1, &rtv, true, nullptr);
		}
		void GraphicsCommandListDep::ClearRenderTarget(RenderTargetViewDep* p_rtv, float(color)[4])
		{
			auto rtv = p_rtv->GetD3D12DescriptorHandle();
			p_command_list_->ClearRenderTargetView(rtv, color, 0u, nullptr);
		}

		// バリア
		void GraphicsCommandListDep::ResourceBarrier(SwapChainDep* p_swapchain, unsigned int buffer_index, ResourceState prev, ResourceState next)
		{
			if (!p_swapchain)
				return;
			if (prev == next)
				return;
			auto* resource = p_swapchain->GetD3D12Resource(buffer_index);

			D3D12_RESOURCE_STATES state_before = ConvertResourceState(prev);
			D3D12_RESOURCE_STATES state_after = ConvertResourceState(next);

			D3D12_RESOURCE_BARRIER desc = {};
			desc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			desc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

			desc.Transition.pResource = resource;
			desc.Transition.StateBefore = state_before;
			desc.Transition.StateAfter = state_after;
			// 現状は全サブリソースを対象.
			desc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

			p_command_list_->ResourceBarrier(1, &desc);
		}
		void GraphicsCommandListDep::SetViewports(u32 num, const  D3D12_VIEWPORT* viewports)
		{
			assert(viewports);
			assert(num);
			p_command_list_->RSSetViewports( num, viewports );
		}
		void GraphicsCommandListDep::SetScissor(u32 num, const  D3D12_RECT* rects)
		{
			assert(rects);
			assert(num);
			p_command_list_->RSSetScissorRects(num, rects);
		}

		void GraphicsCommandListDep::SetPipelineState(GraphicsPipelineStateDep* pso)
		{
			p_command_list_->SetPipelineState(pso->GetD3D12PipelineState());
			p_command_list_->SetGraphicsRootSignature(pso->GetD3D12RootSignature());
		}
		void GraphicsCommandListDep::SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY topology)
		{
			p_command_list_->IASetPrimitiveTopology(topology);
		}
		void GraphicsCommandListDep::SetVertexBuffers(u32 slot, u32 num, const D3D12_VERTEX_BUFFER_VIEW* views)
		{
			p_command_list_->IASetVertexBuffers( slot, num, views );
		}
		void GraphicsCommandListDep::DrawInstanced(u32 num_vtx, u32 num_instance, u32 offset_vtx, u32 offset_instance)
		{
			p_command_list_->DrawInstanced(num_vtx, num_instance, offset_vtx, offset_instance);
		}
		// -------------------------------------------------------------------------------------------------------------------------------------------------

	}
}