#pragma once

#include "mesh_renderer.h"

#include "ngl/rhi/d3d12/command_list.d3d12.h"
#include "ngl/rhi/d3d12/shader.d3d12.h"

#include "ngl/gfx/common_struct.h"

namespace ngl
{
namespace gfx
{
    // TODO. メッシュ描画用のヘルパを実装.

	// 単一PSOでのメッシュ描画.
	// RenderTargetやViewport設定が完了している状態で呼び出される前提.
    void RenderMeshSinglePso(rhi::GraphicsCommandListDep& command_list, rhi::GraphicsPipelineStateDep& pso, const std::vector<gfx::StaticMeshComponent*>& mesh_instance_array, const rhi::ConstantBufferViewDep& cbv_sceneview)
    {
		command_list.SetPipelineState(&pso);

		for (int mesh_comp_i = 0; mesh_comp_i < mesh_instance_array.size(); ++mesh_comp_i)
		{
			const auto* e = mesh_instance_array[mesh_comp_i];

			auto cbv_instance = e->GetInstanceBufferView();


			for (int gi = 0; gi < e->GetMeshData()->data_.shape_array_.size(); ++gi)
			{
				// Descriptor.
				{
					ngl::rhi::DescriptorSetDep desc_set;

					pso.SetDescriptorHandle(&desc_set, "cb_sceneview", cbv_sceneview.GetView().cpu_handle);
					pso.SetDescriptorHandle(&desc_set, "cb_instance", cbv_instance->GetView().cpu_handle);


					// DescriptorSetでViewを設定.
					command_list.SetDescriptorSet(&pso, &desc_set);
				}


				// Geometry.
				auto& shape = e->GetMeshData()->data_.shape_array_[gi];

				// 一括設定. Mesh描画はセマンティクスとスロットを固定化しているため, Meshデータロード時にマッピングを構築してそのまま利用する.
				// PSO側のInputLayoutが要求するセマンティクスとのValidationチェックも可能なはず.
				D3D12_VERTEX_BUFFER_VIEW vtx_views[gfx::MeshVertexSemantic::SemanticSlotMaxCount()] = {};
				for (auto vi = 0; vi < gfx::MeshVertexSemantic::SemanticSlotMaxCount(); ++vi)
				{
					if (shape.slot_mask_.mask & (1 << vi))
						vtx_views[vi] = shape.p_slot_mapping_[vi]->rhi_vbv_.GetView();
				}
				command_list.SetVertexBuffers(0, (u32)std::size(vtx_views), vtx_views);

				// Set Index and topology.
				command_list.SetIndexBuffer(&shape.index_.rhi_vbv_.GetView());
				command_list.SetPrimitiveTopology(ngl::rhi::PrimitiveTopology::TriangleList);

				// Draw.
				command_list.DrawIndexedInstanced(shape.num_primitive_ * 3, 1, 0, 0, 0);
			}
		}
    }
}
}
