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
    // 単一PSOでのメッシュ描画.
    // RenderTargetやViewport設定が完了している状態で呼び出される前提.
    void RenderMeshSinglePso(rhi::GraphicsCommandListDep& command_list, rhi::GraphicsPipelineStateDep& pso, const std::vector<gfx::StaticMeshComponent*>& mesh_instance_array, const rhi::ConstantBufferViewDep& cbv_sceneview);
    void RenderMeshWithMaterialPass(rhi::GraphicsCommandListDep& command_list, const char* pass_name, const std::vector<gfx::StaticMeshComponent*>& mesh_instance_array, const rhi::ConstantBufferViewDep& cbv_sceneview);
}
}
