﻿
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
#include "ngl/gfx/render/global_render_resource.h"
#include "ngl/gfx/raytrace_scene.h"
#include "ngl/gfx/mesh_component.h"

#include "ngl/render/test_render_path.h"

// マテリアルシェーダ関連.
#include "ngl/gfx/material/material_shader_generator.h"
#include "ngl/gfx/material/material_shader_manager.h"

#include "test/test.h"



// アプリ本体.
class AppGame : public ngl::boot::ApplicationBase
{
public:
	AppGame();
	~AppGame();

	bool Initialize() override;
	bool Execute() override;

	void SyncRender();
	void BeginRender();

private:
	double						app_sec_ = 0.0f;
	double						frame_sec_ = 0.0f;

	ngl::platform::CoreWindow	window_;

	ngl::math::Vec3		camera_pos_ = {0.0f, 2.0f, -1.0f};
	ngl::math::Mat33	camera_pose_ = ngl::math::Mat33::Identity();
	float				camera_fov_y = ngl::math::Deg2Rad(60.0f);


	ngl::rhi::DeviceDep							device_;
	ngl::rhi::GraphicsCommandQueueDep			graphics_queue_;
	ngl::rhi::ComputeCommandQueueDep			compute_queue_;

	// RenderTaskGraphのCompileやそれらが利用するリソースプール管理.
	ngl::rtg::RenderTaskGraphManager rtg_manager_{};
	

	// CommandQueue実行完了待機用Fence
	ngl::rhi::FenceDep							wait_fence_;
	// CommandQueue実行完了待機用オブジェクト
	ngl::rhi::WaitOnFenceSignalDep				wait_signal_;
	
	ngl::rhi::FenceDep							test_compute_to_gfx_fence_;
	
	ngl::rhi::RhiRef<ngl::rhi::GraphicsCommandListDep>	gfx_frame_begin_command_list_;
	ngl::rhi::RhiRef<ngl::rhi::ComputeCommandListDep>	compute_command_list_;

	// SwapChain
	ngl::rhi::RhiRef<ngl::rhi::SwapChainDep>	swapchain_;
	std::vector<ngl::rhi::RefRtvDep>			swapchain_rtvs_;
	std::vector<ngl::rhi::EResourceState>		swapchain_resource_state_;

	ngl::rhi::RefTextureDep						tex_rw_;
	ngl::rhi::RefSrvDep							tex_rw_srv_;
	ngl::rhi::RefUavDep							tex_rw_uav_;
	
	// Loaded Texture.
	ngl::res::ResourceHandle<ngl::gfx::ResTexture> res_texture_ = {};

	
	// RtScene.
	ngl::gfx::RtSceneManager					rt_scene_;
	// RtPass.
	ngl::gfx::RtPassTest						rt_pass_test;
	
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

	// Material Shader Manager 破棄.
	ngl::gfx::MaterialShaderManager::Instance().Finalize();
	
	ngl::gfx::GlobalRenderResource::Instance().Finalize();
	
	// リソースマネージャから全て破棄.
	ngl::res::ResourceManager::Instance().ReleaseCacheAll();


	gfx_frame_begin_command_list_.Reset();
	compute_command_list_.Reset();
	swapchain_.Reset();


	graphics_queue_.Finalize();
	compute_queue_.Finalize();
	device_.Finalize();
}

bool AppGame::Initialize()
{
	constexpr auto scree_h = 1080;
	constexpr auto scree_w = scree_h * 16/9;

	// ウィンドウ作成
	if (!window_.Initialize(_T("ToyRenderer"), scree_w, scree_h))
	{
		return false;
	}


	ngl::rhi::DeviceDep::Desc device_desc = {};
#if _DEBUG
	device_desc.enable_debug_layer = true;	// デバッグレイヤ
#endif
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
		std::cout << "[ERROR] Graphics Command Queue Initialize" << std::endl;
		return false;
	}
	if(!compute_queue_.Initialize(&device_))
	{
		std::cout << "[ERROR] Compute Command Queue Initialize" << std::endl;
		return false;
	}
	{
		ngl::rhi::SwapChainDep::Desc swap_chain_desc;
		swap_chain_desc.format = ngl::rhi::EResourceFormat::Format_R10G10B10A2_UNORM;
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
			swapchain_resource_state_[i] = ngl::rhi::EResourceState::Common;// Swapchain初期ステートは指定していないためCOMMON状態.
		}
	}

	// MaterialShaderFile生成.
	//	実際は事前生成すべきだが起動時にテスト実行.
	constexpr char k_material_shader_file_dir[] = "./src/ngl/data/shader/material/generated";
	ngl::gfx::MaterialShaderFileGenerator mtl_gen = {};
	mtl_gen.GenerateMaterialShaderFiles(
		"./src/ngl/data/shader/material/impl",
		"./src/ngl/data/shader/material/pass",
		k_material_shader_file_dir);
	
	// Material Shader Manager Setup.
	{
		// Material PSO Creatorを登録.
		{
			ngl::gfx::MaterialShaderManager::Instance().RegisterPassPsoCreator<ngl::gfx::MaterialPassPsoCreator_depth>();
			ngl::gfx::MaterialShaderManager::Instance().RegisterPassPsoCreator<ngl::gfx::MaterialPassPsoCreator_gbuffer>();
			ngl::gfx::MaterialShaderManager::Instance().RegisterPassPsoCreator<ngl::gfx::MaterialPassPsoCreator_d_shadow>();
			// TODO.
		}

		// 本体のセットアップ.
		ngl::gfx::MaterialShaderManager::Instance().Setup(&device_, k_material_shader_file_dir);
	}

	// デフォルトテクスチャ等の簡易アクセス用クラス初期化.
	{
		if(!ngl::gfx::GlobalRenderResource::Instance().Initialize(&device_))
		{
			assert(false);
			return false;
		}
	}

	// RTGマネージャ初期化.
	{
		rtg_manager_.Init(&device_);
	}

	// UnorderedAccess Texture.
	{
		ngl::rhi::TextureDep::Desc desc = {};
		desc.bind_flag = ngl::rhi::ResourceBindFlag::UnorderedAccess | ngl::rhi::ResourceBindFlag::ShaderResource;
		desc.format = ngl::rhi::EResourceFormat::Format_R16G16B16A16_FLOAT;
		desc.type = ngl::rhi::ETextureType::Texture2D;
		desc.width = scree_w;
		desc.height = scree_h;
		desc.initial_state = ngl::rhi::EResourceState::ShaderRead;

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

	gfx_frame_begin_command_list_ = new ngl::rhi::GraphicsCommandListDep();
	if (!gfx_frame_begin_command_list_->Initialize(&device_))
	{
		std::cout << "[ERROR] Graphics CommandList Initialize" << std::endl;
		assert(false);
		return false;
	}
	compute_command_list_ = new ngl::rhi::ComputeCommandListDep();
	if(!compute_command_list_->Initialize(&device_))
	{
		std::cout << "[ERROR] Compute CommandList Initialize" << std::endl;
		assert(false);
		return false;
	}

	if (!wait_fence_.Initialize(&device_))
	{
		std::cout << "[ERROR] Fence Initialize" << std::endl;
		return false;
	}
	if(!test_compute_to_gfx_fence_.Initialize((&device_)))
	{
		assert(false);
		return false;
	}
	
	{
		const char* mesh_file_box = "./third_party/assimp/test/models/FBX/box.fbx";
		const char* mesh_file_stanford_bunny = "data/model/stanford_bunny/bunny.obj";
		const char* mesh_file_spider = "./third_party/assimp/test/models/FBX/spider.fbx";
		const float spider_base_scale = 0.0001f;
		
		const char* mesh_file_sponza = "./data/model/sponza_gltf/glTF/Sponza.gltf";
		const float sponza_scale = 1.0f;

		// Amazon Lumberyard Bistro.
		const char* mesh_file_bistro = "C:/Users/nagak/Downloads/Bistro_v5_2/Bistro_v5_2/BistroExterior.fbx";
		const float bistro_scale = 1.0f;

		// 基本シーンモデル.
#if 1
		// Sponza.
		const char* mesh_target_scene = mesh_file_sponza;
		const float target_scene_base_scale = sponza_scale;
#else
		// Bistro.
		const char* mesh_target_scene = mesh_file_bistro;
		const float target_scene_base_scale = bistro_scale;
#endif
		
		auto& ResourceMan = ngl::res::ResourceManager::Instance();
		// Mesh Component
		{
			// 基本シーンモデル読み込み.
			{
				auto mc = std::make_shared<ngl::gfx::StaticMeshComponent>();
				mesh_comp_array_.push_back(mc);

				ngl::gfx::ResMeshData::LoadDesc loaddesc = {};
				mc->Initialize(&device_, ResourceMan.LoadResource<ngl::gfx::ResMeshData>(&device_, mesh_target_scene, &loaddesc));
				// スケール設定.
				mc->transform_.SetDiagonal(ngl::math::Vec3(target_scene_base_scale));
			}
			{
				auto mc = std::make_shared<ngl::gfx::StaticMeshComponent>();
				mesh_comp_array_.push_back(mc);
				ngl::gfx::ResMeshData::LoadDesc loaddesc = {};
				mc->Initialize(&device_, ResourceMan.LoadResource<ngl::gfx::ResMeshData>(&device_, mesh_file_spider, &loaddesc));
				
				ngl::math::Mat44 tr = ngl::math::Mat44::Identity();
				tr.SetDiagonal(ngl::math::Vec4(spider_base_scale * 5.0f));
				tr.SetColumn3(ngl::math::Vec4(30.0f, 12.0f, 0.0f, 1.0f));

				mc->transform_ = ngl::math::Mat34(tr);
			}
			
			{
				auto mc = std::make_shared<ngl::gfx::StaticMeshComponent>();
				mesh_comp_array_.push_back(mc);
				ngl::gfx::ResMeshData::LoadDesc loaddesc = {};
				//mc->Initialize(&device_, ResourceMan.LoadResource<ngl::gfx::ResMeshData>(&device_, mesh_file_spider, &loaddesc));
				mc->Initialize(&device_, ResourceMan.LoadResource<ngl::gfx::ResMeshData>(&device_, mesh_file_stanford_bunny, &loaddesc));
				
				ngl::math::Mat44 tr = ngl::math::Mat44::Identity();
				tr.SetDiagonal(ngl::math::Vec4(1.0f));
				tr.SetColumn3(ngl::math::Vec4(0.0f, 12.0f, 0.0f, 1.0f));

				mc->transform_ = ngl::math::Mat34(tr);
			}
			

			for(int i = 0; i < 100; ++i)
			{
				auto mc = std::make_shared<ngl::gfx::StaticMeshComponent>();
				mesh_comp_array_.push_back(mc);
				ngl::gfx::ResMeshData::LoadDesc loaddesc = {};
				mc->Initialize(&device_, ResourceMan.LoadResource<ngl::gfx::ResMeshData>(&device_, mesh_file_spider, &loaddesc));

				constexpr int k_rand_f_div = 10000;
				const float randx = (std::rand() % k_rand_f_div) / (float)k_rand_f_div;
				const float randy = (std::rand() % k_rand_f_div) / (float)k_rand_f_div;
				const float randz = (std::rand() % k_rand_f_div) / (float)k_rand_f_div;
				const float randroty = (std::rand() % k_rand_f_div) / (float)k_rand_f_div;

				constexpr float placement_range = 30.0f;

				ngl::math::Mat44 tr = ngl::math::Mat44::Identity();
				tr.SetDiagonal(ngl::math::Vec4(spider_base_scale));
				tr = ngl::math::Mat44::RotAxisY(randroty * ngl::math::k_pi_f * 2.0f) * tr;
				tr.SetColumn3(ngl::math::Vec4(placement_range* (randx * 2.0f - 1.0f), 10.0f * randy, placement_range* (randz * 2.0f - 1.0f), 1.0f));

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
		if (!rt_scene_.Initialize(&device_))
		{
			std::cout << "[ERROR] Create gfx::RtSceneManager" << std::endl;
			assert(false);
		}

		// RtPass.
		if(!rt_pass_test.Initialize(&device_, 1))
		{
			assert(false);
		}
	}
	
	// テストコード
	ngl_test::TestFunc00(&device_);
	
	// Texture Rexource読み込みのテスト.
	ngl::gfx::ResTexture::LoadDesc tex_load_desc = {};
	
	//const char test_load_texture_file_name[] = "./data/model/sponza_gltf/glTF/6772804448157695701.jpg";
	const char test_load_texture_file_name[] = "./data/texture/sample_dds/test-dxt1.dds";
	res_texture_ = ngl::res::ResourceManager::Instance().LoadResource<ngl::gfx::ResTexture>(&device_, test_load_texture_file_name, &tex_load_desc);
	
	ngl::time::Timer::Instance().StartTimer("app_frame_sec");
	return true;
}

void AppGame::SyncRender()
{
	// FrameのGame-Render同期処理.

	// TODO. Wait Game Thread.
	
	// Graphics Deviceのフレーム準備
	device_.ReadyToNewFrame();
	
	// RTGのフレーム開始処理.
	rtg_manager_.BeginFrame();
}

void AppGame::BeginRender()
{
	// システム用のフレーム先頭CommandListを準備.
	gfx_frame_begin_command_list_->Begin();
	
	// ResourceManagerのRenderThread処理.
	//	TextureLinearBufferや MeshBufferのUploadなど.
	ngl::res::ResourceManager::Instance().UpdateResourceOnRender(&device_, gfx_frame_begin_command_list_.Get());
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
		float camera_translate_speed = 10.0f;


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
			// MEMO. マウスR押下中に実際のマウス位置を動かさないようにしたい(ウィンドウから出てしまうので)
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
	if(true)
	{
		for (int i = 0; i < test_move_mesh_comp_array_.size(); ++i)
		{
			auto* e = test_move_mesh_comp_array_[i];
			float move_range = (i % 10) / 10.0f;
			const float sin_curve = sinf((float)app_sec_ * 2.0f * ngl::math::k_pi_f * 0.1f * (move_range + 1.0f));

			auto trans = e->transform_.GetColumn3();
			trans.z += sin_curve * 0.075f;
			e->transform_.SetColumn3(trans);
		}
	}

	const float screen_aspect_ratio = (float)swapchain_->GetWidth() / swapchain_->GetHeight();

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

	// RaytraceSceneにカメラ設定.
	rt_scene_.SetCameraInfo(camera_pos_, camera_pose_.GetColumn2(), camera_pose_.GetColumn1(), camera_fov_y, screen_aspect_ratio);
	

	// -------------------------------------------------------
	// Sync Render Thread.
	SyncRender();
	{
		// Start Frame Rendering.
		BeginRender();

		
		// フレームのSwapchainインデックス.
		const auto swapchain_index = swapchain_->GetCurrentBufferIndex();
		
		// Raytracingテスト.
		{
			// RtScene更新. AS更新とそのCommand生成.
			rt_scene_.UpdateOnRender(&device_, gfx_frame_begin_command_list_.Get(), frame_scene);
				
			// RtPass 更新.
			rt_pass_test.PreRenderUpdate(&rt_scene_);
				
			// RtPass Render.
			rt_pass_test.Render(gfx_frame_begin_command_list_.Get());
		}
		
		
		// ここからRTGを使ったシーンのレンダリング.

		// Pathが構築したCommandListの出力先.
		struct RtgGenerateCommandListSet
		{
			std::vector<ngl::rtg::RtgSubmitCommandSequenceElem> graphics = {};
			std::vector<ngl::rtg::RtgSubmitCommandSequenceElem> compute = {};
		};
		std::vector<RtgGenerateCommandListSet> rtg_gen_command = {};

		// SubViewの描画テスト.
		if(true)
		{
		#if 0
			constexpr ngl::rhi::EResourceState swapchain_final_state = ngl::rhi::EResourceState::Present;// Execute後のステート指定.

			// Pathの設定.
			ngl::test::RenderFrameDesc render_frame_desc = {};
			{
				render_frame_desc.p_device = &device_;
				
				render_frame_desc.screen_w = screen_w;
				render_frame_desc.screen_h = screen_h;

				render_frame_desc.camera_pos = camera_pos_;
				render_frame_desc.camera_pose = camera_pose_;
				render_frame_desc.camera_fov_y = camera_fov_y;

				render_frame_desc.p_scene = &frame_scene;
				
				render_frame_desc.ref_test_tex_srv0 = rt_pass_test.ray_result_srv_;
				render_frame_desc.ref_test_tex_srv1 = res_texture_->ref_view_;

				render_frame_desc.h_prev_lit = {};// SubViewではヒストリ無効.
			}
			
			swapchain_resource_state_[swapchain_index] = swapchain_final_state;// State変更.

			rtg_gen_command.push_back({});
			RtgGenerateCommandListSet& rtg_result = rtg_gen_command.back();
			// Pathの実行 (RenderTaskGraphの構築と実行).
			ngl::test::RenderFrameOut render_frame_out = {};
			TestFrameRenderingPath(render_frame_desc, render_frame_out, rtg_manager_, rtg_result.graphics, rtg_result.compute);
		#endif
		}
		
		static ngl::rtg::ResourceHandle h_prev_light = {};// 前回フレームハンドルのテスト.
		// MainViewの描画.
		{
			constexpr ngl::rhi::EResourceState swapchain_final_state = ngl::rhi::EResourceState::Present;// Execute後のステート指定.

			// Pathの設定.
			ngl::test::RenderFrameDesc render_frame_desc = {};
			{
				render_frame_desc.p_device = &device_;
				
				render_frame_desc.screen_w = screen_w;
				render_frame_desc.screen_h = screen_h;

				// MainViewはSwapchain書き込みPassを動かすため情報設定.
				render_frame_desc.ref_swapchain = swapchain_;
				render_frame_desc.ref_swapchain_rtv = swapchain_rtvs_[swapchain_index];
				render_frame_desc.swapchain_state_prev = swapchain_resource_state_[swapchain_index];
				render_frame_desc.swapchain_state_next = swapchain_final_state;
				
				render_frame_desc.camera_pos = camera_pos_;
				render_frame_desc.camera_pose = camera_pose_;
				render_frame_desc.camera_fov_y = camera_fov_y;

				render_frame_desc.p_scene = &frame_scene;
				
				render_frame_desc.ref_test_tex_srv0 = rt_pass_test.ray_result_srv_;
				render_frame_desc.ref_test_tex_srv1 = res_texture_->ref_view_;

				render_frame_desc.h_prev_lit = h_prev_light;// MainViewはヒストリ有効.
			}
			
			swapchain_resource_state_[swapchain_index] = swapchain_final_state;// State変更.
			
			rtg_gen_command.push_back({});
			RtgGenerateCommandListSet& rtg_result = rtg_gen_command.back();
			// Pathの実行 (RenderTaskGraphの構築と実行).
			ngl::test::RenderFrameOut render_frame_out = {};
			TestFrameRenderingPath(render_frame_desc, render_frame_out, rtg_manager_, rtg_result.graphics, rtg_result.compute);
			
			h_prev_light = render_frame_out.h_propagate_lit;// Pathの一部を次フレームに伝搬する.
		}

		
		// AsyncComputeテスト.
		ngl::rhi::ComputeCommandListDep* rtg_compute_command_list = {};
		rtg_manager_.GetNewFrameCommandList(rtg_compute_command_list);
		{
			// テストのためPoolから取得したCommandListに積み込み.
			rtg_compute_command_list->Begin();
			// GraphicsQueueがComputeQueueをWaitする状況のテスト用.
			{
				auto ref_cpso = ngl::rhi::RhiRef<ngl::rhi::ComputePipelineStateDep>(new ngl::rhi::ComputePipelineStateDep());
				{
					ngl::rhi::ComputePipelineStateDep::Desc cpso_desc = {};
					{
						ngl::gfx::ResShader::LoadDesc cs_load_desc = {};
						cs_load_desc.stage = ngl::rhi::EShaderStage::Compute;
						cs_load_desc.shader_model_version = "6_3";
						cs_load_desc.entry_point_name = "main_cs";
						auto cs_load_handle = ngl::res::ResourceManager::Instance().LoadResource<ngl::gfx::ResShader>(&device_, "./src/ngl/data/shader/test/async_task_test_cs.hlsl", &cs_load_desc);
					
						cpso_desc.cs = &cs_load_handle->data_;
					}
					// CsPso初期化.
					ref_cpso->Initialize(&device_, cpso_desc);
				}
					
				ngl::rhi::DescriptorSetDep desc_set = {};
				ref_cpso->SetView(&desc_set, "rwtex_out", tex_rw_uav_.Get());
				rtg_compute_command_list->SetPipelineState(ref_cpso.Get());
				rtg_compute_command_list->SetDescriptorSet(ref_cpso.Get(), &desc_set);
				ref_cpso->DispatchHelper(rtg_compute_command_list, tex_rw_->GetWidth(), tex_rw_->GetHeight(), 1);
			}
			rtg_compute_command_list->End();
		}

		
		// CommandList Submit
		{
#if 1
			// フレーム先頭からAcyncComputeのテストSubmit.
			ngl::u64 compute_end_fence_value = {};
			{
				ngl::rhi::CommandListBaseDep* p_command_lists[] =
				{
					rtg_compute_command_list
				};
				compute_queue_.ExecuteCommandLists(static_cast<unsigned int>(std::size(p_command_lists)), p_command_lists);
			}
#endif
			
			// システム用のGraphics CommandListをSubmit.
			{
				if(gfx_frame_begin_command_list_->IsOpen())
					gfx_frame_begin_command_list_->End();
				
				ngl::rhi::CommandListBaseDep* submit_list[]=
				{
					gfx_frame_begin_command_list_.Get()
				};
				graphics_queue_.ExecuteCommandLists(static_cast<unsigned int>(std::size(submit_list)), submit_list);
			}
			
			// RTGのCommaandをSubmit.
			for(auto& e : rtg_gen_command)
			{
				ngl::rtg::RenderTaskGraphBuilder::SubmitCommand(graphics_queue_, compute_queue_, e.graphics, e.compute);
			}
		}
		
		// Present
		swapchain_->GetDxgiSwapChain()->Present(1, 0);

		// CPUへの完了シグナル. Wait用のFenceValueを取得.
		const auto wait_gpu_fence_value = graphics_queue_.SignalAndIncrement(&wait_fence_);
		// 待機
		{
			wait_signal_.Wait(&wait_fence_, wait_gpu_fence_value);
		}
		// フレーム描画完了.
	}

	return true;
}