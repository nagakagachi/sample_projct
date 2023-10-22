#pragma once

#include "render_pass.h"

#include<variant>


namespace ngl::render
{

	namespace task
	{

		// PreZパス.
		struct TaskDepthPass : public rtg::ITaskNode
		{
			// ノード定義コンストラクタ記述マクロ.
			ITASK_NODE_DEF_BEGIN(TaskDepthPass)
				ITASK_NODE_HANDLE_REGISTER(h_depth_)
				ITASK_NODE_DEF_END

				rtg::ResourceHandle h_depth_{};

			rhi::RefCbvDep ref_scene_cbv_{};
			std::vector<gfx::StaticMeshComponent*>* p_mesh_list_;

			rhi::RhiRef<rhi::GraphicsPipelineStateDep> pso_;

			virtual rtg::ETASK_TYPE TaskType() const
			{
				return rtg::ETASK_TYPE::GRAPHICS;
			}

			// リソースとアクセスを定義するプリプロセス.
			void Setup(rtg::RenderTaskGraphBuilder& builder, rhi::DeviceDep* p_device, rhi::RefCbvDep ref_scene_cbv, std::vector<gfx::StaticMeshComponent*>& ref_mesh_list)
			{
				// リソース定義.
				//rtg::ResourceDesc2D depth_desc = rtg::ResourceDesc2D::CreateAsRelative(1.0f, 1.0f, rhi::ResourceFormat::Format_D32_FLOAT_S8X24_UINT);// このフォーマットはRHI対応が必要なので後回し.
				rtg::ResourceDesc2D depth_desc = rtg::ResourceDesc2D::CreateAsRelative(1.0f, 1.0f, rhi::ResourceFormat::Format_D32_FLOAT);

				// リソースアクセス定義.
				h_depth_ = builder.RegisterResourceAccess(*this, builder.CreateResource(depth_desc), rtg::access_type::DEPTH_TARGET);

				{
					ref_scene_cbv_ = ref_scene_cbv;
					p_mesh_list_ = &ref_mesh_list;
				}

				{
					auto& ResourceMan = ngl::res::ResourceManager::Instance();

					static const char* k_shader_model = "6_3";

					ngl::gfx::ResShader::LoadDesc loaddesc_vs = {};
					loaddesc_vs.entry_point_name = "main_vs";
					loaddesc_vs.stage = ngl::rhi::ShaderStage::Vertex;
					loaddesc_vs.shader_model_version = k_shader_model;
					auto res_shader_vs = ResourceMan.LoadResource<ngl::gfx::ResShader>(p_device, "./src/ngl/data/shader/mesh/mesh_simple_depth_vs.hlsl", &loaddesc_vs);

					ngl::gfx::ResShader::LoadDesc loaddesc_ps = {};
					loaddesc_ps.entry_point_name = "main_ps";
					loaddesc_ps.stage = ngl::rhi::ShaderStage::Pixel;
					loaddesc_ps.shader_model_version = k_shader_model;
					auto res_shader_ps = ResourceMan.LoadResource<ngl::gfx::ResShader>(p_device, "./src/ngl/data/shader/mesh/mesh_simple_depth_ps.hlsl", &loaddesc_ps);


					ngl::rhi::GraphicsPipelineStateDep::Desc desc = {};
					desc.vs = &res_shader_vs->data_;
					desc.ps = &res_shader_ps->data_;

					desc.depth_stencil_state.depth_enable = true;
					desc.depth_stencil_state.depth_func = ngl::rhi::CompFunc::Greater; // ReverseZ.
					desc.depth_stencil_state.depth_write_mask = ~ngl::u32(0);
					desc.depth_stencil_state.stencil_enable = false;
					desc.depth_stencil_format = depth_desc.desc.format;

					// 入力レイアウト
					std::array<ngl::rhi::InputElement, 3> input_elem_data;
					desc.input_layout.num_elements = static_cast<ngl::u32>(input_elem_data.size());
					desc.input_layout.p_input_elements = input_elem_data.data();
					{
						input_elem_data[0].semantic_name = ngl::gfx::MeshVertexSemantic::SemanticNameStr(ngl::gfx::EMeshVertexSemanticKind::POSITION);
						input_elem_data[0].semantic_index = 0;
						input_elem_data[0].format = ngl::rhi::ResourceFormat::Format_R32G32B32_FLOAT;
						input_elem_data[0].stream_slot = ngl::gfx::MeshVertexSemantic::SemanticSlot(ngl::gfx::EMeshVertexSemanticKind::POSITION, 0);
						input_elem_data[0].element_offset = 0;

						input_elem_data[1].semantic_name = ngl::gfx::MeshVertexSemantic::SemanticNameStr(ngl::gfx::EMeshVertexSemanticKind::NORMAL);
						input_elem_data[1].semantic_index = 0;
						input_elem_data[1].format = ngl::rhi::ResourceFormat::Format_R32G32B32_FLOAT;
						input_elem_data[1].stream_slot = ngl::gfx::MeshVertexSemantic::SemanticSlot(ngl::gfx::EMeshVertexSemanticKind::NORMAL, 0);
						input_elem_data[1].element_offset = 0;

						input_elem_data[2].semantic_name = ngl::gfx::MeshVertexSemantic::SemanticNameStr(ngl::gfx::EMeshVertexSemanticKind::TEXCOORD);
						input_elem_data[2].semantic_index = 0;
						input_elem_data[2].format = ngl::rhi::ResourceFormat::Format_R32G32_FLOAT;
						input_elem_data[2].stream_slot = ngl::gfx::MeshVertexSemantic::SemanticSlot(ngl::gfx::EMeshVertexSemanticKind::TEXCOORD, 0);
						input_elem_data[2].element_offset = 0;
					}
					pso_ = new rhi::GraphicsPipelineStateDep();
					if (!pso_->Initialize(p_device, desc))
					{
						assert(false);
					}
				}
			}

			// 実際のレンダリング処理.
			void Run(rtg::RenderTaskGraphBuilder& builder, rhi::RhiRef<rhi::GraphicsCommandListDep> commandlist) override
			{
				// ハンドルからリソース取得. 必要なBarrierコマンドは外部で発行済である.
				auto res_depth = builder.GetAllocatedHandleResource(this, h_depth_);
				assert(res_depth.tex_.IsValid() && res_depth.dsv_.IsValid());


				commandlist->ClearDepthTarget(res_depth.dsv_.Get(), 0.0f, 0, true, true);// とりあえずクリアだけ.ReverseZなので0クリア.

				// Set RenderTarget.
				commandlist->SetRenderTargets(nullptr, 0, res_depth.dsv_.Get());

				// Set Viewport and Scissor.
				ngl::gfx::helper::SetFullscreenViewportAndScissor(commandlist.Get(), res_depth.tex_->GetWidth(), res_depth.tex_->GetHeight());

				// Mesh Rendering.
				ngl::gfx::RenderMeshSinglePso(*commandlist, *pso_, *p_mesh_list_, *ref_scene_cbv_);
			}
		};


		// GBufferパス.
		struct TaskGBufferPass : public rtg::ITaskNode
		{
			// ノード定義コンストラクタ記述マクロ.
			ITASK_NODE_DEF_BEGIN(TaskGBufferPass)
				ITASK_NODE_HANDLE_REGISTER(h_depth_)
				ITASK_NODE_HANDLE_REGISTER(h_gb0_)
				ITASK_NODE_HANDLE_REGISTER(h_gb1_)
				ITASK_NODE_DEF_END

				rtg::ResourceHandle h_depth_{};
			rtg::ResourceHandle h_gb0_{};
			rtg::ResourceHandle h_gb1_{};


			virtual rtg::ETASK_TYPE TaskType() const
			{
				return rtg::ETASK_TYPE::GRAPHICS;
			}

			// リソースとアクセスを定義するプリプロセス.
			void Setup(rtg::RenderTaskGraphBuilder& builder, rhi::DeviceDep* p_device, rtg::ResourceHandle h_depth)
			{
				// リソース定義.
				rtg::ResourceDesc2D gbuffer0_desc = rtg::ResourceDesc2D::CreateAsRelative(1.0f, 1.0f, rhi::ResourceFormat::Format_R8G8B8A8_UNORM);
				rtg::ResourceDesc2D gbuffer1_desc = rtg::ResourceDesc2D::CreateAsRelative(1.0f, 1.0f, rhi::ResourceFormat::Format_R11G11B10_FLOAT);

				// リソースアクセス定義.
				h_depth_ = builder.RegisterResourceAccess(*this, h_depth, rtg::access_type::DEPTH_TARGET);
				h_gb0_ = builder.RegisterResourceAccess(*this, builder.CreateResource(gbuffer0_desc), rtg::access_type::RENDER_TARTGET);
				h_gb1_ = builder.RegisterResourceAccess(*this, builder.CreateResource(gbuffer1_desc), rtg::access_type::RENDER_TARTGET);
			}

			// 実際のレンダリング処理.
			void Run(rtg::RenderTaskGraphBuilder& builder, rhi::RhiRef<rhi::GraphicsCommandListDep> commandlist) override
			{
				// ハンドルからリソース取得. 必要なBarrierコマンドは外部で発行済である.
				auto res_depth = builder.GetAllocatedHandleResource(this, h_depth_);
				auto res_gb0 = builder.GetAllocatedHandleResource(this, h_gb0_);
				auto res_gb1 = builder.GetAllocatedHandleResource(this, h_gb1_);

				assert(res_depth.tex_.IsValid() && res_depth.dsv_.IsValid());
				assert(res_gb0.tex_.IsValid() && res_gb0.rtv_.IsValid());
				assert(res_gb1.tex_.IsValid() && res_gb1.rtv_.IsValid());

				// TODO.
			}
		};


		// LinearDepthパス.
		struct TaskLinearDepthPass : public rtg::ITaskNode
		{
			// ノード定義コンストラクタ記述マクロ.
			ITASK_NODE_DEF_BEGIN(TaskLinearDepthPass)
				ITASK_NODE_HANDLE_REGISTER(h_depth_)
				ITASK_NODE_HANDLE_REGISTER(h_linear_depth_)
				ITASK_NODE_DEF_END

				rtg::ResourceHandle h_depth_{};
			rtg::ResourceHandle h_linear_depth_{};

			rhi::RefCbvDep ref_scene_cbv_{};

			rhi::RhiRef<rhi::ComputePipelineStateDep> pso_;

			virtual rtg::ETASK_TYPE TaskType() const
			{
				return rtg::ETASK_TYPE::GRAPHICS;
			}

			// リソースとアクセスを定義するプリプロセス.
			void Setup(rtg::RenderTaskGraphBuilder& builder, rhi::DeviceDep* p_device, rtg::ResourceHandle h_depth, rhi::RefCbvDep ref_scene_cbv)
			{
				{
					// リソース定義.
					rtg::ResourceDesc2D linear_depth_desc = rtg::ResourceDesc2D::CreateAsRelative(1.0f, 1.0f, rhi::ResourceFormat::Format_R32_FLOAT);

					// リソースアクセス定義.
					h_depth_ = builder.RegisterResourceAccess(*this, h_depth, rtg::access_type::SHADER_READ);
					h_linear_depth_ = builder.RegisterResourceAccess(*this, builder.CreateResource(linear_depth_desc), rtg::access_type::UAV);
				}

				{
					ref_scene_cbv_ = ref_scene_cbv;
				}

				{
					auto k_shader_model = "6_3";

					auto& ResourceMan = ngl::res::ResourceManager::Instance();

					ngl::gfx::ResShader::LoadDesc loaddesc = {};
					loaddesc.entry_point_name = "main_cs";
					loaddesc.stage = ngl::rhi::ShaderStage::Compute;
					loaddesc.shader_model_version = k_shader_model;
					auto res_shader = ResourceMan.LoadResource<ngl::gfx::ResShader>(p_device, "./src/ngl/data/shader/screen/generate_lineardepth_cs.hlsl", &loaddesc);

					ngl::rhi::ComputePipelineStateDep::Desc pso_desc = {};
					pso_desc.cs = &res_shader->data_;
					pso_ = new rhi::ComputePipelineStateDep();
					if (!pso_->Initialize(p_device, pso_desc))
					{
						assert(false);
					}
				}
			}

			// 実際のレンダリング処理.
			void Run(rtg::RenderTaskGraphBuilder& builder, rhi::RhiRef<rhi::GraphicsCommandListDep> commandlist) override
			{
				// ハンドルからリソース取得. 必要なBarrierコマンドは外部で発行済である.
				auto res_depth = builder.GetAllocatedHandleResource(this, h_depth_);
				auto res_linear_depth = builder.GetAllocatedHandleResource(this, h_linear_depth_);

				assert(res_depth.tex_.IsValid() && res_depth.srv_.IsValid());
				assert(res_linear_depth.tex_.IsValid() && res_linear_depth.uav_.IsValid());

				ngl::rhi::DescriptorSetDep desc_set = {};
				pso_->SetDescriptorHandle(&desc_set, "TexHardwareDepth", res_depth.srv_->GetView().cpu_handle);
				pso_->SetDescriptorHandle(&desc_set, "RWTexLinearDepth", res_linear_depth.uav_->GetView().cpu_handle);
				pso_->SetDescriptorHandle(&desc_set, "cb_sceneview", ref_scene_cbv_->GetView().cpu_handle);

				commandlist->SetPipelineState(pso_.Get());
				commandlist->SetDescriptorSet(pso_.Get(), &desc_set);

				pso_->DispatchHelper(commandlist.Get(), res_linear_depth.tex_->GetWidth(), res_linear_depth.tex_->GetHeight(), 1);

			}

		};


		// Lightingパス.
		struct TaskLightPass : public rtg::ITaskNode
		{
			// ノード定義コンストラクタ記述マクロ.
			ITASK_NODE_DEF_BEGIN(TaskLightPass)
				ITASK_NODE_HANDLE_REGISTER(h_depth_)
				ITASK_NODE_HANDLE_REGISTER(h_gb0_)
				ITASK_NODE_HANDLE_REGISTER(h_gb1_)
				ITASK_NODE_HANDLE_REGISTER(h_light_)
				ITASK_NODE_DEF_END

				rtg::ResourceHandle h_depth_{};
			rtg::ResourceHandle h_gb0_{};
			rtg::ResourceHandle h_gb1_{};
			rtg::ResourceHandle h_light_{};
			rtg::ResourceHandle h_tmp_{}; // 一時リソーステスト. マクロにも登録しない.


			virtual rtg::ETASK_TYPE TaskType() const
			{
				return rtg::ETASK_TYPE::GRAPHICS;
			}

			// リソースとアクセスを定義するプリプロセス.
			void Setup(rtg::RenderTaskGraphBuilder& builder, rhi::DeviceDep* p_device, rtg::ResourceHandle h_depth, rtg::ResourceHandle h_gb0, rtg::ResourceHandle h_gb1)
			{
				// リソース定義.
				rtg::ResourceDesc2D light_desc = rtg::ResourceDesc2D::CreateAsRelative(1.0f, 1.0f, rhi::ResourceFormat::Format_R16G16B16A16_FLOAT);

				// リソースアクセス定義.
				h_depth_ = builder.RegisterResourceAccess(*this, h_depth, rtg::access_type::SHADER_READ);
				h_gb0_ = builder.RegisterResourceAccess(*this, h_gb0, rtg::access_type::SHADER_READ);
				h_gb1_ = builder.RegisterResourceAccess(*this, h_gb1, rtg::access_type::SHADER_READ);
				h_light_ = builder.RegisterResourceAccess(*this, builder.CreateResource(light_desc), rtg::access_type::RENDER_TARTGET);// 他のNodeのものではなく新規リソースを要求する.


				// リソースアクセス期間による再利用のテスト用. 作業用の一時リソース.
				rtg::ResourceDesc2D temp_desc = rtg::ResourceDesc2D::CreateAsRelative(1.0f, 1.0f, rhi::ResourceFormat::Format_R11G11B10_FLOAT);
				auto temp_res0 = builder.RegisterResourceAccess(*this, builder.CreateResource(temp_desc), rtg::access_type::RENDER_TARTGET);
				h_tmp_ = temp_res0;
			}

			// 実際のレンダリング処理.
			void Run(rtg::RenderTaskGraphBuilder& builder, rhi::RhiRef<rhi::GraphicsCommandListDep> commandlist) override
			{
				// ハンドルからリソース取得. 必要なBarrierコマンドは外部で発行済である.
				auto res_depth = builder.GetAllocatedHandleResource(this, h_depth_);
				auto res_gb0 = builder.GetAllocatedHandleResource(this, h_gb0_);
				auto res_gb1 = builder.GetAllocatedHandleResource(this, h_gb1_);
				auto res_light = builder.GetAllocatedHandleResource(this, h_light_);
				auto res_tmp = builder.GetAllocatedHandleResource(this, h_tmp_);

				assert(res_depth.tex_.IsValid() && res_depth.srv_.IsValid());
				assert(res_gb0.tex_.IsValid() && res_gb0.srv_.IsValid());
				assert(res_gb1.tex_.IsValid() && res_gb1.srv_.IsValid());
				assert(res_light.tex_.IsValid() && res_light.rtv_.IsValid());
				assert(res_tmp.tex_.IsValid() && res_tmp.rtv_.IsValid());

				// TODO.
			}
		};


		// 最終パス.
		struct TaskFinalPass : public rtg::ITaskNode
		{
			// ノード定義コンストラクタ記述マクロ.
			ITASK_NODE_DEF_BEGIN(TaskFinalPass)
				ITASK_NODE_HANDLE_REGISTER(h_depth_)
				ITASK_NODE_HANDLE_REGISTER(h_linear_depth_)
				ITASK_NODE_HANDLE_REGISTER(h_light_)
				ITASK_NODE_DEF_END

				rtg::ResourceHandle h_depth_{};
			rtg::ResourceHandle h_linear_depth_{};
			rtg::ResourceHandle h_light_{};
			rtg::ResourceHandle h_tmp_{}; // 一時リソーステスト. マクロにも登録しない.

			// 外部指定の出力先バッファ.
			rhi::RefRtvDep ref_out_target_external_{};
			int out_width_ = 0;
			int out_height_ = 0;
			rhi::RefSampDep ref_samp_linear_clamp_{};
			rhi::RefSrvDep ref_raytrace_result_srv_;

			rhi::RhiRef<rhi::GraphicsPipelineStateDep> pso_;

			virtual rtg::ETASK_TYPE TaskType() const
			{
				return rtg::ETASK_TYPE::GRAPHICS;
			}

			// リソースとアクセスを定義するプリプロセス.
			void Setup(rtg::RenderTaskGraphBuilder& builder, rhi::DeviceDep* p_device, rtg::ResourceHandle h_depth, rtg::ResourceHandle h_linear_depth, rtg::ResourceHandle h_light,
				rhi::RefSampDep ref_samp_linear_clamp,
				rhi::RefSrvDep ref_raytrace_result_srv,
				rhi::ResourceFormat out_format, int out_width, int out_height, rhi::RefRtvDep ref_out_target)
			{
				{
					// リソースアクセス定義.
					h_depth_ = builder.RegisterResourceAccess(*this, h_depth, rtg::access_type::SHADER_READ);
					h_linear_depth_ = builder.RegisterResourceAccess(*this, h_linear_depth, rtg::access_type::SHADER_READ);
					h_light_ = builder.RegisterResourceAccess(*this, h_light, rtg::access_type::SHADER_READ);

					// リソースアクセス期間による再利用のテスト用. 作業用の一時リソース.
					rtg::ResourceDesc2D temp_desc = rtg::ResourceDesc2D::CreateAsRelative(1.0f, 1.0f, rhi::ResourceFormat::Format_R11G11B10_FLOAT);
					auto temp_res0 = builder.RegisterResourceAccess(*this, builder.CreateResource(temp_desc), rtg::access_type::RENDER_TARTGET);
					h_tmp_ = temp_res0;
				}

				{
					// 外部リソース.

					ref_out_target_external_ = ref_out_target;
					out_width_ = out_width;
					out_height_ = out_height;

					ref_raytrace_result_srv_ = ref_raytrace_result_srv;

					ref_samp_linear_clamp_ = ref_samp_linear_clamp;
				}

				{
					// 初期化. シェーダバイナリの要求とPSO生成.

					auto k_shader_model = "6_3";

					auto& ResourceMan = ngl::res::ResourceManager::Instance();

					ngl::gfx::ResShader::LoadDesc loaddesc_vs = {};
					{
						loaddesc_vs.entry_point_name = "main_vs";
						loaddesc_vs.stage = ngl::rhi::ShaderStage::Vertex;
						loaddesc_vs.shader_model_version = k_shader_model;
					}
					auto res_shader_vs = ResourceMan.LoadResource<ngl::gfx::ResShader>(p_device, "./src/ngl/data/shader/screen/fullscr_procedural_vs.hlsl", &loaddesc_vs);

					ngl::gfx::ResShader::LoadDesc loaddesc_ps = {};
					{
						loaddesc_ps.entry_point_name = "main_ps";
						loaddesc_ps.stage = ngl::rhi::ShaderStage::Pixel;
						loaddesc_ps.shader_model_version = k_shader_model;
					}
					auto res_shader_ps = ResourceMan.LoadResource<ngl::gfx::ResShader>(p_device, "./src/ngl/data/shader/final_screen_pass_ps.hlsl", &loaddesc_ps);


					ngl::rhi::GraphicsPipelineStateDep::Desc desc = {};
					desc.vs = &res_shader_vs->data_;
					desc.ps = &res_shader_ps->data_;

					desc.num_render_targets = 1;
					desc.render_target_formats[0] = out_format;

					desc.blend_state.target_blend_states[0].blend_enable = false;
					desc.blend_state.target_blend_states[0].write_mask = ~ngl::u8(0);

					pso_ = new rhi::GraphicsPipelineStateDep();
					if (!pso_->Initialize(p_device, desc))
					{
						assert(false);
					}
				}
			}

			// 実際のレンダリング処理.
			void Run(rtg::RenderTaskGraphBuilder& builder, rhi::RhiRef<rhi::GraphicsCommandListDep> commandlist) override
			{
				// ハンドルからリソース取得. 必要なBarrierコマンドは外部で発行済である.
				auto res_depth = builder.GetAllocatedHandleResource(this, h_depth_);
				auto res_linear_depth = builder.GetAllocatedHandleResource(this, h_linear_depth_);
				auto res_light = builder.GetAllocatedHandleResource(this, h_light_);
				auto res_tmp = builder.GetAllocatedHandleResource(this, h_tmp_);

				assert(res_depth.tex_.IsValid() && res_depth.srv_.IsValid());
				assert(res_linear_depth.tex_.IsValid() && res_linear_depth.srv_.IsValid());
				assert(res_light.tex_.IsValid() && res_light.srv_.IsValid());
				assert(res_tmp.tex_.IsValid() && res_tmp.rtv_.IsValid());

				assert(ref_out_target_external_.IsValid());// 外部出力先.

				gfx::helper::SetFullscreenViewportAndScissor(commandlist.Get(), out_width_, out_height_);

				// Rtv, Dsv セット.
				{
					const auto* p_rtv = ref_out_target_external_.Get();
					commandlist->SetRenderTargets(&p_rtv, 1, nullptr);
				}

				commandlist->SetPipelineState(pso_.Get());
				ngl::rhi::DescriptorSetDep desc_set = {};
				pso_->SetDescriptorHandle(&desc_set, "tex_lineardepth", res_linear_depth.srv_->GetView().cpu_handle);
				pso_->SetDescriptorHandle(&desc_set, "tex_rt", ref_raytrace_result_srv_->GetView().cpu_handle);
				pso_->SetDescriptorHandle(&desc_set, "samp", ref_samp_linear_clamp_->GetView().cpu_handle);
				commandlist->SetDescriptorSet(pso_.Get(), &desc_set);

				commandlist->DrawInstanced(3, 1, 0, 0);
			}
		};
	}
}

