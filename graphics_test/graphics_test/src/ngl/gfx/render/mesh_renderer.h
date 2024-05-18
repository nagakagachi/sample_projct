﻿#pragma once


#include "ngl/math/math.h"
#include "ngl/gfx/mesh_component.h"

namespace ngl
{
namespace rhi
{
    class GraphicsCommandListDep;
}

namespace gfx
{
    template<typename ViewType>
    struct RenderMeshTemplate
    {
        rhi::ResourceViewName   slot_name = {};
        ViewType*               p_view = {};
    };
    using RenderMeshCbv = RenderMeshTemplate<rhi::ConstantBufferViewDep>;
    using RenderMeshSrv = RenderMeshTemplate<rhi::ShaderResourceViewDep>;
    using RenderMeshUav = RenderMeshTemplate<rhi::UnorderedAccessViewDep>;
    using RenderMeshSampler = RenderMeshTemplate<rhi::SamplerDep>;
    
    struct RenderMeshResource
    {
        RenderMeshCbv cbv_sceneview = {};// SceneView定数バッファ.
        
        RenderMeshCbv cbv_d_shadowview = {};// DirectionalShadowView定数バッファ.
    };
    
    void RenderMeshWithMaterial(
        rhi::GraphicsCommandListDep& command_list, const char* pass_name,
        const std::vector<gfx::StaticMeshComponent*>& mesh_instance_array, const RenderMeshResource& render_mesh_resouce);
}
}
