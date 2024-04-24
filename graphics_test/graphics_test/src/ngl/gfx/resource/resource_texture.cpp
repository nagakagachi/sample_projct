
#include "resource_texture.h"

#include "ngl/resource/resource_manager.h"


#include <direct.h>

namespace ngl
{
	namespace gfx
	{
		void ResTexture::OnResourceRenderUpdate(rhi::DeviceDep* p_device, rhi::GraphicsCommandListDep* p_commandlist)
		{
			assert(ref_texture_.IsValid());
			
			const rhi::TextureDep::Desc& dst_desc = ref_texture_->GetDesc();
			// ロードしたMetadataから取得したSubresource数と一致しないのであればどこか間違っている.
			assert(ref_texture_->NumSubresource() == static_cast<int>(upload_subresource_info_array.size()));

			u64 dst_byte_size;
			std::vector<rhi::TextureSubresourceLayoutInfo> dst_layout;
			dst_layout.resize(ref_texture_->NumSubresource());
			// Subresouceのレイアウト情報を取得.
			ref_texture_->GetSubresourceLayoutInfo(dst_layout.data(), dst_byte_size);

			// Upload一時Buffer生成.
			rhi::RefBufferDep temporal_upload_buffer = new rhi::BufferDep();
			{
				rhi::BufferDep::Desc upload_buffer_desc = {};
				{
					upload_buffer_desc.heap_type = rhi::EResourceHeapType::Upload;
					upload_buffer_desc.initial_state = rhi::EResourceState::General;// D3DではUploadはGeneral(GenericRead)要求.
					upload_buffer_desc.element_byte_size = rhi::align_to(static_cast<u32>(dst_byte_size), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
					upload_buffer_desc.element_count = 1;
				}
				if(!temporal_upload_buffer->Initialize(p_device, upload_buffer_desc))
					assert(false);
			}
			// Upload一時BufferへLayoutを考慮してコピー.
			if(u8* mapped = temporal_upload_buffer->MapAs<u8>())
			{
				// Subresource毎.
				for(int subresource_index = 0; subresource_index < upload_subresource_info_array.size(); ++subresource_index)
				{
					// RowPitchのAlignを考慮してコピー.
					if(dst_layout[subresource_index].row_pitch != static_cast<u32>(upload_subresource_info_array[subresource_index].rowPitch))
					{
						// 読み取りと書き込みのRowPitchがAlighによってずれている場合はRow毎にコピーする.
						const auto* src_pixel_data = upload_subresource_info_array[subresource_index].pixels;
						const auto src_row_pitch = upload_subresource_info_array[subresource_index].rowPitch;
						const auto src_slice_pitch = upload_subresource_info_array[subresource_index].slicePitch;
						const auto num_row = src_slice_pitch / src_row_pitch;
						for(int row_i = 0; row_i<num_row; ++row_i)
						{
							memcpy(mapped + dst_layout[subresource_index].byte_offset + (row_i * dst_layout[subresource_index].row_pitch),
								src_pixel_data + (row_i * src_row_pitch),
								src_row_pitch);
						}
					}
					else
					{
						// RowPitchが一致している場合はSlice毎コピー.
						memcpy(mapped + dst_layout[subresource_index].byte_offset, upload_subresource_info_array[subresource_index].pixels, upload_subresource_info_array[subresource_index].slicePitch);
					}
				}
				
				temporal_upload_buffer->Unmap();
			}

			
			// State Transition.
			{
				// Copy Dst.
				p_commandlist->ResourceBarrier(ref_texture_.Get(), ref_texture_->GetDesc().initial_state, rhi::EResourceState::CopyDst);
				// upload bufferはGeneralのままでOK.
			}

			// Copy Command.
			for(int subresource_index = 0; subresource_index < upload_subresource_info_array.size(); ++subresource_index)
			{
				ref_texture_->CopyTextureRegion(p_commandlist, subresource_index, temporal_upload_buffer.Get(), dst_layout[subresource_index] );
			}

			// State Transition.
			{
				// Initial State の Commonなどに戻すようにした
				p_commandlist->ResourceBarrier(ref_texture_.Get(), rhi::EResourceState::CopyDst, ref_texture_->GetDesc().initial_state);
			}
			
			
			// CPU側メモリ解放.
			this->upload_subresource_info_array = {};
			this->upload_pixel_memory_ = {};
			
		}
	}
}