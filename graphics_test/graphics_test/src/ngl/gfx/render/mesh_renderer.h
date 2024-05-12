#pragma once


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
    void RenderMeshWithMaterialPass(rhi::GraphicsCommandListDep& command_list, const char* material_name, const char* pass_name, const std::vector<gfx::StaticMeshComponent*>& mesh_instance_array, const rhi::ConstantBufferViewDep& cbv_sceneview);
}
}
