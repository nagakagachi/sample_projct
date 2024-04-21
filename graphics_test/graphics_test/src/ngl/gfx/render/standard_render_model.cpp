
#include "standard_render_model.h"

#include "ngl/resource/resource_manager.h"

namespace ngl::gfx
{

    bool StandardRenderModel::Initialize(rhi::DeviceDep* p_device, res::ResourceHandle<ResMeshData> res_mesh)
    {
        auto& res_manager = ngl::res::ResourceManager::Instance();
        

        res_mesh_ = res_mesh;

        material_array_ = {};
        material_array_.resize(res_mesh_->material_data_array_.size());
        for(int i = 0; i < material_array_.size(); ++i)
        {
            ResTexture::LoadDesc load_desc = {};

            if(0 < res_mesh_->material_data_array_[i].tex_basecolor.Length())
                material_array_[i].tex_basecolor = res_manager.LoadResource<ResTexture>(p_device, res_mesh_->material_data_array_[i].tex_basecolor.Get(), &load_desc);
            
            if(0 < res_mesh_->material_data_array_[i].tex_normal.Length())
                material_array_[i].tex_normal = res_manager.LoadResource<ResTexture>(p_device, res_mesh_->material_data_array_[i].tex_normal.Get(), &load_desc);
            
            if(0 < res_mesh_->material_data_array_[i].tex_occlusion.Length())
                material_array_[i].tex_occlusion = res_manager.LoadResource<ResTexture>(p_device, res_mesh_->material_data_array_[i].tex_occlusion.Get(), &load_desc);

            if(0 < res_mesh_->material_data_array_[i].tex_roughness.Length())
                material_array_[i].tex_roughness = res_manager.LoadResource<ResTexture>(p_device, res_mesh_->material_data_array_[i].tex_roughness.Get(), &load_desc);
            
            if(0 < res_mesh_->material_data_array_[i].tex_metalness.Length())
                material_array_[i].tex_metalness = res_manager.LoadResource<ResTexture>(p_device, res_mesh_->material_data_array_[i].tex_metalness.Get(), &load_desc);
        }

        return true;
    }


    
}

