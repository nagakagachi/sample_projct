

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
		void GraphicsCommandListDep::SetViewports(u32 num, const  D3D12_VIEWPORT* p_viewports)
		{
			assert(p_viewports);
			assert(num);
			p_command_list_->RSSetViewports( num, p_viewports );
		}
		void GraphicsCommandListDep::SetScissor(u32 num, const  D3D12_RECT* p_rects)
		{
			assert(p_rects);
			assert(num);
			p_command_list_->RSSetScissorRects(num, p_rects);
		}
		void GraphicsCommandListDep::SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY topology)
		{
			p_command_list_->IASetPrimitiveTopology(topology);
		}
		void GraphicsCommandListDep::SetVertexBuffers(u32 slot, u32 num, const D3D12_VERTEX_BUFFER_VIEW* p_views)
		{
			p_command_list_->IASetVertexBuffers( slot, num, p_views );
		}
		void GraphicsCommandListDep::SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW* p_view)
		{
			p_command_list_->IASetIndexBuffer(p_view);
		}
		void GraphicsCommandListDep::DrawInstanced(u32 num_vtx, u32 num_instance, u32 offset_vtx, u32 offset_instance)
		{
			p_command_list_->DrawInstanced(num_vtx, num_instance, offset_vtx, offset_instance);
		}
		void GraphicsCommandListDep::DrawIndexedInstanced(u32 index_count_per_instance, u32 instance_count, u32 start_index_location, s32  base_vertex_location, u32 start_instance_location)
		{
			p_command_list_->DrawIndexedInstanced(index_count_per_instance, instance_count, start_index_location, base_vertex_location, start_instance_location);
		}
		void GraphicsCommandListDep::SetPipelineState(GraphicsPipelineStateDep* pso)
		{
			p_command_list_->SetPipelineState(pso->GetD3D12PipelineState());
			p_command_list_->SetGraphicsRootSignature(pso->GetD3D12RootSignature());
		}
		void GraphicsCommandListDep::SetDescriptorSet(const GraphicsPipelineStateDep* p_pso, const DescriptorSetDep* p_desc_set)
		{
			assert(p_pso);
			assert(p_desc_set);

			// cbv, srv, uav用デフォルトDescriptor取得.
			auto def_descriptor = parent_device_->GetPersistentDescriptorAllocator()->GetDefaultPersistentDescriptor();
			
			auto resource_layout = p_pso->GetPipelineResourceViewLayout();
			auto&& resource_table =  resource_layout->GetResourceTable();



			// Heapの設定. あとでSampler設定も追加するのでそのときには2つのHeapをここでセット
			ID3D12DescriptorHeap* heaps[] =
			{
				frame_desc_interface_.GetFrameDescriptorManager()->GetD3D12DescriptorHeap(),
			};
			p_command_list_->SetDescriptorHeaps(static_cast<UINT>(std::size(heaps)), heaps);


			// CBV, SRV, UAVの登録
			D3D12_CPU_DESCRIPTOR_HANDLE tmp[k_srv_table_size];
			auto SetViewDesc = [&](u32 count, const D3D12_CPU_DESCRIPTOR_HANDLE* handles, u8 table_index)
			{
				if (0 > table_index)
					return;

				if (0 < count)
				{
					for (u32 i = 0; i < count; i++)
					{
						tmp[i] = (handles[i].ptr > 0) ? handles[i] : def_descriptor.cpu_handle;
					}

					D3D12_CPU_DESCRIPTOR_HANDLE dst_cpu;
					D3D12_GPU_DESCRIPTOR_HANDLE dst_gpu;
					frame_desc_interface_.Allocate(count, dst_cpu, dst_gpu);

					// FrameDescriptorHeapから連続したDescriptorを確保してコピー,CommandListへセットする.
					parent_device_->GetD3D12Device()->CopyDescriptors(
						1, &dst_cpu, &count,
						count, tmp, nullptr,
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					p_command_list_->SetGraphicsRootDescriptorTable(table_index, dst_gpu);
				}
			};
			// 各ステージの各リソースタイプ別に連続Descriptorを確保,コピーしてテーブルにをセットしていく
			// 各ステージ毎各リソースタイプ毎に0番から設定された最大レジスタ番号までの範囲でFrameDescriptorから確保してコピー,CommandListへ設定する.
			SetViewDesc(p_desc_set->GetVsCbv().max_use_register_index + 1, p_desc_set->GetVsCbv().cpu_handles, resource_table.vs_cbv_table);
			SetViewDesc(p_desc_set->GetVsSrv().max_use_register_index + 1, p_desc_set->GetVsSrv().cpu_handles, resource_table.vs_srv_table);

			SetViewDesc(p_desc_set->GetPsCbv().max_use_register_index + 1, p_desc_set->GetPsCbv().cpu_handles, resource_table.ps_cbv_table);
			SetViewDesc(p_desc_set->GetPsSrv().max_use_register_index + 1, p_desc_set->GetPsSrv().cpu_handles, resource_table.ps_srv_table);
			SetViewDesc(p_desc_set->GetPsUav().max_use_register_index + 1, p_desc_set->GetPsUav().cpu_handles, resource_table.ps_uav_table);

			SetViewDesc(p_desc_set->GetGsCbv().max_use_register_index + 1, p_desc_set->GetGsCbv().cpu_handles, resource_table.gs_cbv_table);
			SetViewDesc(p_desc_set->GetGsSrv().max_use_register_index + 1, p_desc_set->GetGsSrv().cpu_handles, resource_table.gs_srv_table);

			SetViewDesc(p_desc_set->GetHsCbv().max_use_register_index + 1, p_desc_set->GetHsCbv().cpu_handles, resource_table.hs_cbv_table);
			SetViewDesc(p_desc_set->GetHsSrv().max_use_register_index + 1, p_desc_set->GetHsSrv().cpu_handles, resource_table.hs_srv_table);

			SetViewDesc(p_desc_set->GetDsCbv().max_use_register_index + 1, p_desc_set->GetDsCbv().cpu_handles, resource_table.ds_cbv_table);
			SetViewDesc(p_desc_set->GetDsSrv().max_use_register_index + 1, p_desc_set->GetDsSrv().cpu_handles, resource_table.ds_srv_table);
		}
		// -------------------------------------------------------------------------------------------------------------------------------------------------

	}
}