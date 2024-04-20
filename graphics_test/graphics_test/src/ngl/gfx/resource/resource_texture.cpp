
#include "resource_texture.h"

#include "ngl/resource/resource_manager.h"

namespace ngl
{
	namespace gfx
	{
		void ResTextureData::OnResourceRenderUpdate(rhi::DeviceDep* p_device, rhi::GraphicsCommandListDep* p_commandlist)
		{
			assert(this->ref_texture_.IsValid());
			
			// TODO. テストのため 0番のみコピー.
			const int subresource_index = 0;

			
			// 一時Buffer生成.
			rhi::RefBufferDep temporal_upload_buffer = new rhi::BufferDep();
			{
				rhi::BufferDep::Desc upload_buffer_desc = {};
				{
					upload_buffer_desc.heap_type = rhi::EResourceHeapType::Upload;
					upload_buffer_desc.element_byte_size = rhi::align_to(upload_subresource_info_array[subresource_index].slicePitch, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
					upload_buffer_desc.element_count = 1;
				}

				if(!temporal_upload_buffer->Initialize(p_device, upload_buffer_desc))
				{
					assert(false);
				}
			}
			// 一時BufferへCopy.
			if(auto* mapped = temporal_upload_buffer->MapAs<u8>())
			{
				memcpy(mapped, upload_subresource_info_array[subresource_index].pixels, upload_subresource_info_array[subresource_index].slicePitch);
			}
			
			// Copy元Buffer上の情報.
			D3D12_PLACED_SUBRESOURCE_FOOTPRINT subresource_copy_src_footprint = {};
			{
				subresource_copy_src_footprint.Footprint.Format = rhi::ConvertResourceFormat(upload_subresource_info_array[subresource_index].format);
				subresource_copy_src_footprint.Footprint.Width = upload_subresource_info_array[subresource_index].width;
				subresource_copy_src_footprint.Footprint.Height = upload_subresource_info_array[subresource_index].height;
				subresource_copy_src_footprint.Footprint.Depth = 1;// Slice単位コピーのため 1.
				subresource_copy_src_footprint.Footprint.RowPitch = ngl::rhi::align_to(upload_subresource_info_array[subresource_index].rowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

				// Copy元のUploadBuffer上での先頭からのオフセット. temporal_upload_buffer をSlice毎に用意して使用するため先頭0指定.
				// Upload用のHeapをやりくりする場合はそのオフセットになる.
				subresource_copy_src_footprint.Offset = 0; 
			}


			// State Transition.
			{
				// Copy Dst.
				p_commandlist->ResourceBarrier(ref_texture_.Get(), ref_texture_->GetDesc().initial_state, rhi::EResourceState::CopyDst);
				
				p_commandlist->ResourceBarrier(temporal_upload_buffer.Get(), temporal_upload_buffer->GetDesc().initial_state, rhi::EResourceState::CopySrc);
			}

			// Copy Command.
			{
				D3D12_TEXTURE_COPY_LOCATION copy_location_src = {};
				{
					copy_location_src.pResource = temporal_upload_buffer->GetD3D12Resource();
					// Bufferの場合はFootprintで指定.
					copy_location_src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
					copy_location_src.PlacedFootprint = subresource_copy_src_footprint;
				}
				D3D12_TEXTURE_COPY_LOCATION copy_location_dst = {};
				{
					copy_location_dst.pResource = ref_texture_->GetD3D12Resource();
					// Textureの場合はSubresourceIndexで指定.
					copy_location_dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
					copy_location_dst.SubresourceIndex = subresource_index;
				}
				
				auto* p_d3d_commandlist = p_commandlist->GetD3D12GraphicsCommandList();
				p_d3d_commandlist->CopyTextureRegion(&copy_location_dst, 0, 0, 0, &copy_location_src, {});
			}

			// State Transition.
			{
				// 今後は読み取りリソースとして扱われるため ShaderRead.
				p_commandlist->ResourceBarrier(ref_texture_.Get(), rhi::EResourceState::CopyDst, rhi::EResourceState::ShaderRead);
				
				// 一応 Common に戻す.
				p_commandlist->ResourceBarrier(temporal_upload_buffer.Get(), rhi::EResourceState::CopySrc, temporal_upload_buffer->GetDesc().initial_state);
			}
			
			
			// CPU側メモリ解放.
			this->upload_subresource_info_array = {};
			this->upload_pixel_memory_ = {};
			
		}
	}
}