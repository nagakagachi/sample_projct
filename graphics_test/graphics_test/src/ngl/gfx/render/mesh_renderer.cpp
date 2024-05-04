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
    	auto default_white_tex_srv = GlobalRenderResource::Instance().default_resource_.tex_white->ref_view_;
    	auto default_black_tex_srv = GlobalRenderResource::Instance().default_resource_.tex_black->ref_view_;
    	auto default_normal_tex_srv = GlobalRenderResource::Instance().default_resource_.tex_default_normal->ref_view_;


    	// このRenderが要求するInput.
    	MeshVertexSemanticSlotMask require_vtx_input_mask = {};
    	{
    		require_vtx_input_mask.AddSlot(EMeshVertexSemanticKind::POSITION, 0);
    		require_vtx_input_mask.AddSlot(EMeshVertexSemanticKind::NORMAL, 0);
    		require_vtx_input_mask.AddSlot(EMeshVertexSemanticKind::TANGENT, 0);
    		require_vtx_input_mask.AddSlot(EMeshVertexSemanticKind::BINORMAL, 0);
    		require_vtx_input_mask.AddSlot(EMeshVertexSemanticKind::TEXCOORD, 0);
    	}
    	

    	
    	// ここではすべてPSO同じ.
		command_list.SetPipelineState(&pso);
    	
		for (int mesh_comp_i = 0; mesh_comp_i < mesh_instance_array.size(); ++mesh_comp_i)
		{
			const auto* e = mesh_instance_array[mesh_comp_i];

			auto cbv_instance = e->GetInstanceBufferView();


			for (int gi = 0; gi < e->model_.res_mesh_->data_.shape_array_.size(); ++gi)
			{
				// Geometry.
				auto& shape = e->model_.res_mesh_->data_.shape_array_[gi];
				const auto& shape_mat_index = e->model_.res_mesh_->shape_material_index_array_[gi];
				const auto& mat_data = e->model_.material_array_[shape_mat_index];

				// 要求する頂点入力が足りない場合はスキップ.
				if(require_vtx_input_mask.mask != (require_vtx_input_mask.mask & shape.vtx_attr_mask_.mask))
					continue;
				
				// Descriptor.
				{
					ngl::rhi::DescriptorSetDep desc_set;

					pso.SetView(&desc_set, "ngl_cb_sceneview", &cbv_sceneview);
					pso.SetView(&desc_set, "ngl_cb_instance", cbv_instance.Get());

					pso.SetView(&desc_set, "samp_default", GlobalRenderResource::Instance().default_resource_.sampler_linear_wrap.Get());
					// テクスチャ設定テスト.
					{
						auto tex_basecolor = (mat_data.tex_basecolor.IsValid())? mat_data.tex_basecolor->ref_view_ : default_white_tex_srv;
						pso.SetView(&desc_set, "tex_basecolor", tex_basecolor.Get());
						
						auto tex_normal = (mat_data.tex_normal.IsValid())? mat_data.tex_normal->ref_view_ : default_normal_tex_srv;
						pso.SetView(&desc_set, "tex_normal", tex_normal.Get());
						
						auto tex_occlusion = (mat_data.tex_occlusion.IsValid())? mat_data.tex_occlusion->ref_view_ : default_white_tex_srv;
						pso.SetView(&desc_set, "tex_occlusion", tex_occlusion.Get());
						
						auto tex_roughness = (mat_data.tex_roughness.IsValid())? mat_data.tex_roughness->ref_view_ : default_white_tex_srv;
						pso.SetView(&desc_set, "tex_roughness", tex_roughness.Get());
						
						auto tex_metalness = (mat_data.tex_metalness.IsValid())? mat_data.tex_metalness->ref_view_ : default_black_tex_srv;
						pso.SetView(&desc_set, "tex_metalness", tex_metalness.Get());
					}

					// DescriptorSetでViewを設定.
					command_list.SetDescriptorSet(&pso, &desc_set);
				}



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
