#pragma once

#include "ngl/rhi/d3d12/resource_view.d3d12.h"
#include "ngl/util/singleton.h"

#include "ngl/resource/resource_manager.h"

namespace ngl::gfx
{
    // レンダリング全体から利用するデフォルトリソースなどを簡易管理する.
    class GlobalRenderResource : public Singleton<GlobalRenderResource>
    {
    public:
        bool Initialize(rhi::DeviceDep* p_device)
        {
            p_device_ = p_device;

            {
                default_sampler_linear_wrap_ = new rhi::SamplerDep();
                
                ngl::rhi::SamplerDep::Desc samp_desc = {};
                samp_desc.Filter = ngl::rhi::ETextureFilterMode::Min_Linear_Mag_Linear_Mip_Linear;
                samp_desc.AddressU = ngl::rhi::ETextureAddressMode::Repeat;
                samp_desc.AddressV = ngl::rhi::ETextureAddressMode::Repeat;
                samp_desc.AddressW = ngl::rhi::ETextureAddressMode::Repeat;
                if (!default_sampler_linear_wrap_->Initialize(p_device_, samp_desc))
                {
                    std::cout << "[ERROR] Create rhi::SamplerDep" << std::endl;
                    assert(false);
                    return false;
                }
            }
            
            // ランタイム生成デフォルトテクスチャ.
            //  RTVのAlignmentがもったいない気がしたので, 通常Textureとして生成してimagedataのupload&copyで塗りつぶしするようにしてみた.
            {
                ngl::rhi::TextureDep::Desc desc = {};
                desc.bind_flag = ngl::rhi::ResourceBindFlag::ShaderResource;
                desc.format = ngl::rhi::EResourceFormat::Format_R8G8B8A8_UNORM;// 8bitカラー.
                desc.type = ngl::rhi::ETextureType::Texture2D;
                desc.width = 64;
                desc.height = 64;
                desc.mip_count = 1;

                assert(desc.format == rhi::EResourceFormat::Format_R8G8B8A8_UNORM);// Uploadを利用して直接ImageDataをコピーするため単純な8bit colorとする.
                default_tex_dummy_ = new ngl::rhi::TextureDep();
                if (!default_tex_dummy_->Initialize(p_device_, desc))
                {
                    std::cout << "[ERROR] Create Texture Initialize" << std::endl;
                    assert(false);
                    return false;
                }
                default_tex_dummy_srv_ = new ngl::rhi::ShaderResourceViewDep();
                if (!default_tex_dummy_srv_->InitializeAsTexture(p_device_, default_tex_dummy_.Get(), 0, 1, 0, 1))
                {
                    std::cout << "[ERROR] Create SRV" << std::endl;
                    assert(false);
                    return false;
                }

                constexpr math::Vec3 clear_color = {1.0f, 0.1f, 0.1f};// 任意カラー指定.
                // クリアするCommandを発行するLambda登録.
                auto p_tex = default_tex_dummy_.Get();
                res::ResourceManager::Instance().AddFrameRenderUpdateLambda(
                    [p_tex, clear_color](rhi::GraphicsCommandListDep* p_command_list)
                    {
                        u64 dst_byte_size;
                        std::vector<rhi::TextureSubresourceLayoutInfo> dst_layout;
                        dst_layout.resize(p_tex->NumSubresource());
                        // Subresouceのレイアウト情報を取得.
                        p_tex->GetSubresourceLayoutInfo(dst_layout.data(), dst_byte_size);
			
                        rhi::RefBufferDep temporal_upload_buffer = {};
                        u8* p_upload_buffer_memory = {};
                        // TextureUpload用の一時バッファ上メモリを確保.
                        res::ResourceManager::Instance().AllocTextureUploadIntermediateBufferMemory(temporal_upload_buffer, p_upload_buffer_memory, dst_byte_size, p_tex->GetParentDevice());
                        if(!p_upload_buffer_memory)
                        {
                            // 一時Buffer上のメモリ確保に失敗.
                            std::cout << "[ERROR] Failed to AllocTextureUploadIntermediateBufferMemory." << std::endl;
                            assert(p_upload_buffer_memory);
                            return;
                        }
                        // Uploadするイメージデータ. 指定カラーで埋める(ここでは 8bit color前提).
                        for(u32 bi = 0; bi < dst_byte_size; bi += 4)
                        {
                            p_upload_buffer_memory[bi+0] = static_cast<u8>(255 * clear_color.x);
                            p_upload_buffer_memory[bi+1] = static_cast<u8>(255 * clear_color.y);
                            p_upload_buffer_memory[bi+2] = static_cast<u8>(255 * clear_color.z);
                            p_upload_buffer_memory[bi+3] = 0xff;
                        }

                        p_command_list->ResourceBarrier(p_tex, p_tex->GetDesc().initial_state, rhi::EResourceState::CopyDst);
                        p_tex->CopyTextureRegion(p_command_list, 0, temporal_upload_buffer.Get(), dst_layout[0]);
                        p_command_list->ResourceBarrier(p_tex, rhi::EResourceState::CopyDst, rhi::EResourceState::ShaderRead);// これ以降はSrv利用のみ.
                    }
                );
            }

            
            return true;
        }
        void Finalize()
        {
            default_sampler_linear_wrap_ = {};

            default_tex_dummy_ = {};
            default_tex_dummy_srv_ = {};
        }

        rhi::DeviceDep* p_device_ = {};
        
        rhi::RefSampDep default_sampler_linear_wrap_ = {};

        rhi::RefTextureDep      default_tex_dummy_ = {};
        rhi::RefSrvDep          default_tex_dummy_srv_ = {};
    };

}
