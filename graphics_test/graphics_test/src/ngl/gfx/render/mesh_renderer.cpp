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

	namespace
	{
		// TODO. 有効なバッファのみ設定.
		void SetVertexBufferHelper(rhi::GraphicsCommandListDep& command_list, EMeshVertexSemanticSlotKind::Type semantic_kind, int semantic_index, const MeshShapeVertexDataBase& vtx_buffer)
		{
			const auto slot_index = MeshVertexSemanticSlotInfo::SemanticSlot(semantic_kind, semantic_index);

			if (vtx_buffer.IsValid())
				command_list.SetVertexBuffers(slot_index, 1, &vtx_buffer.rhi_vbv_.GetView());
		}
	}

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
				// Set Vertex.
				SetVertexBufferHelper(command_list, EMeshVertexSemanticSlotKind::POSITION, 0, shape.position_);
				SetVertexBufferHelper(command_list, EMeshVertexSemanticSlotKind::NORMAL, 0, shape.normal_);
				SetVertexBufferHelper(command_list, EMeshVertexSemanticSlotKind::TANNGENT, 0, shape.normal_);
				SetVertexBufferHelper(command_list, EMeshVertexSemanticSlotKind::BINORMAL, 0, shape.normal_);
				for(auto si = 0; si < shape.texcoord_.size(); ++si)
					SetVertexBufferHelper(command_list, EMeshVertexSemanticSlotKind::TEXCOORD, si, shape.texcoord_[si]);
				for (auto si = 0; si < shape.color_.size(); ++si)
					SetVertexBufferHelper(command_list, EMeshVertexSemanticSlotKind::COLOR, si, shape.color_[si]);


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
