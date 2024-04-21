#pragma once

#include "mesh_renderer.h"

#include "global_render_resource.h"
#include "ngl/rhi/d3d12/command_list.d3d12.h"
#include "ngl/rhi/d3d12/shader.d3d12.h"

#include "ngl/gfx/common_struct.h"

namespace ngl
{
namespace gfx
{
	// 単一PSOでのメッシュ描画.
	// RenderTargetやViewport設定が完了している状態で呼び出される前提.
    void RenderMeshSinglePso(rhi::GraphicsCommandListDep& command_list, rhi::GraphicsPipelineStateDep& pso, const std::vector<gfx::StaticMeshComponent*>& mesh_instance_array, const rhi::ConstantBufferViewDep& cbv_sceneview)
    {
    	auto dummy_tex_srv = GlobalRenderResource::Instance().default_tex_dummy_srv_;

    	// ここではすべてPSO同じ.
		command_list.SetPipelineState(&pso);
    	
		for (int mesh_comp_i = 0; mesh_comp_i < mesh_instance_array.size(); ++mesh_comp_i)
		{
			const auto* e = mesh_instance_array[mesh_comp_i];

			auto cbv_instance = e->GetInstanceBufferView();


			for (int gi = 0; gi < e->model_.res_mesh_->data_.shape_array_.size(); ++gi)
			{
				const auto& shape_mat_index = e->model_.res_mesh_->shape_material_index_array_[gi];
				const auto& mat_data = e->model_.material_array_[shape_mat_index];
				
				// Descriptor.
				{
					ngl::rhi::DescriptorSetDep desc_set;

					pso.SetView(&desc_set, "cb_sceneview", &cbv_sceneview);
					pso.SetView(&desc_set, "cb_instance", cbv_instance.Get());

					pso.SetView(&desc_set, "samp_default", GlobalRenderResource::Instance().default_sampler_linear_wrap_.Get());
					// テクスチャ設定テスト.
					{
						auto tex_basecolor = (mat_data.tex_basecolor.IsValid())? mat_data.tex_basecolor->ref_view_ : dummy_tex_srv;
						pso.SetView(&desc_set, "tex_basecolor", tex_basecolor.Get());
					}

					// DescriptorSetでViewを設定.
					command_list.SetDescriptorSet(&pso, &desc_set);
				}


				// Geometry.
				auto& shape = e->model_.res_mesh_->data_.shape_array_[gi];

				// 一括設定. Mesh描画はセマンティクスとスロットを固定化しているため, Meshデータロード時にマッピングを構築してそのまま利用する.
				// PSO側のInputLayoutが要求するセマンティクスとのValidationチェックも可能なはず.
				D3D12_VERTEX_BUFFER_VIEW vtx_views[gfx::MeshVertexSemantic::SemanticSlotMaxCount()] = {};
				for (auto vi = 0; vi < gfx::MeshVertexSemantic::SemanticSlotMaxCount(); ++vi)
				{
					if (shape.vtx_attr_mask_.mask & (1 << vi))
						vtx_views[vi] = shape.p_vtx_attr_mapping_[vi]->rhi_vbv_.GetView();
				}
				command_list.SetVertexBuffers(0, (u32)std::size(vtx_views), vtx_views);

				// Set Index and topology.
				command_list.SetIndexBuffer(&shape.index_.rhi_vbv_.GetView());
				command_list.SetPrimitiveTopology(ngl::rhi::EPrimitiveTopology::TriangleList);

				// Draw.
				command_list.DrawIndexedInstanced(shape.num_primitive_ * 3, 1, 0, 0, 0);
			}
		}
    }
}
}
