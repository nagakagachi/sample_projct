﻿#pragma once

#include "render_pass.h"

#include<variant>


namespace ngl::render
{

	namespace graph
	{
		using ResName = text::HashCharPtr<64>;


		enum class AccessType
		{
			ReadOnly,
			WriteOnly,
			ReadWrite
		};

		
		struct IPassNode;

		struct PinWrapper
		{
			// Pinの親Node.
			IPassNode* p_parent_{};
			// Pinのローカル名.
			ResName local_name_{};
			// Pinの参照先.
			const PinWrapper* ref_{};

			// Pinの参照をつなげる.
			void connect_from(const PinWrapper& src)
			{
				ref_ = &src;
			}
		};

		struct IPassNode
		{
			ResName name_{};

			std::vector<PinWrapper*> ref_pin_list_{};
			std::vector<PinWrapper*> out_pin_list_{};

			// Nodeの情報セットアップ.
			void SetupNode(ResName node_name)
			{
				name_ = node_name;
			}

			void InitRefPin(PinWrapper& pin, ResName name)
			{
				SetupPin(pin, name);
				ref_pin_list_.push_back(&pin);
			}
			void InitOutPin(PinWrapper& pin, ResName name)
			{
				SetupPin(pin, name);
				out_pin_list_.push_back(&pin);
			}

		private:
			// 保持するPinのセットアップ.
			void SetupPin(PinWrapper& pin, ResName name)
			{
				// Pinの親Nodeへの参照他をセットアップ.
				pin = { this, name , {} };
			}
		};

		struct ResourceAllocHandle
		{
			uint32_t inner_data = {};

			
			static constexpr ResourceAllocHandle InvalidHandle()
			{
				return ResourceAllocHandle({});
			}
			bool IsInvalid() const
			{
				return inner_data == InvalidHandle().inner_data;
			}
		};

		struct GpuTaskGraphBuilder
		{
			// IPassNode
			template<typename TPassNode>
			TPassNode* CreateNode()
			{
				auto new_node = new TPassNode();
				node_array_.push_back(new_node);
				return new_node;
			}

			ResourceAllocHandle CreateResource(int res_desc)
			{
				ResourceAllocHandle handle{};

				++s_res_handle_id_counter_;
				if (0 == s_res_handle_id_counter_)
					++s_res_handle_id_counter_;// 0は無効ID扱いのためスキップ.

				handle.inner_data = s_res_handle_id_counter_;// ユニークID割当.

				return handle;
			}

			void CreateResourceAccess(ResourceAllocHandle handle, bool write)
			{
				if (handle.IsInvalid())
				{
					assert(false);
					return;
				}

			}

			~GpuTaskGraphBuilder()
			{
				for (auto* p : node_array_)
				{
					if (p)
					{
						delete p;
						p = nullptr;
					}
				}
				node_array_.clear();
			}

			std::vector<IPassNode*> node_array_{};

			uint32_t s_res_handle_id_counter_ = {};
		};

		struct PassPreZ : public IPassNode
		{
			PinWrapper out_depth_{};

			PassPreZ()
			{
				SetupNode("prez");

				InitOutPin(out_depth_, "depth");
			}

			void SetupGraph()
			{
			}
		};

		struct PassGbuffer : public IPassNode
		{
			PinWrapper readwrite_depth_{};

			PinWrapper out_gbuffer0_{};
			PinWrapper out_gbuffer1_{};
			PinWrapper out_depth_{};

			PassGbuffer()
			{
				SetupNode("gbuffer");

				InitRefPin(readwrite_depth_, "gbuffer_depth");
				InitRefPin(out_gbuffer0_, "gbuffer0");
				InitRefPin(out_gbuffer1_, "gbuffer1");

				InitOutPin(out_depth_, "depth");

				// 参照したDepthを出力としてエイリアス.
				out_depth_.connect_from(readwrite_depth_);
			}

			void SetupGraph(const PinWrapper& readwrite_depth)
			{
				readwrite_depth_.connect_from(readwrite_depth);
			}
		};

		struct PassLighting : public IPassNode
		{
			PinWrapper read_depth_{};
			PinWrapper read_gbuffer0_{};
			PinWrapper read_gbuffer1_{};

			PinWrapper out_lighting_{};

			PassLighting()
			{
				SetupNode("lighting");

				InitRefPin(read_depth_, "read_depth");
				InitRefPin(read_gbuffer0_, "read_gbuffer0");
				InitRefPin(read_gbuffer1_, "read_gbuffer1");

				InitOutPin(out_lighting_, "lighting");
			}

			void SetupGraph(const PinWrapper& read_depth, const PinWrapper& read_gbuffer0, const PinWrapper& read_gbuffer1)
			{
				read_depth_.connect_from(read_depth);
				read_gbuffer0_.connect_from(read_gbuffer0);
				read_gbuffer1_.connect_from(read_gbuffer1);
			}
		};

		struct PassPost : public IPassNode
		{
			PinWrapper read_lighting_{};

			PinWrapper out_post_{};

			PassPost()
			{
				SetupNode("post");

				InitRefPin(read_lighting_, "read_lighting");

				InitOutPin(out_post_, "post");

				// 参照したバッファを出力としてエイリアス.
				out_post_.connect_from(read_lighting_);
			}

			void SetupGraph(const PinWrapper& read_lighting)
			{
				read_lighting_.connect_from(read_lighting);
			}
		};


		void Test1();

		void Test()
		{
			GpuTaskGraphBuilder builder{};


			auto p_prez = builder.CreateNode<PassPreZ>();

			auto p_gbuffer = builder.CreateNode<PassGbuffer>();
			// prez -> gbuffer.
			p_gbuffer->SetupGraph(p_prez->out_depth_);

			auto p_lighting = builder.CreateNode<PassLighting>();
			// gbuffer -> lighting.
			p_lighting->SetupGraph(p_gbuffer->out_depth_, p_gbuffer->out_gbuffer0_, p_gbuffer->out_gbuffer1_);

			auto p_post = builder.CreateNode<PassPost>();
			// lighting -> post.
			p_post->SetupGraph(p_lighting->out_lighting_);


			Test1();
			
			return;
		}


		void Test1()
		{
			struct TaskDepthPass : public rtg::ITaskNode
			{
				// ノード定義コンストラクタ記述マクロ.
				ITASK_NODE_DEF_BEGIN(TaskDepthPass)
					ITASK_NODE_HANDLE_REGISTER(h_depth_)
				ITASK_NODE_DEF_END

				rtg::ResourceHandle h_depth_{};


				virtual rtg::ETASK_TYPE TaskType() const
				{
					return rtg::ETASK_TYPE::GRAPHICS;
				}

				// リソースとアクセスを定義するプリプロセス.
				void Setup(rtg::RenderTaskGraphBuilder& builder)
				{
					// リソース定義.
					rtg::ResourceDesc2D depth_desc = rtg::ResourceDesc2D::CreateAsRelative(1.0f, 1.0f, rhi::ResourceFormat::Format_D32_FLOAT_S8X24_UINT);

					// リソースアクセス定義.
					h_depth_ = builder.RegisterResourceAccess(*this, builder.CreateResource(depth_desc), rtg::EACCESS_TYPE::DEPTH_TARGET);
				}

				// 実際のレンダリング処理.
				void Run(rtg::RenderTaskGraphBuilder& builder) override
				{
					// builder から リソースハンドル h_depth_ で実リソースを取得する.
				}

			};

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
				void Setup(rtg::RenderTaskGraphBuilder& builder, rtg::ResourceHandle h_depth)
				{
					// リソース定義.
					rtg::ResourceDesc2D gbuffer0_desc = rtg::ResourceDesc2D::CreateAsRelative(1.0f, 1.0f, rhi::ResourceFormat::Format_R8G8B8A8_UNORM);
					rtg::ResourceDesc2D gbuffer1_desc = rtg::ResourceDesc2D::CreateAsRelative(1.0f, 1.0f, rhi::ResourceFormat::Format_R11G11B10_FLOAT);
					
					// リソースアクセス定義.
					h_depth_ = builder.RegisterResourceAccess(*this, h_depth, rtg::EACCESS_TYPE::DEPTH_TARGET);
					h_gb0_ = builder.RegisterResourceAccess(*this, builder.CreateResource(gbuffer0_desc), rtg::EACCESS_TYPE::RENDER_TARTGET);
					h_gb1_ = builder.RegisterResourceAccess(*this, builder.CreateResource(gbuffer1_desc), rtg::EACCESS_TYPE::RENDER_TARTGET);
				}

				// 実際のレンダリング処理.
				void Run(rtg::RenderTaskGraphBuilder& builder) override
				{
					// builder から リソースハンドル out_depth で実リソースを取得する.
				}

			};
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


				virtual rtg::ETASK_TYPE TaskType() const
				{
					return rtg::ETASK_TYPE::GRAPHICS;
				}

				// リソースとアクセスを定義するプリプロセス.
				void Setup(rtg::RenderTaskGraphBuilder& builder, rtg::ResourceHandle h_depth, rtg::ResourceHandle h_gb0, rtg::ResourceHandle h_gb1)
				{
					// リソース定義.
					rtg::ResourceDesc2D light_desc = rtg::ResourceDesc2D::CreateAsRelative(1.0f, 1.0f, rhi::ResourceFormat::Format_R16G16B16A16_FLOAT);
					
					// リソースアクセス定義.
					h_depth_ =	builder.RegisterResourceAccess(*this, h_depth, rtg::EACCESS_TYPE::SHADER_READ);
					h_gb0_ =	builder.RegisterResourceAccess(*this, h_gb0, rtg::EACCESS_TYPE::SHADER_READ);
					h_gb1_ =	builder.RegisterResourceAccess(*this, h_gb1, rtg::EACCESS_TYPE::SHADER_READ);
					h_light_=	builder.RegisterResourceAccess(*this, builder.CreateResource(light_desc), rtg::EACCESS_TYPE::RENDER_TARTGET);
				}

				// 実際のレンダリング処理.
				void Run(rtg::RenderTaskGraphBuilder& builder) override
				{
					// builder から リソースハンドル out_depth で実リソースを取得する.
				}
			};
			struct TaskFinalPass : public rtg::ITaskNode
			{
				// ノード定義コンストラクタ記述マクロ.
				ITASK_NODE_DEF_BEGIN(TaskFinalPass)
					ITASK_NODE_HANDLE_REGISTER(h_depth_)
					ITASK_NODE_HANDLE_REGISTER(h_light_)
					ITASK_NODE_HANDLE_REGISTER(h_final_)
				ITASK_NODE_DEF_END

				rtg::ResourceHandle h_depth_{};
				rtg::ResourceHandle h_light_{};
				rtg::ResourceHandle h_final_{};


				virtual rtg::ETASK_TYPE TaskType() const
				{
					return rtg::ETASK_TYPE::GRAPHICS;
				}

				// リソースとアクセスを定義するプリプロセス.
				void Setup(rtg::RenderTaskGraphBuilder& builder, rtg::ResourceHandle h_depth, rtg::ResourceHandle h_light, rtg::ResourceHandle h_final)
				{
					// リソースアクセス定義.
					h_depth_ = builder.RegisterResourceAccess(*this, h_depth, rtg::EACCESS_TYPE::SHADER_READ);
					h_light_ = builder.RegisterResourceAccess(*this, h_light, rtg::EACCESS_TYPE::SHADER_READ);
					h_final_ = builder.RegisterResourceAccess(*this, h_final, rtg::EACCESS_TYPE::RENDER_TARTGET);
				}

				// 実際のレンダリング処理.
				void Run(rtg::RenderTaskGraphBuilder& builder) override
				{
					// builder から リソースハンドル out_depth で実リソースを取得する.
				}
			};


			rtg::RenderTaskGraphBuilder rtg_builder{};

			auto* task_depth = rtg_builder.CreateNewNodeInSequenceTail<TaskDepthPass>();
			task_depth->Setup(rtg_builder);

			auto* task_gbuffer = rtg_builder.CreateNewNodeInSequenceTail<TaskGBufferPass>();
			task_gbuffer->Setup(rtg_builder, task_depth->h_depth_);

			auto* task_light = rtg_builder.CreateNewNodeInSequenceTail<TaskLightPass>();
			task_light->Setup(rtg_builder, task_gbuffer->h_depth_, task_gbuffer->h_gb0_, task_gbuffer->h_gb1_);

			auto h_swapchain = rtg_builder.GetSwapchainResourceHandle();// Swapchain.
			auto* task_final = rtg_builder.CreateNewNodeInSequenceTail<TaskFinalPass>();
			task_final->Setup(rtg_builder, task_light->h_depth_, task_light->h_light_, h_swapchain);

			rtg_builder.Compile();

			return;
		}
	}






	// LinearDepth生成ComputePass.
	class PassGenLinearDepth
	{
	public:

		bool Initialize(rhi::DeviceDep* p_device)
		{			
			// 初期化. シェーダバイナリの要求とPSO生成.

			auto k_shader_model = "6_3";

			auto& ResourceMan = ngl::res::ResourceManager::Instance();

			ngl::gfx::ResShader::LoadDesc loaddesc = {};
			loaddesc.entry_point_name = "main_cs";
			loaddesc.stage = ngl::rhi::ShaderStage::Compute;
			loaddesc.shader_model_version = k_shader_model;
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



	class PassFinalComposite
	{
	public:

		bool Initialize(rhi::DeviceDep* p_device, rhi::ResourceFormat format)
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


			pso_ = new rhi::GraphicsPipelineStateDep();
			ngl::rhi::GraphicsPipelineStateDep::Desc desc = {};
			desc.vs = &res_shader_vs->data_;
			desc.ps = &res_shader_ps->data_;

			desc.num_render_targets = 1;
			desc.render_target_formats[0] = format;

			desc.blend_state.target_blend_states[0].blend_enable = false;
			desc.blend_state.target_blend_states[0].write_mask = ~ngl::u8(0);

			if (!pso_->Initialize(p_device, desc))
			{
				assert(false);
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

			auto cbv_scene_view = builder.GetFrameCbv("scene_view"); assert(cbv_scene_view.IsValid());

			auto tex_linear_depth = builder.GetFrameTexture("linear_depth"); assert(tex_linear_depth.IsValid());
			auto srv_linear_depth = builder.GetFrameSrv("linear_depth"); assert(srv_linear_depth.IsValid());

			auto srv_rt_test = builder.GetFrameSrv("rt_test_buffer"); assert(srv_rt_test.IsValid());

			auto samp = builder.GetFrameSamp("samp_linear"); assert(samp.IsValid());

			auto tex_work = builder.GetFrameTexture("tex_work"); assert(tex_work.IsValid());
			auto rtv_work = builder.GetFrameRtv("tex_work"); assert(rtv_work.IsValid());


			// Viewport and Scissor.
			ngl::gfx::helper::SetFullscreenViewportAndScissor(p_command_list, tex_work->GetWidth(), tex_work->GetHeight());

			// Rtv, Dsv セット.
			{
				const auto* p_rtv = rtv_work.Get();
				p_command_list->SetRenderTargets(&p_rtv, 1, nullptr);
			}

			p_command_list->SetPipelineState(pso_.Get());
			ngl::rhi::DescriptorSetDep desc_set = {};
			pso_->SetDescriptorHandle(&desc_set, "tex_lineardepth", srv_linear_depth->GetView().cpu_handle);
			pso_->SetDescriptorHandle(&desc_set, "tex_rt", srv_rt_test->GetView().cpu_handle);
			pso_->SetDescriptorHandle(&desc_set, "samp", samp->GetView().cpu_handle);
			p_command_list->SetDescriptorSet(pso_.Get(), &desc_set);

			p_command_list->DrawInstanced(3, 1, 0, 0);
		}

	private:
		rhi::RhiRef<rhi::GraphicsPipelineStateDep> pso_;
	};

	// フルスクリーンコピーパス.
	class PassCopyFullscreen
	{
	public:

		bool Initialize(rhi::DeviceDep* p_device, rhi::ResourceFormat format, const char* target_res_name, const char* src_res_name)
		{
			target_res_name_ = target_res_name;
			src_res_name_ = src_res_name;

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
			auto res_shader_ps = ResourceMan.LoadResource<ngl::gfx::ResShader>(p_device, "./src/ngl/data/shader/screen/copy_tex_to_screen_ps.hlsl", &loaddesc_ps);


			pso_ = new rhi::GraphicsPipelineStateDep();
			ngl::rhi::GraphicsPipelineStateDep::Desc desc = {};
			desc.vs = &res_shader_vs->data_;
			desc.ps = &res_shader_ps->data_;

			desc.num_render_targets = 1;
			desc.render_target_formats[0] = format;

			desc.blend_state.target_blend_states[0].blend_enable = false;
			desc.blend_state.target_blend_states[0].write_mask = ~ngl::u8(0);

			if (!pso_->Initialize(p_device, desc))
			{
				assert(false);
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

			auto srv_src = builder.GetFrameSrv(src_res_name_); assert(srv_src.IsValid());

			auto samp = builder.GetFrameSamp("samp_linear"); assert(samp.IsValid());

			auto rtv_target = builder.GetFrameRtv(target_res_name_); assert(rtv_target.IsValid());
			uint32_t target_w{}, target_h{};
			// ターゲットサイズを取得. swapchain の場合は専用に取得.
			if (target_res_name_ == GraphResouceNameText("swapchain"))
			{
				auto tmp = builder.GetSwapchain();
				assert(tmp.IsValid());
				target_w = tmp->GetWidth();
				target_h = tmp->GetHeight();
			}
			else
			{
				auto tmp = builder.GetFrameTexture(target_res_name_);
				assert(tmp.IsValid());
				target_w = tmp->GetWidth();
				target_h = tmp->GetHeight();
			}


			// Viewport and Scissor.
			ngl::gfx::helper::SetFullscreenViewportAndScissor(p_command_list, target_w, target_h);

			// Rtv, Dsv セット.
			{
				const auto* p_rtv = rtv_target.Get();
				p_command_list->SetRenderTargets(&p_rtv, 1, nullptr);
			}

			p_command_list->SetPipelineState(pso_.Get());
			ngl::rhi::DescriptorSetDep desc_set = {};
			pso_->SetDescriptorHandle(&desc_set, "tex", srv_src->GetView().cpu_handle);
			pso_->SetDescriptorHandle(&desc_set, "samp", samp->GetView().cpu_handle);
			p_command_list->SetDescriptorSet(pso_.Get(), &desc_set);

			p_command_list->DrawInstanced(3, 1, 0, 0);
		}

	private:
		rhi::RhiRef<rhi::GraphicsPipelineStateDep> pso_;

		GraphResouceNameText src_res_name_;
		GraphResouceNameText target_res_name_;
	};
}
