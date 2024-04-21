#pragma once


#include "ngl/gfx/resource/resource_mesh.h"
#include "ngl/gfx/resource/resource_texture.h"
#include "ngl/math/math.h"
#include "ngl/resource/resource.h"

namespace ngl
{
namespace gfx
{
    class StandardRenderMaterial
    {
    public:
        res::ResourceHandle<ResTexture> tex_basecolor = {};
        res::ResourceHandle<ResTexture> tex_normal = {};
        res::ResourceHandle<ResTexture> tex_occlusion = {};
        res::ResourceHandle<ResTexture> tex_roughness = {};
        res::ResourceHandle<ResTexture> tex_metalness = {};
    };
    
    class StandardRenderModel
    {
    public:
        bool Initialize(rhi::DeviceDep* p_device, res::ResourceHandle<ResMeshData> res_mesh);
        
    public:
        res::ResourceHandle<ResMeshData> res_mesh_ = {};
        std::vector<StandardRenderMaterial> material_array_ = {};
    };
    
}
}
