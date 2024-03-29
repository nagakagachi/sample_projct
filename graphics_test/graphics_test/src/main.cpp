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
	ngl::rhi::ComputeCommandQueueDep			compute_queue_;

	// RenderTaskGraphのCompileやそれらが利用するリソースプール管理.
	ngl::rtg::RenderTaskGraphManager rtg_manager_{};
	

	// CommandQueue実行完了待機用Fence
	ngl::rhi::FenceDep							wait_fence_;
	// CommandQueue実行完了待機用オブジェクト
	ngl::rhi::WaitOnFenceSignalDep				wait_signal_;
	
	ngl::rhi::FenceDep							test_compute_to_gfx_fence_;
	
	ngl::rhi::RhiRef<ngl::rhi::GraphicsCommandListDep>	gfx_command_list_;
	ngl::rhi::RhiRef<ngl::rhi::ComputeCommandListDep>	compute_command_list_;

	// SwapChain
	ngl::rhi::RhiRef<ngl::rhi::SwapChainDep>	swapchain_;
	std::vector<ngl::rhi::RefRtvDep>			swapchain_rtvs_;
	std::vector<ngl::rhi::ResourceState>		swapchain_resource_state_;


	ngl::rhi::RefTextureDep						tex_work_;
	ngl::rhi::RefRtvDep							tex_work_rtv_;
	ngl::rhi::RefSrvDep							tex_work_srv_;

	ngl::rhi::RefTextureDep						tex_lineardepth_;
	ngl::rhi::RefSrvDep							tex_lineardepth_srv_;
	ngl::rhi::RefUavDep							tex_lineardepth_uav_;


	ngl::rhi::RefTextureDep						tex_rw_;
	ngl::rhi::RefSrvDep							tex_rw_srv_;
	ngl::rhi::RefUavDep							tex_rw_uav_;


	ngl::rhi::RefSampDep						samp_linear_clamp_;

	std::array<ngl::rhi::RefBufferDep, 2>		cb_sceneview_;
	std::array<ngl::rhi::RefCbvDep, 2>			cbv_sceneview_;
	int											flip_index_sceneview_ = 0;

	// RtScene.
	ngl::gfx::RtSceneManager					rt_st_;
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

	// リソースマネージャから全て破棄.
	ngl::res::ResourceManager::Instance().ReleaseCacheAll();


	gfx_command_list_.Reset();
	compute_command_list_.Reset();
	swapchain_.Reset();


	graphics_queue_.Finalize();
	compute_queue_.Finalize();
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

	// RTGマネージャ初期化.
	{
		rtg_manager_.Init(&device_);
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
		desc.format = ngl::rhi::ResourceFormat::Format_R16G16B16A16_FLOAT;
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

	gfx_command_list_ = new ngl::rhi::GraphicsCommandListDep();
	if (!gfx_command_list_->Initialize(&device_))
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


	// Render Frame.
	{
		// -------------------------------------------------------
		// Deviceのフレーム準備
		device_.ReadyToNewFrame();
		
		// RTGのフレーム開始処理.
		rtg_manager_.BeginFrame();
		// -------------------------------------------------------


		// フレームのSwapchainインデックス.
		const auto swapchain_index = swapchain_->GetCurrentBufferIndex();

		
		// RtgのCommandList配列.
		std::vector<ngl::rhi::CommandListBaseDep*> rtg_command_list_sequence = {};
		std::vector<ngl::rtg::RtgCommandListRangeInfo> rtg_command_list_range_info = {};
		
		ngl::rhi::ComputeCommandListDep* rtg_compute_command_list = {};
		rtg_manager_.GetNewFrameCommandList(rtg_compute_command_list);
		
		{
			gfx_command_list_->Begin();
			{
				// CommandList に最初にResourceManagerの処理を積み込み.
				ngl::res::ResourceManager::Instance().UpdateResourceOnRender(&device_, gfx_command_list_.Get());

				// Raytracing.
				{
					// RtScene更新.
					rt_st_.UpdateOnRender(&device_, gfx_command_list_.Get(), frame_scene);
				
					// RtPass 更新.
					rt_pass_test.PreRenderUpdate(&rt_st_, gfx_command_list_.Get());
				
					// RtPass Render.
					rt_pass_test.Render(gfx_command_list_.Get());
				}
			}
			gfx_command_list_->End();

			
			// RenderTaskGraphによるレンダリングパス.
			{
				// RTG Build.
				ngl::rtg::RenderTaskGraphBuilder rtg_builder{};// 実行単位のGraph構築.
				
				// Append External Resource Info.
				ngl::rtg::ResourceHandle h_swapchain = {};
				{
					constexpr ngl::rhi::ResourceState swapchain_final_state = ngl::rhi::ResourceState::Present;// Execute後のステート指定.
					// 外部リソース登録.
					h_swapchain = rtg_builder.AppendExternalResource(swapchain_, swapchain_rtvs_[swapchain_->GetCurrentBufferIndex()], swapchain_resource_state_[swapchain_index], swapchain_final_state);
					
					// 状態追跡更新.
					swapchain_resource_state_[swapchain_index] = swapchain_final_state;
				}

				// 前回フレームハンドルのテスト.
				static ngl::rtg::ResourceHandle h_prev_light = {};

				// Build Rendering Pass.
				{
#define ASYNC_COMPUTE_TEST 1
					// AsyncComputeの依存関係の追い越しパターンテスト用.
					ngl::rtg::ResourceHandle async_compute_tex2 = {};
#if ASYNC_COMPUTE_TEST
					// AsyncCompute Pass.
					auto* task_test_compute1 = rtg_builder.AppendNodeToSequence<ngl::render::task::TaskCopmuteTest>();
					task_test_compute1->Setup(rtg_builder, &device_, {});
					async_compute_tex2 = task_test_compute1->h_work_tex_;
#endif
					
					// PreZ Pass.
					auto* task_depth = rtg_builder.AppendNodeToSequence<ngl::render::task::TaskDepthPass>();
					task_depth->Setup(rtg_builder, &device_, cbv_sceneview_[flip_index_sceneview_], frame_scene.mesh_instance_array_);

					ngl::rtg::ResourceHandle async_compute_tex = {};
#if ASYNC_COMPUTE_TEST
					// AsyncCompute Pass 其の二.
					auto* task_test_compute2 = rtg_builder.AppendNodeToSequence<ngl::render::task::TaskCopmuteTest>();
					task_test_compute2->Setup(rtg_builder, &device_, task_depth->h_depth_);
					async_compute_tex = task_test_compute2->h_work_tex_;
#endif
					
					// GBuffer Pass.
					auto* task_gbuffer = rtg_builder.AppendNodeToSequence<ngl::render::task::TaskGBufferPass>();
					task_gbuffer->Setup(rtg_builder, &device_, task_depth->h_depth_, async_compute_tex);
					
					// Linear Depth Pass.
					auto* task_linear_depth = rtg_builder.AppendNodeToSequence<ngl::render::task::TaskLinearDepthPass>();
					task_linear_depth->Setup(rtg_builder, &device_, task_depth->h_depth_,async_compute_tex2, cbv_sceneview_[flip_index_sceneview_]);

					// Deferred Lighting Pass.
					auto* task_light = rtg_builder.AppendNodeToSequence<ngl::render::task::TaskLightPass>();
					task_light->Setup(rtg_builder, &device_, task_gbuffer->h_depth_, task_gbuffer->h_gb0_, task_gbuffer->h_gb1_, task_linear_depth->h_linear_depth_, h_prev_light, samp_linear_clamp_);

					// Final Composite to Swapchain.
					auto* task_final = rtg_builder.AppendNodeToSequence<ngl::render::task::TaskFinalPass>();
					task_final->Setup(rtg_builder, &device_, h_swapchain, task_light->h_depth_, task_linear_depth->h_linear_depth_, task_light->h_light_, samp_linear_clamp_, rt_pass_test.ray_result_srv_);

					// 次回フレームへの伝搬. 次回フレームでは h_prev_light によって前回フレームリソースを利用できる.
					{
						h_prev_light = rtg_builder.PropagateResouceToNextFrame(task_light->h_light_);
					}
				}

				// Compile. ManagerでCompileを実行する.
				rtg_manager_.Compile(rtg_builder);
				
				// Execute. GraphのCommandListを生成する.
				//	Compileによってリソースプールのステートが更新され, その後にCompileされたGraphはそれを前提とするため, Graphは必ずExecuteする必要がある.
				//	GraphのExecuteで生成されるCommandListはCompileされた順序でSubmitされることで正しい実行順となる.
				//	よって複数のGraphを別スレッドでExecuteして別のCommandListを生成->正しい順序でSubmitという運用は許可される.
				//	Compile,ExecuteしたBuilderは再利用不可となり使い捨てする.
				rtg_builder.ExecuteMultiCommandlist(rtg_command_list_sequence, rtg_command_list_range_info);
				
			}
		}

		// AsyncComputeテスト.
		{
			// テストのためPoolから取得したCommandListに積み込み.
			rtg_compute_command_list->Begin();

			// 仮で適当なAsyncComputeタスクを生成.
			// GraphicsQueueがComputeQueueをWaitする状況のテスト用.
			{
				auto ref_cpso = ngl::rhi::RhiRef<ngl::rhi::ComputePipelineStateDep>(new ngl::rhi::ComputePipelineStateDep());
				{
					ngl::rhi::ComputePipelineStateDep::Desc cpso_desc = {};
					{
						ngl::gfx::ResShader::LoadDesc cs_load_desc = {};
						cs_load_desc.stage = ngl::rhi::ShaderStage::Compute;
						cs_load_desc.shader_model_version = "6_3";
						cs_load_desc.entry_point_name = "main_cs";
						auto cs_load_handle = ngl::res::ResourceManager::Instance().LoadResource<ngl::gfx::ResShader>(&device_, "./src/ngl/data/shader/test/async_task_test_cs.hlsl", &cs_load_desc);
					
						cpso_desc.cs = &cs_load_handle->data_;
					}
					// CsPso初期化.
					ref_cpso->Initialize(&device_, cpso_desc);
				}
					
				ngl::rhi::DescriptorSetDep desc_set = {};
				ref_cpso->SetDescriptorHandle(&desc_set, "rwtex_out", tex_rw_uav_->GetView().cpu_handle);
				rtg_compute_command_list->SetPipelineState(ref_cpso.Get());
				rtg_compute_command_list->SetDescriptorSet(ref_cpso.Get(), &desc_set);
				ref_cpso->DispatchHelper(rtg_compute_command_list, tex_rw_->GetWidth(), tex_rw_->GetHeight(), 1);
			}

			rtg_compute_command_list->End();
		}

		// CommandList Submit
		{
#if 1
			// Compute.
			ngl::u64 compute_end_fence_value = {};
			{
				ngl::rhi::CommandListBaseDep* p_command_lists[] =
				{
					rtg_compute_command_list
				};
				compute_queue_.ExecuteCommandLists(static_cast<unsigned int>(std::size(p_command_lists)), p_command_lists);
				
				// ComputeのCommandListの末尾からSignal発行.
				//compute_end_fence_value = compute_queue_.SignalAndIncrement(&test_compute_to_gfx_fence_);
			}
			
			// 適当にComputeQueueのFenceを待つWait.
			//	実際にはGraphicsタスクの先頭でのWaitなので並列動作しないが, RTG内部でのAsyncComputeの検証のためここでWait.
			//graphics_queue_.Wait(&test_compute_to_gfx_fence_, compute_end_fence_value);
#endif
			
			// システム用の先頭CommandListをSubmit.
			{
				ngl::rhi::CommandListBaseDep* submit_list[]=
				{
					gfx_command_list_.Get()
				};
				graphics_queue_.ExecuteCommandLists(static_cast<unsigned int>(std::size(submit_list)), submit_list);
			}
#if 1
			// RTGのGraphicsとComputeのCommandListを実行するテスト.
			{

				// RTGがスケジューリングした依存関係IDに割り当てられたFenceを取得(確保)する.
				std::vector<ngl::rhi::RhiRef<ngl::rhi::FenceDep>> dependency_fence_list = {};
				std::vector<ngl::u64>	dependency_fence_value = {};
				std::vector<bool>		dependency_fence_signaled = {};
				auto GetDepencendyFenceObject = [&dependency_fence_list, &dependency_fence_value, &dependency_fence_signaled](ngl::rhi::DeviceDep* p_device, int fence_id)
				-> int
				{
					// Fenceを必要分確保. fence_idは連番で最小限となっているはず.
					for(;dependency_fence_list.size() <= fence_id;)
					{
						dependency_fence_list.push_back({});
						dependency_fence_value.push_back({});// 初期値.
						dependency_fence_signaled.push_back({});
					}
					assert(dependency_fence_list.size() > fence_id);
					assert(dependency_fence_value.size() > fence_id);
					if(!dependency_fence_list[fence_id].IsValid())
					{
						dependency_fence_list[fence_id].Reset( new ngl::rhi::FenceDep() );
						dependency_fence_list[fence_id]->Initialize(p_device);
						
						dependency_fence_value[fence_id] = dependency_fence_list[fence_id]->GetFenceValue();// 初期値.
						dependency_fence_signaled[fence_id] = false;
					}

					return fence_id;
				};
				
				
				for(int range_i = 0; range_i < rtg_command_list_range_info.size(); ++range_i)
				{
					const auto queue_type = rtg_command_list_range_info[range_i].type;
					const auto begin_index = rtg_command_list_range_info[range_i].begin;
					const auto list_count =rtg_command_list_range_info[range_i].count;
					const auto signal_id = rtg_command_list_range_info[range_i].fence_signal;
					const auto wait_id = rtg_command_list_range_info[range_i].fence_wait;


					int local_command_list_index = begin_index;
					int local_command_list_count = list_count;
					// Computeの場合は先頭にState Transition用のGraphicsCommandListが存在する場合があるため, その専用処理.
					if(ngl::rtg::ETASK_TYPE::COMPUTE == queue_type)
					{
						// ComputeTaskのCommandListの1つ目のタイプがGraphicsの場合はState Transitionのためのものであるため, GraphicsQueueにSubmitしたうえでFenceで同期する.
						// TODO. 面倒なので一旦D3D12型を直接使用.
						if(D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT == rtg_command_list_sequence[local_command_list_index]->GetDesc().type)
						{
							// サブミット.
							graphics_queue_.ExecuteCommandLists(static_cast<unsigned int>(1), &rtg_command_list_sequence[local_command_list_index]);
							// インデックスとカウント更新.
							local_command_list_index += 1;
							local_command_list_count -= 1;

							// GraphicsにSubmitしたState TransitionをComputeが待機するようにFence.
							{
								// 現状ではFenceはその場で生成して使い捨て.
								ngl::rhi::RhiRef<ngl::rhi::FenceDep> fence = new ngl::rhi::FenceDep();
								fence->Initialize(&device_);

								const auto signal_value = graphics_queue_.SignalAndIncrement(fence.Get());
								compute_queue_.Wait(fence.Get(), signal_value);
							}
						}
					}

					// Wait.
					//	Submit前にWaitが必要であれば発行.
					if(0 <= wait_id)
					{
						// Wait発行.
						const int fence_index = GetDepencendyFenceObject(&device_, wait_id);
						// まだSignalに使われていない場合はFence値が更新されていないため, +1した値をWaitする.
						auto wait_value = (dependency_fence_signaled[fence_index])? dependency_fence_value[fence_index] : dependency_fence_value[fence_index]+1;
						
						if(ngl::rtg::ETASK_TYPE::GRAPHICS == queue_type)
							graphics_queue_.Wait(dependency_fence_list[fence_index].Get(), wait_value);
						else
							compute_queue_.Wait(dependency_fence_list[fence_index].Get(), wait_value);
					}
					
					// Submit.
					//	メインのCommandList群をSubmit.
					//	TODO. Typeが同じでFenceにかかわらない連続したNodeのCommandListはマージしてExecuteしたいところ.
					if(ngl::rtg::ETASK_TYPE::GRAPHICS == queue_type)
					{
						graphics_queue_.ExecuteCommandLists(static_cast<unsigned int>(local_command_list_count), &rtg_command_list_sequence[local_command_list_index]);
					}
					else
					{
						compute_queue_.ExecuteCommandLists(static_cast<unsigned int>(local_command_list_count), &rtg_command_list_sequence[local_command_list_index]);
					}

					// Signal.
					//	Submit後にSignalが必要であれば発行.
					if(0 <= signal_id)
					{
						// Signal発行.
						const int fence_index = GetDepencendyFenceObject(&device_, signal_id);
						assert(false == dependency_fence_signaled[fence_index]);// Fenceオブジェクトは現状では使い回ししないためfalceのはず.
						dependency_fence_signaled[fence_index] = true;
						
						++dependency_fence_value[fence_index];// Signalの値を更新.
						auto signal_value = dependency_fence_value[fence_index];
						
						if(ngl::rtg::ETASK_TYPE::GRAPHICS == queue_type)
							graphics_queue_.Signal(dependency_fence_list[fence_index].Get(), signal_value);
						else
							compute_queue_.Signal(dependency_fence_list[fence_index].Get(), signal_value);
					}
				}
			}
#else
			// Graphics.
			// AsyncCompute無し想定ですべてのCommandListを直列実行するテスト.
			{
				std::vector<ngl::rhi::CommandListBaseDep*> p_command_list_submit_sequence = {};
				{
					// RtgのCommandList配列をPush.
					for(auto& e : rtg_command_list_sequence)
					{
						p_command_list_submit_sequence.push_back(e);
					}
				}
				graphics_queue_.ExecuteCommandLists(static_cast<unsigned int>(p_command_list_submit_sequence.size()), p_command_list_submit_sequence.data());
			}
#endif
		}
		// Present
		swapchain_->GetDxgiSwapChain()->Present(1, 0);

		// CPUへの完了シグナル. Wait用のFenceValueを取得.
		const auto wait_gpu_fence_value = graphics_queue_.SignalAndIncrement(&wait_fence_);
		// 待機
		{
			wait_signal_.Wait(&wait_fence_, wait_gpu_fence_value);
		}

	}

	return true;
}