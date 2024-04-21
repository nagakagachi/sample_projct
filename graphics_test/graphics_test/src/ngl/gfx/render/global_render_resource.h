#pragma once
#include "ngl/rhi/d3d12/resource_view.d3d12.h"
#include "ngl/util/singleton.h"


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
            
            
            {
                ngl::rhi::TextureDep::Desc desc = {};
                desc.bind_flag = ngl::rhi::ResourceBindFlag::ShaderResource;
                desc.format = ngl::rhi::EResourceFormat::Format_R8G8B8A8_UNORM;
                desc.type = ngl::rhi::ETextureType::Texture2D;
                desc.width = 64;
                desc.height = 64;
                desc.initial_state = ngl::rhi::EResourceState::ShaderRead;

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
                    std::cout << "[ERROR] Create RW SRV" << std::endl;
                    assert(false);
                    return false;
                }
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
