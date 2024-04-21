
#include "resource_texture.h"

#include "ngl/resource/resource_manager.h"


#include <direct.h>

namespace ngl
{
	namespace gfx
	{
		void ResTextureData::OnResourceRenderUpdate(rhi::DeviceDep* p_device, rhi::GraphicsCommandListDep* p_commandlist)
		{
			assert(ref_texture_.IsValid());
			
				
			const rhi::TextureDep::Desc& dst_desc = ref_texture_->GetDesc();
			assert((dst_desc.mip_count * dst_desc.array_size * dst_desc.depth) == static_cast<int>(upload_subresource_info_array.size()));// ロードしたMetadataから取得したSubresource数と一致しないのであればどこか間違っている.

			// Copy用のLayout情報取得のためD3D12を直接利用している.
			u64 dst_byte_size;
			std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> dst_layout;
			dst_layout.resize(upload_subresource_info_array.size());
		
			const D3D12_RESOURCE_DESC d3d_dst_desc = ref_texture_->GetD3D12Resource()->GetDesc();
			p_device->GetD3D12Device()->GetCopyableFootprints(&d3d_dst_desc,0, static_cast<u32>(dst_layout.size()), 0,
				dst_layout.data(), nullptr, nullptr, &dst_byte_size);


			// 一時Buffer生成.
			rhi::RefBufferDep temporal_upload_buffer = new rhi::BufferDep();
			{
				rhi::BufferDep::Desc upload_buffer_desc = {};
				{
					upload_buffer_desc.heap_type = rhi::EResourceHeapType::Upload;
					upload_buffer_desc.element_byte_size = rhi::align_to(static_cast<u32>(dst_byte_size), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
					upload_buffer_desc.element_count = 1;
				}
				if(!temporal_upload_buffer->Initialize(p_device, upload_buffer_desc))
				{
					assert(false);
				}
			}
			// 一時BufferへCopy.
			if(u8* mapped = temporal_upload_buffer->MapAs<u8>())
			{
				// Subresource毎.
				for(int subresource_index = 0; subresource_index < upload_subresource_info_array.size(); ++subresource_index)
				{
					// RowPitchのAlignを考慮してコピー.
					if(dst_layout[subresource_index].Footprint.RowPitch != static_cast<u32>(upload_subresource_info_array[subresource_index].rowPitch))
					{
						// 読み取りと書き込みのRowPitchがAlighによってずれている場合はRow毎にコピーする.
						const auto* src_pixel_data = upload_subresource_info_array[subresource_index].pixels;
						const auto src_row_pitch = upload_subresource_info_array[subresource_index].rowPitch;
						const auto src_slice_pitch = upload_subresource_info_array[subresource_index].slicePitch;
						const auto num_row = src_slice_pitch / src_row_pitch;
						for(int row_i = 0; row_i<num_row; ++row_i)
						{
							memcpy(mapped + dst_layout[subresource_index].Offset + (row_i * dst_layout[subresource_index].Footprint.RowPitch),
								src_pixel_data + (row_i * src_row_pitch),
								src_row_pitch);
						}
					}
					else
					{
						// RowPitchが一致している場合はSlice毎コピー.
						memcpy(mapped + dst_layout[subresource_index].Offset, upload_subresource_info_array[subresource_index].pixels, upload_subresource_info_array[subresource_index].slicePitch);
					}
				}
				
				temporal_upload_buffer->Unmap();
			}

			
			// State Transition.
			{
				// Copy Dst.
				p_commandlist->ResourceBarrier(ref_texture_.Get(), ref_texture_->GetDesc().initial_state, rhi::EResourceState::CopyDst);
				
				p_commandlist->ResourceBarrier(temporal_upload_buffer.Get(), temporal_upload_buffer->GetDesc().initial_state, rhi::EResourceState::CopySrc);
			}

			// Copy Command.
			for(int subresource_index = 0; subresource_index < upload_subresource_info_array.size(); ++subresource_index)
			{
				D3D12_TEXTURE_COPY_LOCATION copy_location_src = {};
				{
					copy_location_src.pResource = temporal_upload_buffer->GetD3D12Resource();
					// Bufferの場合はFootprintで指定.
					copy_location_src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
					// UploadBufferに RowPitch を考慮してコピーしておくことで, dstのレイアウト情報をそのまま利用してコピー.
					copy_location_src.PlacedFootprint = dst_layout[subresource_index];
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
				//// 今後は読み取りリソースとして扱われるため ShaderRead.
				// Initial State の Commonなどに戻すようにした
				p_commandlist->ResourceBarrier(ref_texture_.Get(), rhi::EResourceState::CopyDst, ref_texture_->GetDesc().initial_state);
				
				// 一応 Common に戻す.
				p_commandlist->ResourceBarrier(temporal_upload_buffer.Get(), rhi::EResourceState::CopySrc, temporal_upload_buffer->GetDesc().initial_state);
			}
			
			
			// CPU側メモリ解放.
			this->upload_subresource_info_array = {};
			this->upload_pixel_memory_ = {};
			
		}
	}
}