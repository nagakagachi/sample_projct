#pragma once

#include "render_pass.h"


namespace ngl::render
{

	// LinearDepth生成ComputePass.
	class PassGenLinearDepth
	{
	public:

		bool Initialize(rhi::DeviceDep* p_device)
		{			
			// 初期化. シェーダバイナリの要求とPSO生成.

			auto& ResourceMan = ngl::res::ResourceManager::Instance();

			ngl::gfx::ResShader::LoadDesc loaddesc = {};
			loaddesc.entry_point_name = "main_cs";
			loaddesc.stage = ngl::rhi::ShaderStage::Compute;
			loaddesc.shader_model_version = "6_3";
			auto res_shader = ResourceMan.LoadResource<ngl::gfx::ResShader>(p_device, "./src/ngl/data/shader/screen/generate_lineardepth_cs.hlsl", &loaddesc);

			pso_ = new rhi::ComputePipelineStateDep();
			ngl::rhi::ComputePipelineStateDep::Desc pso_desc = {};
			pso_desc.cs = &res_shader->data_;
			if (!pso_->Initialize(p_device, pso_desc))
			{
				assert(false);
				return false;
			}
			return true;
		}

		// Execute前のリソースSetup処理.
		void Setup(GraphBuilder& builder)
		{
			//	将来的にはここでRenderGraphにリソース定義登録などをする予定.

			auto* p_device = builder.GetDevice();
		}
		// 実処理.
		void Execute(GraphBuilder& builder, rhi::GraphicsCommandListDep* p_command_list)
		{
			// 将来的にはSetupで登録した情報から適切なリソース参照をRenderGraphから引き出す予定.

			// 現在は直接リソースのMapから引き出している. ステート遷移などは外側でやっているので注意.

			auto cbv_scene_view = builder.GetFrameCbv("scene_view");
			assert(cbv_scene_view.IsValid());

			auto srv_depth = builder.GetFrameSrv("hardware_depth");
			assert(srv_depth.IsValid());

			auto tex_linear_depth = builder.GetFrameTexture("linear_depth");
			assert(tex_linear_depth.IsValid());
			auto uav_linear_depth = builder.GetFrameUav("linear_depth");
			assert(uav_linear_depth.IsValid());


			ngl::rhi::DescriptorSetDep desc_set = {};
			pso_->SetDescriptorHandle(&desc_set, "TexHardwareDepth", srv_depth->GetView().cpu_handle);
			pso_->SetDescriptorHandle(&desc_set, "RWTexLinearDepth", uav_linear_depth->GetView().cpu_handle);
			pso_->SetDescriptorHandle(&desc_set, "cb_sceneview", cbv_scene_view->GetView().cpu_handle);

			p_command_list->SetPipelineState(pso_.Get());
			p_command_list->SetDescriptorSet(pso_.Get(), &desc_set);

			pso_->DispatchHelper(p_command_list, tex_linear_depth->GetWidth(), tex_linear_depth->GetHeight(), 1);
		}

	private:
		rhi::RhiRef<rhi::ComputePipelineStateDep> pso_;
	};


}

