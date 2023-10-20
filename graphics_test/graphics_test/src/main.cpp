
#include <iostream>
#include <vector>
#include <array>
#include <unordered_map>
#include <memory>

#include <thread>


#include "ngl/boot/boot_application.h"
#include "ngl/platform/window.h"
#include "ngl/util/time/timer.h"
#include "ngl/memory/tlsf_memory_pool.h"
#include "ngl/memory/tlsf_allocator.h"
#include "ngl/file/file.h"

#include "ngl/math/math.h"

// resource
#include "ngl/resource/resource_manager.h"

// rhi
#include "ngl/rhi/d3d12/device.d3d12.h"
#include "ngl/rhi/d3d12/command_list.d3d12.h"
#include "ngl/rhi/d3d12/descriptor.d3d12.h"
#include "ngl/rhi/d3d12/resource.d3d12.h"
#include "ngl/rhi/d3d12/resource_view.d3d12.h"


// gfx
#include "ngl/gfx/common_struct.h"
#include "ngl/gfx/raytrace_scene.h"
#include "ngl/gfx/mesh_component.h"

#include "ngl/thread/lockfree_stack_intrusive.h"
#include "ngl/thread/lockfree_stack_intrusive_test.h"

#include "ngl/gfx/command_helper.h"

#include "ngl/gfx/render/mesh_renderer.h"


#include "ngl/render/graph_builder.h"
#include "ngl/render/test_pass.h"

// test
#include "test/test.h"



// アプリ本体.
class AppGame : public ngl::boot::ApplicationBase
{
public:
	AppGame();
	~AppGame();

	bool Initialize() override;
	bool Execute() override;

private:
	double						app_sec_ = 0.0f;
	double						frame_sec_ = 0.0f;

	ngl::platform::CoreWindow	window_;

	ngl::math::Vec3		camera_pos_ = {0.0f, 2.0f, -1.0f};
	ngl::math::Mat33	camera_pose_ = ngl::math::Mat33::Identity();
	float				camera_fov_y = ngl::math::Deg2Rad(60.0f);


	ngl::rhi::DeviceDep							device_;
	ngl::rhi::GraphicsCommandQueueDep			graphics_queue_;
	

	// CommandQueue実行完了待機用Fence
	ngl::rhi::FenceDep							wait_fence_;
	// Fenceの初期値がゼロであるため待機用の値は1から開始.
	ngl::types::u64								wait_fence_value_ = 1;
	// CommandQueue実行完了待機用オブジェクト
	ngl::rhi::WaitOnFenceSignalDep				wait_signal_;

	ngl::rhi::RhiRef<ngl::rhi::GraphicsCommandListDep>	gfx_command_list_;

	// SwapChain
	ngl::rhi::RhiRef<ngl::rhi::SwapChainDep>	swapchain_;
	std::vector<ngl::rhi::RefRtvDep>			swapchain_rtvs_;
	std::vector<ngl::rhi::ResourceState>		swapchain_resource_state_;


	ngl::rhi::RefTextureDep						tex_work_;
	ngl::rhi::RefRtvDep							tex_work_rtv_;
	ngl::rhi::RefSrvDep							tex_work_srv_;

	ngl::rhi::RefTextureDep						tex_depth_;
	ngl::rhi::RefDsvDep							tex_depth_dsv_;
	ngl::rhi::RefSrvDep							tex_depth_srv_;
	ngl::rhi::ResourceState						tex_depth_state_;


	ngl::rhi::RefTextureDep						tex_lineardepth_;
	ngl::rhi::RefSrvDep							tex_lineardepth_srv_;
	ngl::rhi::RefUavDep							tex_lineardepth_uav_;


	ngl::rhi::RefTextureDep						tex_rw_;
	ngl::rhi::RefSrvDep							tex_rw_srv_;
	ngl::rhi::RefUavDep							tex_rw_uav_;

	ngl::res::ResourceHandle <ngl::gfx::ResShader> res_shader_vs_mesh_simple_depth;
	ngl::res::ResourceHandle <ngl::gfx::ResShader> res_shader_ps_mesh_simple_depth;


	ngl::rhi::RefSampDep						samp_linear_clamp_;


	std::array<ngl::rhi::RefBufferDep, 2>		cb_sceneview_;
	std::array<ngl::rhi::RefCbvDep, 2>			cbv_sceneview_;
	int											flip_index_sceneview_ = 0;


	ngl::rhi::GraphicsPipelineStateDep			pso_mesh_simple_depth;

	// RtScene.
	ngl::gfx::RtSceneManager					rt_st_;
	// RtPass.
	ngl::gfx::RtPassTest						rt_pass_test;
	
	// -----------------------------------------------------------------------------------
	// Pass

	ngl::render::PassGenLinearDepth				pass_gen_linear_depth;
	ngl::render::PassFinalComposite				pass_final_composite;
	ngl::render::PassCopyFullscreen				pass_copy_to_swapchain;
	// -----------------------------------------------------------------------------------

	std::vector<std::shared_ptr<ngl::gfx::StaticMeshComponent>>	mesh_comp_array_;
	std::vector<ngl::gfx::StaticMeshComponent*>	test_move_mesh_comp_array_;
};


int main()
{
	std::cout << "Hello World!" << std::endl;
	ngl::time::Timer::Instance().StartTimer("AppGameTime");

	{
		std::unique_ptr<ngl::boot::BootApplication> boot(ngl::boot::BootApplication::Create());
		AppGame app;
		boot->Run(&app);
	}

	std::cout << "App Time: " << ngl::time::Timer::Instance().GetElapsedSec("AppGameTime") << std::endl;
	return 0;
}


AppGame::AppGame()
{
}
AppGame::~AppGame()
{
	// リソース参照クリア.
	mesh_comp_array_.clear();

	// リソースマネージャから全て破棄.
	ngl::res::ResourceManager::Instance().ReleaseCacheAll();


	gfx_command_list_.Reset();
	swapchain_.Reset();


	graphics_queue_.Finalize();
	device_.Finalize();
}

bool AppGame::Initialize()
{
	constexpr auto scree_w = 1280;
	constexpr auto scree_h = 720;

	// ウィンドウ作成
	if (!window_.Initialize(_T("Test Window"), scree_w, scree_h))
	{
		return false;
	}


	ngl::rhi::DeviceDep::Desc device_desc = {};
	device_desc.enable_debug_layer = true;	// デバッグレイヤ
	device_desc.frame_descriptor_size = 500000;
	device_desc.persistent_descriptor_size = 500000;
	if (!device_.Initialize(&window_, device_desc))
	{
		std::cout << "[ERROR] Device Initialize" << std::endl;
		return false;
	}

	// Raytracing Support Check.
	if (!device_.IsSupportDxr())
	{
		MessageBoxA(window_.Dep().GetWindowHandle(), "Raytracing is not supported on this device.", "Info", MB_OK);
	}

	if (!graphics_queue_.Initialize(&device_))
	{
		std::cout << "[ERROR] Command Queue Initialize" << std::endl;
		return false;
	}
	{
		ngl::rhi::SwapChainDep::Desc swap_chain_desc;
		swap_chain_desc.format = ngl::rhi::ResourceFormat::Format_R10G10B10A2_UNORM;
		swapchain_ = new ngl::rhi::SwapChainDep();
		if (!swapchain_->Initialize(&device_, &graphics_queue_, swap_chain_desc))
		{
			std::cout << "[ERROR] SwapChain Initialize" << std::endl;
			return false;
		}

		swapchain_rtvs_.resize(swapchain_->NumResource());
		swapchain_resource_state_.resize(swapchain_->NumResource());
		for (auto i = 0u; i < swapchain_->NumResource(); ++i)
		{
			swapchain_rtvs_[i] = new ngl::rhi::RenderTargetViewDep();
			swapchain_rtvs_[i]->Initialize(&device_, swapchain_.Get(), i);
			swapchain_resource_state_[i] = ngl::rhi::ResourceState::Common;// Swapchain初期ステートは指定していないためCOMMON状態.
		}
	}

	// DepthBuffer
	{
		ngl::rhi::TextureDep::Desc desc = {};
		desc.bind_flag = ngl::rhi::ResourceBindFlag::DepthStencil | ngl::rhi::ResourceBindFlag::ShaderResource;
		desc.format = ngl::rhi::ResourceFormat::Format_D32_FLOAT;
		desc.type = ngl::rhi::TextureType::Texture2D;
		desc.width = scree_w;
		desc.height = scree_h;
		desc.depth_stencil.clear_value = 0.0f;// DepthBufferクリア既定値. ReverseZ.

		tex_depth_ = new ngl::rhi::TextureDep();
		if (!tex_depth_->Initialize(&device_, desc))
		{
			std::cout << "[ERROR] Create DepthTexture Initialize" << std::endl;
			assert(false);
		}
		tex_depth_dsv_ = new ngl::rhi::DepthStencilViewDep();
		if (!tex_depth_dsv_->Initialize(&device_, tex_depth_.Get(), 0, 0, 1))
		{
			std::cout << "[ERROR] Create Dsv Initialize" << std::endl;
			assert(false);
		}
		tex_depth_srv_ = new ngl::rhi::ShaderResourceViewDep();
		if (!tex_depth_srv_->InitializeAsTexture(&device_, tex_depth_.Get(), 0, 1, 0, 1))
		{
			assert(false);
		}

		tex_depth_state_ = desc.initial_state;
	}

	// Work RenderBuffer.
	{
		ngl::rhi::TextureDep::Desc desc = {};
		desc.bind_flag = ngl::rhi::ResourceBindFlag::RenderTarget | ngl::rhi::ResourceBindFlag::ShaderResource;
		desc.format = ngl::rhi::ResourceFormat::Format_R16G16B16A16_FLOAT;
		desc.type = ngl::rhi::TextureType::Texture2D;
		desc.width = scree_w;
		desc.height = scree_h;
		desc.initial_state = ngl::rhi::ResourceState::ShaderRead;

		tex_work_ = new ngl::rhi::TextureDep();
		if (!tex_work_->Initialize(&device_, desc))
		{
			assert(false);
		}
		tex_work_rtv_ = new ngl::rhi::RenderTargetViewDep();
		if (!tex_work_rtv_->Initialize(&device_, tex_work_.Get(), 0, 0, 1))
		{
			assert(false);
		}
		tex_work_srv_ = new ngl::rhi::ShaderResourceViewDep();
		if (!tex_work_srv_->InitializeAsTexture(&device_, tex_work_.Get(), 0, 1, 0, 1))
		{
			assert(false);
		}
	}

	// LinearDepth Texture.
	{
		ngl::rhi::TextureDep::Desc desc = {};
		desc.bind_flag = ngl::rhi::ResourceBindFlag::ShaderResource | ngl::rhi::ResourceBindFlag::UnorderedAccess;
		desc.format = ngl::rhi::ResourceFormat::Format_R32_FLOAT;
		desc.type = ngl::rhi::TextureType::Texture2D;
		desc.width = scree_w;
		desc.height = scree_h;
		desc.initial_state = ngl::rhi::ResourceState::ShaderRead;

		tex_lineardepth_ = new ngl::rhi::TextureDep();
		if (!tex_lineardepth_->Initialize(&device_, desc))
		{
			assert(false);
		}
		tex_lineardepth_srv_ = new ngl::rhi::ShaderResourceViewDep();
		if (!tex_lineardepth_srv_->InitializeAsTexture(&device_, tex_lineardepth_.Get(), 0, 1, 0, 1))
		{
			assert(false);
		}
		tex_lineardepth_uav_ = new ngl::rhi::UnorderedAccessViewDep();
		if (!tex_lineardepth_uav_->Initialize(&device_, tex_lineardepth_.Get(), 0, 0, 1))
		{
			assert(false);
		}
	}

	// UnorderedAccess Texture.
	{
		ngl::rhi::TextureDep::Desc desc = {};
		desc.bind_flag = ngl::rhi::ResourceBindFlag::UnorderedAccess | ngl::rhi::ResourceBindFlag::ShaderResource;
		desc.format = ngl::rhi::ResourceFormat::Format_R8G8B8A8_UNORM;
		desc.type = ngl::rhi::TextureType::Texture2D;
		desc.width = scree_w;
		desc.height = scree_h;
		desc.initial_state = ngl::rhi::ResourceState::ShaderRead;

		tex_rw_ = new ngl::rhi::TextureDep();
		if (!tex_rw_->Initialize(&device_, desc))
		{
			std::cout << "[ERROR] Create RW Texture Initialize" << std::endl;
			assert(false);
		}
		tex_rw_srv_ = new ngl::rhi::ShaderResourceViewDep();
		if (!tex_rw_srv_->InitializeAsTexture(&device_, tex_rw_.Get(), 0, 1, 0, 1))
		{
			std::cout << "[ERROR] Create RW SRV" << std::endl;
			assert(false);
		}
		tex_rw_uav_ = new ngl::rhi::UnorderedAccessViewDep();
		if (!tex_rw_uav_->Initialize(&device_, tex_rw_.Get(), 0, 0, 1))
		{
			std::cout << "[ERROR] Create RW UAV" << std::endl;
			assert(false);
		}
	}

	ngl::rhi::GraphicsCommandListDep::Desc gcl_desc = {};
	gfx_command_list_ = new ngl::rhi::GraphicsCommandListDep();
	if (!gfx_command_list_->Initialize(&device_, gcl_desc))
	{
		std::cout << "[ERROR] CommandList Initialize" << std::endl;
		return false;
	}

	if (!wait_fence_.Initialize(&device_))
	{
		std::cout << "[ERROR] Fence Initialize" << std::endl;
		return false;
	}

	

	// Shader Load.
	{
		auto& ResourceMan = ngl::res::ResourceManager::Instance();

		static const char* k_shader_model = "6_3";

		{
			ngl::gfx::ResShader::LoadDesc loaddesc = {};
			loaddesc.entry_point_name = "main_vs";
			loaddesc.stage = ngl::rhi::ShaderStage::Vertex;
			loaddesc.shader_model_version = k_shader_model;
			res_shader_vs_mesh_simple_depth= ResourceMan.LoadResource<ngl::gfx::ResShader>(&device_, "./src/ngl/data/shader/mesh/mesh_simple_depth_vs.hlsl", &loaddesc);
		}
		{
			ngl::gfx::ResShader::LoadDesc loaddesc = {};
			loaddesc.entry_point_name = "main_ps";
			loaddesc.stage = ngl::rhi::ShaderStage::Pixel;
			loaddesc.shader_model_version = k_shader_model;
			res_shader_ps_mesh_simple_depth = ResourceMan.LoadResource<ngl::gfx::ResShader>(&device_, "./src/ngl/data/shader/mesh/mesh_simple_depth_ps.hlsl", &loaddesc);
		}
	}

	{
		// Sampler.
		ngl::rhi::SamplerDep::Desc samp_desc = {};
		samp_desc.Filter = ngl::rhi::TextureFilterMode::Min_Linear_Mag_Linear_Mip_Linear;
		samp_desc.AddressU = ngl::rhi::TextureAddressMode::Clamp;
		samp_desc.AddressV = ngl::rhi::TextureAddressMode::Clamp;
		samp_desc.AddressW = ngl::rhi::TextureAddressMode::Clamp;
		samp_linear_clamp_ = new ngl::rhi::SamplerDep();
		if (!samp_linear_clamp_->Initialize(&device_, samp_desc))
		{
			std::cout << "[ERROR] Create rhi::SamplerDep" << std::endl;
			assert(false);
		}
	}

	// MeshRendering Shader.
	{
		// SceneView Constant.
		for (int i = 0; i < cb_sceneview_.size(); ++i)
		{
			cb_sceneview_[i] = new ngl::rhi::BufferDep();
			cbv_sceneview_[i] = new ngl::rhi::ConstantBufferViewDep();

			ngl:: rhi::BufferDep::Desc desc = {};
			desc.SetupAsConstantBuffer(sizeof(ngl::gfx::CbSceneView));
			cb_sceneview_[i]->Initialize(&device_, desc);

			ngl::rhi::ConstantBufferViewDep::Desc cbv_desc = {};
			cbv_sceneview_[i]->Initialize(cb_sceneview_[i].Get(), cbv_desc);
		}

		// Depth Prepass.
		{
			ngl::rhi::GraphicsPipelineStateDep::Desc desc = {};
			desc.vs = &res_shader_vs_mesh_simple_depth->data_;
			desc.ps = &res_shader_ps_mesh_simple_depth->data_;

			desc.depth_stencil_state.depth_enable = true;
			desc.depth_stencil_state.depth_func = ngl::rhi::CompFunc::Greater; // ReverseZ.
			desc.depth_stencil_state.depth_write_mask = ~ngl::u32(0);
			desc.depth_stencil_state.stencil_enable = false;
			desc.depth_stencil_format = tex_depth_->GetFormat();


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
			if (!pso_mesh_simple_depth.Initialize(&device_, desc))
			{
				assert(false);
			}
		}
	}

	{
		// LinearDepth生成パス.
		if (!pass_gen_linear_depth.Initialize(&device_))
		{
			assert(false);
		}

		// 最終画面生成パス.
		if (!pass_final_composite.Initialize(&device_, tex_work_->GetFormat()))
		{
			assert(false);
		}

		// Swapchainへのコピーパス.
		if (!pass_copy_to_swapchain.Initialize(&device_, swapchain_->GetDesc().format, "swapchain", "tex_work"))
		{
			assert(false);
		}
	}

	{
		const char* mesh_file_box = "../third_party/assimp/test/models/FBX/box.fbx";
		const char* mesh_file_spider = "../third_party/assimp/test/models/FBX/spider.fbx";
		
		//const char* mesh_file_sponza = "./data/model/sponza/sponza.obj";
		const char* mesh_file_sponza = "./data/model/sponza_gltf/glTF/Sponza.gltf";

		auto& ResourceMan = ngl::res::ResourceManager::Instance();

		// Mesh Component
		{
			{
				auto mc = std::make_shared<ngl::gfx::StaticMeshComponent>();
				mc->Initialize(&device_);
				mesh_comp_array_.push_back(mc);

				ngl::gfx::ResMeshData::LoadDesc loaddesc = {};
				mc->SetMeshData(ResourceMan.LoadResource<ngl::gfx::ResMeshData>(&device_, mesh_file_sponza, &loaddesc));
				mc->transform_.SetColumn3(ngl::math::Vec3(0, 0, 0)).SetDiagonal(ngl::math::Vec3(0.1f));
			}

			for(int i = 0; i < 100; ++i)
			{
				auto mc = std::make_shared<ngl::gfx::StaticMeshComponent>();
				mc->Initialize(&device_);
				mesh_comp_array_.push_back(mc);
				ngl::gfx::ResMeshData::LoadDesc loaddesc = {};
				mc->SetMeshData(ResourceMan.LoadResource<ngl::gfx::ResMeshData>(&device_, mesh_file_spider, &loaddesc));



				constexpr int k_rand_f_div = 10000;
				const float randx = (std::rand() % k_rand_f_div) / (float)k_rand_f_div;
				const float randz = (std::rand() % k_rand_f_div) / (float)k_rand_f_div;
				const float randroty = (std::rand() % k_rand_f_div) / (float)k_rand_f_div;

				constexpr float placement_range = 200.0f;
				

				ngl::math::Mat44 tr = ngl::math::Mat44::Identity();
				tr.SetDiagonal(ngl::math::Vec4(0.1f));
				tr = ngl::math::Mat44::RotAxisY(randroty * ngl::math::k_pi_f * 2.0f) * tr;
				tr.SetColumn3(ngl::math::Vec4(placement_range* (randx * 2.0f - 1.0f), 0, placement_range* (randz * 2.0f - 1.0f), 1.0f));

				mc->transform_ = ngl::math::Mat34(tr);

				// 移動テスト用に追加.
				test_move_mesh_comp_array_.push_back(mc.get());
			}

			{
				// 即時破棄をしても内部でのRenderThread初期化処理リストでの参照保持等が正常に動作するか確認するため.
				ngl::gfx::ResMeshData::LoadDesc loaddesc = {};
				auto immediate_destroy_resmesh = ResourceMan.LoadResource<ngl::gfx::ResMeshData>(&device_, mesh_file_box, &loaddesc);
				immediate_destroy_resmesh = {};
			}
		}
	}

	{

		// AS他.
		if (!rt_st_.Initialize(&device_))
		{
			std::cout << "[ERROR] Create gfx::RtSceneManager" << std::endl;
			assert(false);
		}

		// RtPass.
		if(!rt_pass_test.Initialize(&device_, sizeof(float) * 4, sizeof(float) * 2, 1))
		{
			assert(false);
		}
	}


	// テストコード
	ngl_test::TestFunc00(&device_);

	ngl::render::graph::Test(device_);

	ngl::time::Timer::Instance().StartTimer("app_frame_sec");
	return true;
}

// メインループから呼ばれる
bool AppGame::Execute()
{
	// ウィンドウが無効になったら終了
	if (!window_.IsValid())
	{
		return false;
	}
	{
		frame_sec_ = ngl::time::Timer::Instance().GetElapsedSec("app_frame_sec");
		app_sec_ += frame_sec_;

		// 再スタート
		ngl::time::Timer::Instance().StartTimer("app_frame_sec");
	}
	const float delta_sec = static_cast<float>(frame_sec_);

	ngl::u32 screen_w, screen_h;
	window_.Impl()->GetScreenSize(screen_w, screen_h);




	// 操作系.
	{
		float camera_translate_speed = 60.0f;


		const auto mouse_pos = window_.Dep().GetMousePosition();
		const auto mouse_pos_delta = window_.Dep().GetMousePositionDelta();
		const bool mouse_l = window_.Dep().GetMouseLeft();
		const bool mouse_r = window_.Dep().GetMouseRight();
		const bool mouse_m = window_.Dep().GetMouseMiddle();

		{
			const auto mx = std::get<0>(mouse_pos);
			const auto my = std::get<1>(mouse_pos);
			const ngl::math::Vec2 mouse_pos((float)mx, (float)my);

			static bool prev_mouse_r = false;
			if (!prev_mouse_r && mouse_r)
			{
				// MouseR Start.

				// R押下開始でマウス位置固定開始.
				window_.Dep().SetMousePositionRequest(mx, my);
				window_.Dep().SetMousePositionClipInWindow(true);
			}
			if (prev_mouse_r && !mouse_r)
			{
				// MouseR End.

				// R押下終了でマウス位置固定リセット.
				window_.Dep().ResetMousePositionRequest();
				window_.Dep().SetMousePositionClipInWindow(false);
			}

			// UEライクなマウスR押下中にカメラ向きと位置操作(WASD)
			// TODO マウスR押下中に実際のマウス位置を動かさないようにしたい(ウィンドウから出てしまうので)
			if (mouse_r)
			{
				// マウス押下中カーソル移動量(pixel)
				const ngl::math::Vec2 mouse_diff((float)std::get<0>(mouse_pos_delta), (float)std::get<1>(mouse_pos_delta));

				// 向き.
				if(true)
				{
					// 適当に回転量へ.
					const auto rot_rad = ngl::math::k_pi_f * mouse_diff * 0.001f;
					auto rot_yaw = ngl::math::Mat33::RotAxisY(rot_rad.x);
					auto rot_pitch = ngl::math::Mat33::RotAxisX(rot_rad.y);
					// 回転.
					camera_pose_ = camera_pose_ * rot_yaw * rot_pitch;

					// sideベクトルをワールドXZ麺に制限.
					if (0.9999 > std::fabsf(camera_pose_.GetColumn2().y))
					{
						// 視線がY-Axisと不一致なら視線ベクトルとY-Axisから補正.
						const float sign_y = (0.0f < camera_pose_.GetColumn1().y) ? 1.0f : -1.0f;
						auto lx = ngl::math::Vec3::Cross(ngl::math::Vec3::UnitY() * sign_y, camera_pose_.GetColumn2());
						auto ly = ngl::math::Vec3::Cross(camera_pose_.GetColumn2(), lx);
						const auto cam_pose_transpose = ngl::math::Mat33(ngl::math::Vec3::Normalize(lx), ngl::math::Vec3::Normalize(ly), camera_pose_.GetColumn2());
						camera_pose_ = ngl::math::Mat33::Transpose(cam_pose_transpose);
					}
					else
					{
						// 視線がY-Axisと一致か近いならサイドベクトルのY成分潰して補正.
						auto lx = camera_pose_.GetColumn1();
						lx = ngl::math::Vec3({ lx.x, 0.0f, lx.z });
						auto ly = ngl::math::Vec3::Cross(camera_pose_.GetColumn2(), lx);
						const auto cam_pose_transpose = ngl::math::Mat33(ngl::math::Vec3::Normalize(lx), ngl::math::Vec3::Normalize(ly), camera_pose_.GetColumn2());
						camera_pose_ = ngl::math::Mat33::Transpose(cam_pose_transpose);
					}
				}

				// 移動.
				{
					const auto vk_a = 65;// VK_A.
					if (window_.Dep().GetVirtualKeyState()[VK_SPACE])
					{
						camera_pos_ += camera_pose_.GetColumn1() * delta_sec * camera_translate_speed;
					}
					if (window_.Dep().GetVirtualKeyState()[VK_CONTROL])
					{
						camera_pos_ += -camera_pose_.GetColumn1() * delta_sec * camera_translate_speed;
					}
					if (window_.Dep().GetVirtualKeyState()[vk_a + 'w' - 'a'])
					{
						camera_pos_ += camera_pose_.GetColumn2() * delta_sec * camera_translate_speed;
					}
					if (window_.Dep().GetVirtualKeyState()[vk_a + 's' - 'a'])
					{
						camera_pos_ += -camera_pose_.GetColumn2() * delta_sec * camera_translate_speed;
					}
					if (window_.Dep().GetVirtualKeyState()[vk_a + 'd' - 'a'])
					{
						camera_pos_ += camera_pose_.GetColumn0() * delta_sec * camera_translate_speed;
					}
					if (window_.Dep().GetVirtualKeyState()[vk_a + 'a' - 'a'])
					{
						camera_pos_ += -camera_pose_.GetColumn0() * delta_sec * camera_translate_speed;
					}
				}
			}
			prev_mouse_r = mouse_r;
		}
	}

	// オブジェクト操作(適当).
	{
		for (int i = 0; i < test_move_mesh_comp_array_.size(); ++i)
		{
			auto* e = test_move_mesh_comp_array_[i];

			float move_range = (i % 10) / 10.0f;

			const float sin_curve = sinf((float)app_sec_ * 2.0f * ngl::math::k_pi_f * 0.1f * (move_range + 1.0f));

			auto trans = e->transform_.GetColumn3();

			trans.z += sin_curve * 1.0f;

			e->transform_.SetColumn3(trans);

		}
	}


	const float screen_aspect_ratio = (float)swapchain_->GetWidth() / swapchain_->GetHeight();

	{
		ngl::math::Mat34 view_mat = ngl::math::CalcViewMatrix(camera_pos_, camera_pose_.GetColumn2(), camera_pose_.GetColumn1());

		const float fov_y = camera_fov_y;;
		const float aspect_ratio = screen_aspect_ratio;
		const float near_z = 0.1f;
		const float far_z = 10000.0f;

		// Infinite Far Reverse Perspective
		ngl::math::Mat44 proj_mat = ngl::math::CalcReverseInfiniteFarPerspectiveMatrix(fov_y, aspect_ratio, 0.1f);
		ngl::math::Vec4 ndc_z_to_view_z_coef = ngl::math::CalcViewDepthReconstructCoefForInfiniteFarReversePerspective(near_z);

		flip_index_sceneview_ = (flip_index_sceneview_ + 1) % 2;
		if (auto* mapped = (ngl::gfx::CbSceneView*)cb_sceneview_[flip_index_sceneview_]->Map())
		{
			mapped->cb_view_mtx = view_mat;
			mapped->cb_proj_mtx = proj_mat;
			mapped->cb_view_inv_mtx = ngl::math::Mat34::Inverse(view_mat);
			mapped->cb_proj_inv_mtx = ngl::math::Mat44::Inverse(proj_mat);

			mapped->cb_ndc_z_to_view_z_coef = ndc_z_to_view_z_coef;

			cb_sceneview_[flip_index_sceneview_]->Unmap();
		}
	}


	// 描画用シーン情報.
	ngl::gfx::SceneRepresentation frame_scene = {};
	{
		for (auto& e : mesh_comp_array_)
		{
			// Render更新.
			e->UpdateRenderData();

			// 登録.
			frame_scene.mesh_instance_array_.push_back(e.get());
		}
	}

	// カメラ設定.
	rt_st_.SetCameraInfo(camera_pos_, camera_pose_.GetColumn2(), camera_pose_.GetColumn1(), camera_fov_y, screen_aspect_ratio);


	// RhiRefによるガベコレテスト.
	if(false)
	{
		for (int i = 0; i < 10; ++i)
		{
			// RhiRefで保持してそのまま破棄することで安全に遅延破棄される(はず).
			ngl::rhi::RhiRef<ngl::rhi::BufferDep> buffer_ref(new ngl::rhi::BufferDep());

			// ConstantBuffer作成
			ngl::rhi::BufferDep::Desc buffer_desc = {};
			buffer_desc.heap_type = ngl::rhi::ResourceHeapType::Upload;
			buffer_desc.bind_flag = (int)ngl::rhi::ResourceBindFlag::ConstantBuffer;
			buffer_desc.element_byte_size = 1024;// テスト用に大きめで確保.
			buffer_desc.element_count = 1;

			buffer_ref->Initialize(&device_, buffer_desc);
		}
	}


	// Frame Graph Builder.
	ngl::render::GraphBuilder frame_graph_builder(&device_);
	{
		// リソース登録.
		{
			frame_graph_builder.SetSwapchain(swapchain_);
			frame_graph_builder.AddFrameResource("swapchain", swapchain_rtvs_[swapchain_->GetCurrentBufferIndex()]);


			frame_graph_builder.AddFrameResource("hardware_depth", tex_depth_);
			frame_graph_builder.AddFrameResource("hardware_depth", tex_depth_srv_);
			frame_graph_builder.AddFrameResource("hardware_depth", tex_depth_dsv_);


			frame_graph_builder.AddFrameResource("linear_depth", tex_lineardepth_);
			frame_graph_builder.AddFrameResource("linear_depth", tex_lineardepth_srv_);
			frame_graph_builder.AddFrameResource("linear_depth", tex_lineardepth_uav_);


			frame_graph_builder.AddFrameResource("tex_work", tex_work_);
			frame_graph_builder.AddFrameResource("tex_work", tex_work_rtv_);
			frame_graph_builder.AddFrameResource("tex_work", tex_work_srv_);

			frame_graph_builder.AddFrameResource("rt_test_buffer", rt_pass_test.ray_result_srv_);


			frame_graph_builder.AddFrameResource("samp_linear", samp_linear_clamp_);


			frame_graph_builder.AddFrameResource("scene_view", cbv_sceneview_[flip_index_sceneview_]);
		}

		// Passセットアップ.
		pass_gen_linear_depth.Setup(frame_graph_builder);
		pass_final_composite.Setup(frame_graph_builder);
		pass_copy_to_swapchain.Setup(frame_graph_builder);
	}


	// Render Loop
	{
		// Deviceのフレーム準備
		device_.ReadyToNewFrame();

		const auto swapchain_index = swapchain_->GetCurrentBufferIndex();
		{
			gfx_command_list_->Begin();

			// CommandList に最初にResourceManagerの処理を積み込み.
			ngl::res::ResourceManager::Instance().UpdateResourceOnRender(&device_, gfx_command_list_.Get());

			// Rt.
			{
				// RtScene更新.
				rt_st_.UpdateOnRender(&device_, gfx_command_list_.Get(), frame_scene);
				
				// RtPass 更新.
				rt_pass_test.PreRenderUpdate(&rt_st_, gfx_command_list_.Get());
				
				// RtPass Render.
				rt_pass_test.Render(gfx_command_list_.Get());
			}

			if(false)
			{
				// 旧テスト描画パスバージョン.

				// Barrier.
				{
					// Dsv State
					gfx_command_list_->ResourceBarrier(tex_depth_.Get(), tex_depth_state_, ngl::rhi::ResourceState::DepthWrite);
					tex_depth_state_ = ngl::rhi::ResourceState::DepthWrite;
				}

				// Dsvクリア.
				gfx_command_list_->ClearDepthTarget(tex_depth_dsv_.Get(), 0.0f, 0, true, false);
				
				// Mesh Raster Render.
				// DepthPrepass.
				{
					auto* p_depth = tex_depth_.Get();
					auto* p_depth_view = tex_depth_dsv_.Get();
						
					// Set RenderTarget.
					gfx_command_list_->SetRenderTargets(nullptr, 0, p_depth_view);

					// Set Viewport and Scissor.
					ngl::gfx::helper::SetFullscreenViewportAndScissor(gfx_command_list_.Get(), p_depth->GetWidth(), p_depth->GetHeight());

					// Mesh Rendering.
					ngl::gfx::RenderMeshSinglePso(*gfx_command_list_, pso_mesh_simple_depth, frame_scene.mesh_instance_array_, *cbv_sceneview_[flip_index_sceneview_].Get());
				}

				// Gen LinearDepth.
				{
					// Barrier.
					{
						gfx_command_list_->ResourceBarrier(tex_depth_.Get(), ngl::rhi::ResourceState::DepthWrite, ngl::rhi::ResourceState::ShaderRead);
						gfx_command_list_->ResourceBarrier(tex_lineardepth_.Get(), ngl::rhi::ResourceState::ShaderRead, ngl::rhi::ResourceState::UnorderedAccess);
					}

					// LinearDepth生成Pass実行.
					pass_gen_linear_depth.Execute(frame_graph_builder, gfx_command_list_.Get());

					// Barrier.
					{
						gfx_command_list_->ResourceBarrier(tex_depth_.Get(), ngl::rhi::ResourceState::ShaderRead, ngl::rhi::ResourceState::DepthWrite);
						gfx_command_list_->ResourceBarrier(tex_lineardepth_.Get(), ngl::rhi::ResourceState::UnorderedAccess, ngl::rhi::ResourceState::ShaderRead);
					}
				}



				// 最終レンダリングパス.
				{
					// Barrier.
					{
						// WorkRenderTarget Transition to RenderTarget
						gfx_command_list_->ResourceBarrier(tex_work_.Get(), ngl::rhi::ResourceState::ShaderRead, ngl::rhi::ResourceState::RenderTarget);
					}

					pass_final_composite.Execute(frame_graph_builder, gfx_command_list_.Get());

					// Barrier.
					{
						gfx_command_list_->ResourceBarrier(tex_work_.Get(), ngl::rhi::ResourceState::RenderTarget, ngl::rhi::ResourceState::ShaderRead);
					}
				}

				// Swapchainへのコピーパス.
				{
					// Barrier.
					{
						gfx_command_list_->ResourceBarrier(swapchain_.Get(), swapchain_index, swapchain_resource_state_[swapchain_index], ngl::rhi::ResourceState::RenderTarget);
						swapchain_resource_state_[swapchain_index] = ngl::rhi::ResourceState::RenderTarget;
					}

					pass_copy_to_swapchain.Execute(frame_graph_builder, gfx_command_list_.Get());

					{
						// Swapchain State to Present
						gfx_command_list_->ResourceBarrier(swapchain_.Get(), swapchain_index, swapchain_resource_state_[swapchain_index], ngl::rhi::ResourceState::Present);
						swapchain_resource_state_[swapchain_index] = ngl::rhi::ResourceState::Present;
					}
				}
			}
			else
			{
				// RenderTaskGraphのテスト.

				// Barrier.
				{
					gfx_command_list_->ResourceBarrier(swapchain_.Get(), swapchain_index, swapchain_resource_state_[swapchain_index], ngl::rhi::ResourceState::RenderTarget);
					swapchain_resource_state_[swapchain_index] = ngl::rhi::ResourceState::RenderTarget;
				}

				ngl::render::graph::Test1(device_, gfx_command_list_, 
					cbv_sceneview_[flip_index_sceneview_],
					samp_linear_clamp_, swapchain_->GetDesc().format, swapchain_rtvs_[swapchain_->GetCurrentBufferIndex()]);

				{
					// Swapchain State to Present
					gfx_command_list_->ResourceBarrier(swapchain_.Get(), swapchain_index, swapchain_resource_state_[swapchain_index], ngl::rhi::ResourceState::Present);
					swapchain_resource_state_[swapchain_index] = ngl::rhi::ResourceState::Present;
				}
			}

			gfx_command_list_->End();
		}

		// CommandList Submit
		ngl::rhi::GraphicsCommandListDep* command_lists[] =
		{
			gfx_command_list_.Get()
		};
		graphics_queue_.ExecuteCommandLists(static_cast<unsigned int>(std::size(command_lists)), command_lists);

		// Present
		swapchain_->GetDxgiSwapChain()->Present(1, 0);

		// 完了シグナル
		graphics_queue_.Signal(&wait_fence_, wait_fence_value_);
		// 待機
		{
			wait_signal_.Wait(&wait_fence_, wait_fence_value_);
			++wait_fence_value_;
		}

	}

	return true;
}