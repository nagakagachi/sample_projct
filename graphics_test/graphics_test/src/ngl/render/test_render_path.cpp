﻿
#include "test_render_path.h"

#include "ngl/render/test_pass.h"

#include "ngl/gfx/render/mesh_renderer.h"
#include "ngl/gfx/render/global_render_resource.h"
#include "ngl/gfx/material/material_shader_manager.h"
#include "ngl/util/time/timer.h"

namespace ngl::test
{
	auto TestFrameRenderingPath(
			const RenderFrameDesc& render_frame_desc,
			RenderFrameOut& out_frame_out,
			ngl::rtg::RenderTaskGraphManager& rtg_manager,
			std::vector<ngl::rtg::RtgSubmitCommandSequenceElem>& out_graphics_cmd,
			std::vector<ngl::rtg::RtgSubmitCommandSequenceElem>& out_compute_cmd
		) -> void
	{
		auto* p_device = render_frame_desc.p_device;
				
		const auto screen_w = render_frame_desc.screen_w;
		const auto screen_h = render_frame_desc.screen_h;
		const float k_near_z = 0.1f;
		const float k_far_z = 10000.0f;
				
		const ngl::gfx::SceneRepresentation* p_scene = render_frame_desc.p_scene;

		// PassにわたすためのView情報.
		render::task::RenderPassViewInfo view_info;
		{
			view_info.camera_pos = render_frame_desc.camera_pos;
			view_info.camera_pose = render_frame_desc.camera_pose;
			view_info.near_z = k_near_z;
			view_info.far_z = k_far_z;
			view_info.camera_fov_y = render_frame_desc.camera_fov_y;
			view_info.aspect_ratio = (float)screen_w / (float)screen_h;
		}
		
				
		// Update View Constant Buffer.
		ngl::rhi::RefBufferDep sceneview_buffer = new ngl::rhi::BufferDep();
		ngl::rhi::RefCbvDep sceneview_cbv = new ngl::rhi::ConstantBufferViewDep();
		{
			ngl::rhi::BufferDep::Desc buffer_desc{};
			buffer_desc.SetupAsConstantBuffer(sizeof(ngl::gfx::CbSceneView));
			sceneview_buffer->Initialize(p_device, buffer_desc);

			ngl::rhi::ConstantBufferViewDep::Desc cbv_desc{};
			sceneview_cbv->Initialize(sceneview_buffer.Get(), cbv_desc);
		}
		// SceneView ConstantBuffer内容更新.
		{
			ngl::math::Mat34 view_mat = ngl::math::CalcViewMatrix(view_info.camera_pos, view_info.camera_pose.GetColumn2(), view_info.camera_pose.GetColumn1());

			// Infinite Far Reverse Perspective
			ngl::math::Mat44 proj_mat = ngl::math::CalcReverseInfiniteFarPerspectiveMatrix(view_info.camera_fov_y, view_info.aspect_ratio, view_info.near_z);
			ngl::math::Vec4 ndc_z_to_view_z_coef = ngl::math::CalcViewDepthReconstructCoefForInfiniteFarReversePerspective(view_info.near_z);

			if (auto* mapped = sceneview_buffer->MapAs<ngl::gfx::CbSceneView>())
			{
				mapped->cb_view_mtx = view_mat;
				mapped->cb_proj_mtx = proj_mat;
				mapped->cb_view_inv_mtx = ngl::math::Mat34::Inverse(view_mat);
				mapped->cb_proj_inv_mtx = ngl::math::Mat44::Inverse(proj_mat);

				mapped->cb_ndc_z_to_view_z_coef = ndc_z_to_view_z_coef;

				mapped->cb_time_sec = std::fmodf(static_cast<float>(ngl::time::Timer::Instance().GetElapsedSec("AppGameTime")), 60.0f*60.0f*24.0f);

				sceneview_buffer->Unmap();
			}
		}
				
		// RtgによるRenderPathの構築.
		{
			// Rtg構築用オブジェクト, 1回の構築-Compile-実行で使い捨てされる.
			ngl::rtg::RenderTaskGraphBuilder rtg_builder(screen_w, screen_h);
				
			ngl::rtg::RtgResourceHandle h_swapchain = {};
			// Rtgへ外部リソースの登録.
			{
				if(render_frame_desc.ref_swapchain.IsValid())
				{
					// このRtgの開始時点のStateと終了時にあるべきStateを指定.
					h_swapchain = rtg_builder.RegisterSwapchainResource(render_frame_desc.ref_swapchain, render_frame_desc.ref_swapchain_rtv, render_frame_desc.swapchain_state_prev, render_frame_desc.swapchain_state_next);
				}
				// TODO. any other.
				// ...
			}
			
				
#define ASYNC_COMPUTE_TEST0 0
#define ASYNC_COMPUTE_TEST1 1
			// 1連のRenderPathを構成するPass群を生成し, それらの間のリソース依存関係を構築.
			{
				// AsyncComputeの依存関係の追い越しパターンテスト用.
				ngl::rtg::RtgResourceHandle async_compute_tex0 = {};
#if ASYNC_COMPUTE_TEST0
				// ----------------------------------------
				// AsyncCompute Pass.
				auto* task_test_compute0 = rtg_builder.AppendTaskNode<ngl::render::task::TaskCopmuteTest>();
				{
					ngl::render::task::TaskCopmuteTest::SetupDesc setup_desc{};
					{
						setup_desc.ref_scene_cbv = sceneview_cbv;
					}
					task_test_compute0->Setup(rtg_builder, p_device, view_info, {}, setup_desc);
					async_compute_tex0 = task_test_compute0->h_work_tex_;
				}
#endif

				// ----------------------------------------
				// Raytrace Pass.
				rtg::RtgResourceHandle h_rt_result = {};
				if(render_frame_desc.p_rt_scene)
				{
					// 外部からRaytraceSceneが渡されていればPassを生成する.
					auto* task_rt_test = rtg_builder.AppendTaskNode<render::task::TaskRtDispatch>();
					{
						render::task::TaskRtDispatch::SetupDesc setup_desc{};
						{
							setup_desc.p_rt_scene = render_frame_desc.p_rt_scene;
						}
						task_rt_test->Setup(rtg_builder, p_device, view_info, setup_desc);

						h_rt_result = task_rt_test->h_rt_result_;
					}
				}

				// ----------------------------------------
				// PreZ Pass.
				auto* task_depth = rtg_builder.AppendTaskNode<ngl::render::task::TaskDepthPass>();
				{
					ngl::render::task::TaskDepthPass::SetupDesc setup_desc{};
					{
						setup_desc.ref_scene_cbv = sceneview_cbv;
						setup_desc.p_mesh_list = &p_scene->mesh_instance_array_;
					}
					task_depth->Setup(rtg_builder, p_device, view_info, setup_desc);
				}
					
				// ----------------------------------------
				// Linear Depth Pass.
				auto* task_linear_depth = rtg_builder.AppendTaskNode<ngl::render::task::TaskLinearDepthPass>();
				{
					ngl::render::task::TaskLinearDepthPass::SetupDesc setup_desc{};
					{
						setup_desc.ref_scene_cbv = sceneview_cbv;
					}
					task_linear_depth->Setup(rtg_builder, p_device, view_info, task_depth->h_depth_, async_compute_tex0, setup_desc);
				}
				
				ngl::rtg::RtgResourceHandle async_compute_tex1 = {};
#if ASYNC_COMPUTE_TEST1
				// ----------------------------------------
				// AsyncCompute Pass.
				auto* task_test_compute1 = rtg_builder.AppendTaskNode<ngl::render::task::TaskCopmuteTest>();
				{
					ngl::render::task::TaskCopmuteTest::SetupDesc setup_desc{};
					{
						setup_desc.ref_scene_cbv = sceneview_cbv;
					}
					task_test_compute1->Setup(rtg_builder, p_device, view_info, task_linear_depth->h_linear_depth_, setup_desc);
					async_compute_tex1 = task_test_compute1->h_work_tex_;
				}
#endif
					
				// ----------------------------------------
				// GBuffer Pass.
				auto* task_gbuffer = rtg_builder.AppendTaskNode<ngl::render::task::TaskGBufferPass>();
				{
					ngl::render::task::TaskGBufferPass::SetupDesc setup_desc{};
					{
						setup_desc.ref_scene_cbv = sceneview_cbv;
						setup_desc.p_mesh_list = &p_scene->mesh_instance_array_;
					}
					task_gbuffer->Setup(rtg_builder, p_device, view_info, task_depth->h_depth_, async_compute_tex0, setup_desc);
				}
					
				// ----------------------------------------
				// DirectionalShadow Pass.
				auto* task_d_shadow = rtg_builder.AppendTaskNode<ngl::render::task::TaskDirectionalShadowPass>();
				{
					ngl::render::task::TaskDirectionalShadowPass::SetupDesc setup_desc{};
					{
						setup_desc.ref_scene_cbv = sceneview_cbv;
						setup_desc.p_mesh_list = &p_scene->mesh_instance_array_;
						
						// Directionalのライト方向テスト.
						setup_desc.directional_light_dir = ngl::math::Vec3::Normalize(render_frame_desc.directional_light_dir);
					}
					task_d_shadow->Setup(rtg_builder, p_device, view_info, setup_desc);
				}
					
				// ----------------------------------------
				// Deferred Lighting Pass.
				auto* task_light = rtg_builder.AppendTaskNode<ngl::render::task::TaskLightPass>();
				{
					ngl::render::task::TaskLightPass::SetupDesc setup_desc{};
					{
						setup_desc.ref_scene_cbv = sceneview_cbv;
						setup_desc.ref_shadow_cbv = task_d_shadow->ref_d_shadow_sample_cbv_;
					}
					task_light->Setup(rtg_builder, p_device, view_info,
						task_gbuffer->h_gb0_, task_gbuffer->h_gb1_, task_gbuffer->h_gb2_, task_gbuffer->h_gb3_,
						task_gbuffer->h_velocity_, task_linear_depth->h_linear_depth_, render_frame_desc.h_prev_lit,
						task_d_shadow->h_shadow_depth_atlas_,
						async_compute_tex1,
						setup_desc);
				}

				// ----------------------------------------
				// Final Composite to Swapchain.
				if(!h_swapchain.IsInvalid())
				{
					// Swapchainが指定されている場合のみ最終Passを登録.
					auto* task_final = rtg_builder.AppendTaskNode<ngl::render::task::TaskFinalPass>();
					{
						// GBufferデバッグ表示用のリソース.
						rtg::RtgResourceHandle debug_gbuffer0 = {};
						rtg::RtgResourceHandle debug_gbuffer1 = {};
						rtg::RtgResourceHandle debug_gbuffer2 = {};
						rtg::RtgResourceHandle debug_gbuffer3 = {};
						if(render_frame_desc.debugview_gbuffer)
						{
							debug_gbuffer0 = task_gbuffer->h_gb0_;
							debug_gbuffer1 = task_gbuffer->h_gb1_;
							debug_gbuffer2 = task_gbuffer->h_gb2_;
							debug_gbuffer3 = task_gbuffer->h_gb3_;
						}
						rtg::RtgResourceHandle debug_dshadow = {};
						if(render_frame_desc.debugview_dshadow)
						{
							debug_dshadow = task_d_shadow->h_shadow_depth_atlas_;
						}
						
						ngl::render::task::TaskFinalPass::SetupDesc setup_desc{};
						{
							setup_desc.ref_scene_cbv = sceneview_cbv;

							setup_desc.debugview_halfdot_gray = render_frame_desc.debugview_halfdot_gray;
							setup_desc.debugview_subview_result = render_frame_desc.debugview_subview_result;
							setup_desc.debugview_raytrace_result = render_frame_desc.debugview_raytrace_result;

							setup_desc.debugview_gbuffer = render_frame_desc.debugview_gbuffer;
							setup_desc.debugview_dshadow = render_frame_desc.debugview_dshadow;
						}
						
						task_final->Setup(rtg_builder, p_device, view_info, h_swapchain,
							task_gbuffer->h_depth_, task_linear_depth->h_linear_depth_, task_light->h_light_,
							render_frame_desc.h_other_graph_out_tex, h_rt_result,
							debug_gbuffer0, debug_gbuffer1, debug_gbuffer2, debug_gbuffer3,
							debug_dshadow,
							setup_desc);
					}
				}
					
				// 次回フレームへの伝搬. 次回フレームでは h_prev_light によって前回フレームリソースを利用できる.
				{
					out_frame_out.h_propagate_lit = rtg_builder.PropagateResouceToNextFrame(task_light->h_light_);
				}
			}

			// 構築したRtgをCompile.
			//	ここでリソース依存関係とBarrier, 非同期コンピュート同期, リソース再利用などが確定する.
			rtg_manager.Compile(rtg_builder);
				
			// Rtgを実行し構成Taskの Run() を実行, CommandListを生成する.
			//	Compileによってリソースプールのステートが更新され, その後にCompileされたGraphはそれを前提とするため, Graphは必ずExecuteする必要がある.
			//	各TaskのRun()はそれぞれ別スレッドで並列実行される可能性がある.
			rtg_builder.Execute(out_graphics_cmd, out_compute_cmd);
		}
	}
}

