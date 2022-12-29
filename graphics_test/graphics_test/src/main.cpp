
#include <iostream>
#include <vector>
#include <array>
#include <unordered_map>
#include <memory>

#include "ngl/boot/boot_application.h"
#include "ngl/platform/window.h"
#include "ngl/util/time/timer.h"
#include "ngl/memory/tlsf_memory_pool.h"
#include "ngl/memory/tlsf_allocator.h"
#include "ngl/file/file.h"
//#include "ngl/util/shared_ptr.h"

#include "ngl/math/math.h"

// resource
#include "ngl/resource/resource_manager.h"

// rhi
#include "ngl/rhi/d3d12/rhi.d3d12.h"
#include "ngl/rhi/d3d12/rhi_command_list.d3d12.h"
#include "ngl/rhi/d3d12/rhi_descriptor.d3d12.h"
#include "ngl/rhi/d3d12/rhi_resource.d3d12.h"
#include "ngl/rhi/d3d12/rhi_resource_view.d3d12.h"


// gfx
#include "ngl/gfx/mesh_resource.h"
#include "ngl/gfx/rt_structure_manager.h"
#include "ngl/gfx/mesh_component.h"


struct CbSampleVs
{
	ngl::math::Vec4 cb_param_vs0;
};
struct CbSamplePs
{
	ngl::math::Vec4 cb_param0;
};


// アプリ本体.
class AppGame : public ngl::boot::ApplicationBase
{
public:
	AppGame();
	~AppGame();

	bool Initialize() override;
	bool Execute() override;

	void TestCode();

private:
	double						app_sec_ = 0.0f;
	double						frame_sec_ = 0.0f;

	ngl::platform::CoreWindow	window_;


	ngl::math::Vec4 clear_color_ = ngl::math::Vec4(0.0f);


	ngl::math::Vec3		camera_pos_ = {0.0f, 2.0f, -1.0f};
	ngl::math::Mat33	camera_pose_ = ngl::math::Mat33::Identity();


	ngl::rhi::DeviceDep							device_;
	ngl::rhi::GraphicsCommandQueueDep			graphics_queue_;
	

	// CommandQueue実行完了待機用Fence
	ngl::rhi::FenceDep							wait_fence_;
	// Fenceの初期値がゼロであるため待機用の値は1から開始.
	ngl::types::u64								wait_fence_value_ = 1;
	// CommandQueue実行完了待機用オブジェクト
	ngl::rhi::WaitOnFenceSignalDep				wait_signal_;

	ngl::rhi::GraphicsCommandListDep			gfx_command_list_;

	// SwapChain
	ngl::rhi::SwapChainDep						swapchain_;
	std::vector<ngl::rhi::RenderTargetViewDep>	swapchain_rtvs_;
	std::vector <ngl::rhi::ResourceState>		swapchain_resource_state_;

	ngl::rhi::TextureDep						tex_depth_;
	ngl::rhi::DepthStencilViewDep				tex_depth_dsv_;
	ngl::rhi::ResourceState						tex_depth_state_;


	bool										tex_rt_rendered_ = false;
	ngl::rhi::TextureDep						tex_rt_;
	ngl::rhi::RenderTargetViewDep				tex_rt_rtv_;
	ngl::rhi::ShaderResourceViewDep				tex_rt_srv_;
	ngl::rhi::ResourceState						tex_rt_state_;

	ngl::rhi::TextureDep						tex_ua_;
	ngl::rhi::UnorderedAccessViewDep				tex_ua_uav_;
	ngl::rhi::ResourceState						tex_ua_state_;


	ngl::rhi::ShaderDep							sample_vs_;
	ngl::rhi::ShaderDep							sample_ps_;
	ngl::rhi::GraphicsPipelineStateDep			sample_pso_;

	ngl::rhi::BufferDep							cb_sample_vs_;
	ngl::rhi::ConstantBufferViewDep				cbv_sample_vs_;
	ngl::rhi::BufferDep							cb_sample_ps_;
	ngl::rhi::ConstantBufferViewDep				cbv_sample_ps_;

	ngl::rhi::BufferDep							vb_sample_;
	ngl::rhi::VertexBufferViewDep				vbv_sample_;

	ngl::rhi::BufferDep							ib_sample_;
	ngl::rhi::IndexBufferViewDep				ibv_sample_;


	ngl::rhi::ShaderDep							sample_fullscr_proc_vs_;
	ngl::rhi::ShaderDep							sample_fullscr_proc_ps_;
	ngl::rhi::GraphicsPipelineStateDep			sample_fullscr_proc_pso_;

	ngl::rhi::SamplerDep						samp_;


	ngl::gfx::RaytraceStructureManager			rt_st_;

	ngl::rhi::ShaderDep							rt_shader_lib0_;
	ngl::gfx::RaytraceStateObject				rt_state_object_;


	
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
	ngl::math::funcAA();

	const ngl::math::Mat44 k_inv_test_00 = ngl::math::Mat44::Identity();

	const ngl::math::Mat44 k_rot_z = ngl::math::Mat44(std::cos(0.5f), -std::sin(0.5f), 0, 0, /**/ std::sin(0.5f), std::cos(0.5f), 0, 0, /**/ 0, 0, 1, 0, /**/ 0, 0, 0, 1);
	const ngl::math::Mat44 k_rot_z_inv = ngl::math::Mat44::Inverse(k_rot_z);


	const ngl::math::Mat44 k_rot_x = ngl::math::Mat44(1, 0, 0, 0, /**/ 0, std::cos(0.5f), -std::sin(0.5f), 0, /**/ 0, std::sin(0.5f), std::cos(0.5f), 0, /**/ 0, 0, 0, 1);
	const ngl::math::Mat44 k_rot_x_inv = ngl::math::Mat44::Inverse(k_rot_x);

	const ngl::math::Mat44 k_rot_y = ngl::math::Mat44(std::cos(0.5f), 0, -std::sin(0.5f), 0, /**/ 0, 1, 0, 0, /**/ std::sin(0.5f), 0, std::cos(0.5f), 0, /**/ 0, 0, 0, 1);
	const ngl::math::Mat44 k_rot_y_inv = ngl::math::Mat44::Inverse(k_rot_y);


	const auto m_0 = k_rot_z * k_rot_x * ngl::math::Mat44::Inverse(k_rot_z * k_rot_x);
	const auto m_1 = k_rot_x * k_rot_x_inv;
	const auto m_2 = k_rot_y * k_rot_y_inv;



	const ngl::math::Mat33 k_rot33_z = ngl::math::Mat33(std::cos(0.5f), -std::sin(0.5f), 0, /**/ std::sin(0.5f), std::cos(0.5f), 0, /**/ 0, 0, 1);
	const ngl::math::Mat33 k_rot33_z_inv = ngl::math::Mat33::Inverse(k_rot33_z);
	const auto k_rot33_z_mul = k_rot33_z * k_rot33_z_inv;


	const ngl::math::Mat22 k_rot22_z = ngl::math::Mat22(std::cos(0.5f), -std::sin(0.5f),/**/ std::sin(0.5f), std::cos(0.5f));
	const ngl::math::Mat22 k_rot22_z_inv = ngl::math::Mat22::Inverse(k_rot22_z);
	const auto k_rot22_z_mul = k_rot22_z * k_rot22_z_inv;

	ngl::math::Vec3 nv0 = ngl::math::Vec3::Normalize(ngl::math::Vec3(1.0));



	const float fov_y = ngl::math::Deg2Rad(90.0f);
	const float aspect = 16.0f / 9.0f;

	const float vertical_view_max = std::tanf(fov_y*0.5f);

	// View Matrix.
	const auto view_dir0 = ngl::math::Vec3::Normalize(ngl::math::Vec3(0.5, 0.0, 0.5));
	const auto view_location0 = ngl::math::Vec3(1, 2, 3);
	const ngl::math::Mat34 viewmat0 = ngl::math::CalcViewMatrix(view_location0, view_dir0, ngl::math::Vec3(0, 1, 0));
	const auto viewmat0_inv = ngl::math::Mat34::Inverse(viewmat0);

	const ngl::math::Vec3 vp0 = viewmat0 * ngl::math::Vec3(0, 0, 0);
	const ngl::math::Vec3 vp1 = viewmat0 * ngl::math::Vec3(1, 0, 0);
	const ngl::math::Vec3 vp2 = viewmat0 * ngl::math::Vec3(0, 1, 0);
	const ngl::math::Vec3 vp3 = viewmat0 * ngl::math::Vec3(0, 0, 1);
	const auto pos_in_view_line = ((viewmat0.r0.XYZ() * vertical_view_max * aspect) + (viewmat0.r1.XYZ() * vertical_view_max) + (viewmat0.r2.XYZ() * 1.0f)) * 100.0f;
	const ngl::math::Vec3 vp4 = viewmat0 * (pos_in_view_line + view_location0);

	const ngl::math::Vec3 vp4_inv = viewmat0_inv * (vp4);


	//const ngl::math::Mat44 proj0 = ngl::math::CalcStandardPerspectiveMatrixRH(fov_y, aspect, 0.01f, 10000.0f);// 標準透視投影.
	const ngl::math::Mat44 proj0 = ngl::math::CalcReverseInfiniteFarPerspectiveMatrix(fov_y, aspect, 0.1f);// ReverseAndInfiniteFar透視投影.

	const auto pp4 = proj0 * ngl::math::Vec4(vp4, 1);
	const auto pp4_ndc = pp4 / pp4.w;


}
AppGame::~AppGame()
{
	// リソース参照クリア.
	mesh_comp_array_.clear();

	// リソースマネージャから全て破棄.
	ngl::res::ResourceManager::Instance().ReleaseCacheAll();


	gfx_command_list_.Finalize();
	swapchain_.Finalize();
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
		swap_chain_desc.format = ngl::rhi::ResourceFormat::Format_R10G10B10A2_UNORM;// DXGI_FORMAT_R10G10B10A2_UNORM;
		if (!swapchain_.Initialize(&device_, &graphics_queue_, swap_chain_desc))
		{
			std::cout << "[ERROR] SwapChain Initialize" << std::endl;
			return false;
		}

		swapchain_rtvs_.resize(swapchain_.NumResource());
		swapchain_resource_state_.resize(swapchain_.NumResource());
		for (auto i = 0u; i < swapchain_.NumResource(); ++i)
		{
			swapchain_rtvs_[i].Initialize(&device_, &swapchain_, i);
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

		if (!tex_depth_.Initialize(&device_, desc))
		{
			std::cout << "[ERROR] Create DepthTexture Initialize" << std::endl;
			assert(false);
		}

		if (!tex_depth_dsv_.Initialize(&device_, &tex_depth_, 0, 0, 1))
		{
			std::cout << "[ERROR] Create Dsv Initialize" << std::endl;
			assert(false);
		}

		tex_depth_state_ = desc.initial_state;
	}

	// RenderTarget Texture.
	{
		ngl::rhi::TextureDep::Desc desc = {};
		desc.bind_flag = ngl::rhi::ResourceBindFlag::RenderTarget | ngl::rhi::ResourceBindFlag::ShaderResource;
		desc.format = ngl::rhi::ResourceFormat::Format_R8G8B8A8_UNORM;
		desc.type = ngl::rhi::TextureType::Texture2D;
		desc.width = 256;
		desc.height = 256;

		if (!tex_rt_.Initialize(&device_, desc))
		{
			std::cout << "[ERROR] Create RenderTarget Texture Initialize" << std::endl;
			assert(false);
		}
		if (!tex_rt_srv_.InitializeAsTexture(&device_, &tex_rt_, 0, 1, 0, 1))
		{
			std::cout << "[ERROR] Create Srv Initialize" << std::endl;
			assert(false);
		}
		if (!tex_rt_rtv_.Initialize(&device_, &tex_rt_, 0, 0, 1))
		{
			std::cout << "[ERROR] Create Rtv Initialize" << std::endl;
			assert(false);
		}

		tex_rt_state_ = desc.initial_state;
	}

	// UnorderedAccess Texture.
	{
		ngl::rhi::TextureDep::Desc desc = {};
		desc.bind_flag = ngl::rhi::ResourceBindFlag::UnorderedAccess | ngl::rhi::ResourceBindFlag::ShaderResource;
		desc.format = ngl::rhi::ResourceFormat::Format_R8G8B8A8_UNORM;
		desc.type = ngl::rhi::TextureType::Texture2D;
		desc.width = 256;
		desc.height = 256;

		if (!tex_ua_.Initialize(&device_, desc))
		{
			std::cout << "[ERROR] Create RenderTarget Texture Initialize" << std::endl;
			assert(false);
		}

		if (!tex_ua_uav_.Initialize(&device_, &tex_ua_, 0, 0, 1))
		{
			std::cout << "[ERROR] Create Rtv Initialize" << std::endl;
			assert(false);
		}

		tex_ua_state_ = desc.initial_state;
	}

	ngl::rhi::GraphicsCommandListDep::Desc gcl_desc = {};
	if (!gfx_command_list_.Initialize(&device_, gcl_desc))
	{
		std::cout << "[ERROR] CommandList Initialize" << std::endl;
		return false;
	}

	if (!wait_fence_.Initialize(&device_))
	{
		std::cout << "[ERROR] Fence Initialize" << std::endl;
		return false;
	}


	clear_color_ = ngl::math::Vec4::Zero();

	{
		// HLSLからコンパイルして初期化.
		ngl::rhi::ShaderReflectionDep reflect02;
		{
			ngl::rhi::ShaderDep::InitFileDesc shader_desc = {};
			shader_desc.shader_file_path = "./src/ngl/data/shader/sample_vs.hlsl";
			shader_desc.entry_point_name = "main_vs";
			shader_desc.stage = ngl::rhi::ShaderStage::Vertex;
			shader_desc.shader_model_version = "6_0";

			if (!sample_vs_.Initialize(&device_, shader_desc))
			{
				std::cout << "[ERROR] Create rhi::ShaderDep" << std::endl;
			}
			reflect02.Initialize(&device_, &sample_vs_);
		}
		// HLSLからコンパイルして初期化.
		ngl::rhi::ShaderReflectionDep reflect00;
		{
			ngl::rhi::ShaderDep::InitFileDesc shader_desc = {};
			shader_desc.shader_file_path = "./src/ngl/data/shader/sample_ps.hlsl";
			shader_desc.entry_point_name = "main_ps";
			shader_desc.stage = ngl::rhi::ShaderStage::Pixel;
			shader_desc.shader_model_version = "6_0";

			if (!sample_ps_.Initialize(&device_, shader_desc))
			{
				std::cout << "[ERROR] Create rhi::ShaderDep" << std::endl;
			}
			reflect00.Initialize(&device_, &sample_ps_);
		}

		{
			ngl::rhi::ShaderDep::InitFileDesc shader_desc = {};
			shader_desc.shader_file_path = "./src/ngl/data/shader/sample_fullscr_procedural_vs.hlsl";
			shader_desc.entry_point_name = "main_vs";
			shader_desc.stage = ngl::rhi::ShaderStage::Vertex;
			shader_desc.shader_model_version = "6_0";

			if (!sample_fullscr_proc_vs_.Initialize(&device_, shader_desc))
			{
				std::cout << "[ERROR] Create rhi::ShaderDep" << std::endl;
			}
		}
		{
			ngl::rhi::ShaderDep::InitFileDesc shader_desc = {};
			shader_desc.shader_file_path = "./src/ngl/data/shader/sample_draw_procedural_ps.hlsl";
			shader_desc.entry_point_name = "main_ps";
			shader_desc.stage = ngl::rhi::ShaderStage::Pixel;
			shader_desc.shader_model_version = "6_0";

			if (!sample_fullscr_proc_ps_.Initialize(&device_, shader_desc))
			{
				std::cout << "[ERROR] Create rhi::ShaderDep" << std::endl;
			}
		}

	}

	// PSO
	{
		ngl::rhi::GraphicsPipelineStateDep::Desc desc = {};
		desc.vs = &sample_vs_;
		desc.ps = &sample_ps_;

		desc.num_render_targets = 1;
		desc.render_target_formats[0] = ngl::rhi::ResourceFormat::Format_R10G10B10A2_UNORM;

		desc.depth_stencil_state.depth_enable = true;
		desc.depth_stencil_state.depth_func = ngl::rhi::CompFunc::Less;
		desc.depth_stencil_state.depth_write_mask = ~ngl::u32(0);
		desc.depth_stencil_state.stencil_enable = false;
		desc.depth_stencil_format = tex_depth_.GetFormat();

		desc.blend_state.target_blend_states[0].blend_enable = false;
		desc.blend_state.target_blend_states[0].write_mask = ~ngl::u8(0);

		// 入力レイアウト
		std::array<ngl::rhi::InputElement, 2> input_elem_data;
		desc.input_layout.num_elements = static_cast<ngl::u32>(input_elem_data.size());
		desc.input_layout.p_input_elements = input_elem_data.data();
		input_elem_data[0].semantic_name = "POSITION";
		input_elem_data[0].semantic_index = 0;
		input_elem_data[0].format = ngl::rhi::ResourceFormat::Format_R32G32B32A32_FLOAT;
		input_elem_data[0].stream_slot = 0;
		input_elem_data[0].element_offset = 0;
		input_elem_data[1].semantic_name = "TEXCOORD";
		input_elem_data[1].semantic_index = 0;
		input_elem_data[1].format = ngl::rhi::ResourceFormat::Format_R32G32_FLOAT;
		input_elem_data[1].stream_slot = 0;
		input_elem_data[1].element_offset = sizeof(float) * 4;

		if (!sample_pso_.Initialize(&device_, desc))
		{
			std::cout << "[ERROR] Create rhi::GraphicsPipelineState" << std::endl;
		}
	}

	// InputLayout無しのPSO
	{
		ngl::rhi::GraphicsPipelineStateDep::Desc desc = {};
		desc.vs = &sample_fullscr_proc_vs_;
		desc.ps = &sample_fullscr_proc_ps_;

		desc.num_render_targets = 1;
		desc.render_target_formats[0] = ngl::rhi::ResourceFormat::Format_R8G8B8A8_UNORM;


		desc.blend_state.target_blend_states[0].blend_enable = false;
		desc.blend_state.target_blend_states[0].write_mask = ~ngl::u8(0);

		if (!sample_fullscr_proc_pso_.Initialize(&device_, desc))
		{
			std::cout << "[ERROR] Create rhi::GraphicsPipelineState" << std::endl;
		}
	}

	{
		// ConstantBuffer作成
		ngl::rhi::BufferDep::Desc cb_desc = {};
		cb_desc.heap_type = ngl::rhi::ResourceHeapType::Upload;
		cb_desc.bind_flag = (int)ngl::rhi::ResourceBindFlag::ConstantBuffer;
		cb_desc.element_byte_size = sizeof(CbSampleVs);
		cb_desc.element_count = 1;

		if (cb_sample_vs_.Initialize(&device_, cb_desc))
		{
			if (auto* map_data = reinterpret_cast<CbSampleVs*>(cb_sample_vs_.Map()))
			{
				map_data->cb_param_vs0 = ngl::math::Vec4(1.0f, 0.5f, 0.0f, 1.0f);

				cb_sample_vs_.Unmap();
			}
		}

		// ConstantBufferView作成
		ngl::rhi::ConstantBufferViewDep::Desc cbv_desc = {};
		cbv_sample_vs_.Initialize(&cb_sample_vs_, cbv_desc);
	}
	{
		// ConstantBuffer作成
		ngl::rhi::BufferDep::Desc cb_desc = {};
		cb_desc.heap_type = ngl::rhi::ResourceHeapType::Upload;
		cb_desc.bind_flag = (int)ngl::rhi::ResourceBindFlag::ConstantBuffer;
		cb_desc.element_byte_size = sizeof(CbSamplePs);
		cb_desc.element_count = 1;

		if (cb_sample_ps_.Initialize(&device_, cb_desc))
		{
			if (auto* map_data = reinterpret_cast<CbSamplePs*>(cb_sample_ps_.Map()))
			{
				map_data->cb_param0 = ngl::math::Vec4(1.0f, 0.5f, 0.0f, 1.0f);

				cb_sample_ps_.Unmap();
			}
		}

		// ConstantBufferView作成
		ngl::rhi::ConstantBufferViewDep::Desc cbv_desc = {};
		cbv_sample_ps_.Initialize(&cb_sample_ps_, cbv_desc);
	}

	{
		// VertexBuffer作成
		struct VertexPosUv
		{
			ngl::math::Vec4 pos;

			float u;
			float v;
		};

		constexpr float shape_scale = 0.95f;
		VertexPosUv sample_vtx_list[] =
		{
			{{-shape_scale, shape_scale, 0.0f, 1.0f},	0.0f, 0.0f },
			{{shape_scale, -shape_scale, 0.0f, 1.0f},	1.0f, 1.0f },
			{{-shape_scale, -shape_scale, 0.0f, 1.0f},	0.0f, 1.0f},

			{{-shape_scale, shape_scale, 0.0f, 1.0f},	0.0f, 0.0f },
			{{ shape_scale, shape_scale, 0.0f, 1.0f},	1.0f, 0.0f },
			{{ shape_scale, -shape_scale, 0.0f, 1.0f},	1.0f, 1.0f},
		};


		ngl::rhi::BufferDep::Desc vtx_desc = {};
		vtx_desc.heap_type = ngl::rhi::ResourceHeapType::Upload;
		vtx_desc.bind_flag = (int)ngl::rhi::ResourceBindFlag::VertexBuffer;
		vtx_desc.element_count = static_cast<ngl::u32>(std::size(sample_vtx_list));
		vtx_desc.element_byte_size = sizeof(sample_vtx_list[0]);

		if (vb_sample_.Initialize(&device_, vtx_desc))
		{
			if (auto* map_data = reinterpret_cast<VertexPosUv*>(vb_sample_.Map()))
			{
				memcpy(map_data, sample_vtx_list, sizeof(sample_vtx_list));

				vb_sample_.Unmap();
			}
		}

		ngl::rhi::VertexBufferViewDep::Desc vbv_desc{};
		if (!vbv_sample_.Initialize(&vb_sample_, vbv_desc))
		{
			assert(false);
		}
	}
	{
		int sample_index_data[] =
		{
			0, 1, 2,
			3, 4, 5,
		};
		ngl::rhi::BufferDep::Desc idx_desc = {};
		idx_desc.heap_type = ngl::rhi::ResourceHeapType::Upload;
		idx_desc.bind_flag = (int)ngl::rhi::ResourceBindFlag::IndexBuffer;
		idx_desc.element_count = static_cast<ngl::u32>(std::size(sample_index_data));
		idx_desc.element_byte_size = sizeof(sample_index_data[0]);

		if (!ib_sample_.Initialize(&device_, idx_desc))
		{
			assert(false);
		}
		if (auto* map_data = reinterpret_cast<int*>(ib_sample_.Map()))
		{
			memcpy(map_data, sample_index_data, sizeof(sample_index_data));
		}

		ngl::rhi::IndexBufferViewDep::Desc ibv_desc = {};
		if (!ibv_sample_.Initialize(&ib_sample_, ibv_desc))
		{
		}
	}


	{
		// Samplerテスト
		ngl::rhi::SamplerDep::Desc samp_desc = {};
		samp_desc.Filter = ngl::rhi::TextureFilterMode::Min_Linear_Mag_Linear_Mip_Linear;
		samp_desc.AddressU = ngl::rhi::TextureAddressMode::Clamp;
		samp_desc.AddressV = ngl::rhi::TextureAddressMode::Clamp;
		samp_desc.AddressW = ngl::rhi::TextureAddressMode::Clamp;
		// ダミー用Descriptorの1個分を除いた最大数まで確保するテスト.
		if (!samp_.Initialize(&device_, samp_desc))
		{
			std::cout << "[ERROR] Create rhi::SamplerDep" << std::endl;
			assert(false);
		}
	}

	{
		const char* mesh_file_box = "../third_party/assimp/test/models/FBX/box.fbx";
		const char* mesh_file_spider = "../third_party/assimp/test/models/FBX/spider.fbx";
		const char* mesh_file_sponza = "./data/model/sponza/sponza.obj";

		auto& ResourceMan = ngl::res::ResourceManager::Instance();

		// Mesh Component
		{
			{
				auto mc = std::make_shared<ngl::gfx::StaticMeshComponent>();
				mesh_comp_array_.push_back(mc);

				mc->SetMeshData(ResourceMan.LoadResMesh(&device_, mesh_file_sponza));
				mc->transform_.SetColumn3(ngl::math::Vec3(0, 0, 0));
			}

			{
				auto mc = std::make_shared<ngl::gfx::StaticMeshComponent>();
				mesh_comp_array_.push_back(mc);

				mc->SetMeshData(ResourceMan.LoadResMesh(&device_, mesh_file_sponza));
				mc->transform_.SetColumn3(ngl::math::Vec3(0, 0, 20));
			}

			for(int i = 0; i < 100; ++i)
			{
				auto mc = std::make_shared<ngl::gfx::StaticMeshComponent>();
				mesh_comp_array_.push_back(mc);
				mc->SetMeshData(ResourceMan.LoadResMesh(&device_, mesh_file_spider));



				constexpr int k_rand_f_div = 10000;
				const float randx = (std::rand() % k_rand_f_div) / (float)k_rand_f_div;
				const float randz = (std::rand() % k_rand_f_div) / (float)k_rand_f_div;
				const float randroty = (std::rand() % k_rand_f_div) / (float)k_rand_f_div;

				constexpr float placement_range = 600.0f;
				

				ngl::math::Mat44 tr = ngl::math::Mat44::Identity();
				tr.SetDiagonal(ngl::math::Vec4(0.3f));
				tr = ngl::math::Mat44::RotAxisY(randroty * ngl::math::k_pi_f * 2.0f) * tr;
				tr.SetColumn3(ngl::math::Vec4(placement_range* (randx * 2.0f - 1.0f), 0, placement_range* (randz * 2.0f - 1.0f), 1.0f));

				mc->transform_ = ngl::math::Mat34(tr);

				mc->test_render_info_ = (std::rand()&0x01);// 適当に設定.


				// 移動テスト用に追加.
				test_move_mesh_comp_array_.push_back(mc.get());
			}


			{
				auto mc = std::make_shared<ngl::gfx::StaticMeshComponent>();
				mesh_comp_array_.push_back(mc);

				mc->SetMeshData(ResourceMan.LoadResMesh(&device_, mesh_file_box));
				mc->transform_.SetDiagonal(ngl::math::Vec3(0.01f)).SetColumn3(ngl::math::Vec3(10, 30, 0));
			}
			{
				auto mc = std::make_shared<ngl::gfx::StaticMeshComponent>();
				mesh_comp_array_.push_back(mc);

				mc->SetMeshData(ResourceMan.LoadResMesh(&device_, mesh_file_box));
				mc->transform_.SetDiagonal(ngl::math::Vec3(0.01f)).SetColumn3(ngl::math::Vec3(0, 30, 0));
			}
			{
				auto mc = std::make_shared<ngl::gfx::StaticMeshComponent>();
				mesh_comp_array_.push_back(mc);

				mc->SetMeshData(ResourceMan.LoadResMesh(&device_, mesh_file_box));
				mc->transform_.SetDiagonal(ngl::math::Vec3(0.01f)).SetColumn3(ngl::math::Vec3(-10, 30, 0));
			}
		}
	}

	{
		// ShaderLibrary.
		ngl::rhi::ShaderDep::InitFileDesc shader_desc = {};
		shader_desc.stage = ngl::rhi::ShaderStage::ShaderLibrary;
		shader_desc.shader_model_version = "6_3";
		shader_desc.shader_file_path = "./src/ngl/data/shader/dxr_sample_lib.hlsl";
		if (!rt_shader_lib0_.Initialize(&device_, shader_desc))
		{
			std::cout << "[ERROR] Create DXR ShaderLib" << std::endl;
			assert(false);
		}

		// StateObject生成.
		{
			std::vector<ngl::gfx::RaytraceShaderRegisterInfo> shader_reg_info_array = {};
			{
				// Shader登録エントリ新規.
				auto shader_index = shader_reg_info_array.size();
				shader_reg_info_array.push_back({});

				// 関数登録元ShaderLib参照.
				shader_reg_info_array[shader_index].p_shader_library = &rt_shader_lib0_;

				// シェーダから公開するRaygenerationShader名.
				shader_reg_info_array[shader_index].ray_generation_shader_array.push_back("rayGen");

				// シェーダから公開するMissShader名.
				shader_reg_info_array[shader_index].miss_shader_array.push_back("miss");
				shader_reg_info_array[shader_index].miss_shader_array.push_back("miss2");

				// シェーダから公開するHitGroup関連情報.
				{
					auto hg_index = shader_reg_info_array[shader_index].hitgroup_array.size();
					shader_reg_info_array[shader_index].hitgroup_array.push_back({});

					shader_reg_info_array[shader_index].hitgroup_array[hg_index].hitgorup_name = "hitGroup";
					// このHitGroupはClosestHitのみ.
					shader_reg_info_array[shader_index].hitgroup_array[hg_index].closest_hit_name = "closestHit";
				}
				{
					auto hg_index = shader_reg_info_array[shader_index].hitgroup_array.size();
					shader_reg_info_array[shader_index].hitgroup_array.push_back({});

					shader_reg_info_array[shader_index].hitgroup_array[hg_index].hitgorup_name = "hitGroup2";
					// このHitGroupはClosestHitのみ.
					shader_reg_info_array[shader_index].hitgroup_array[hg_index].closest_hit_name = "closestHit2";
				}
			}

			if (!rt_state_object_.Initialize(&device_, shader_reg_info_array, sizeof(float) * 4, sizeof(float) * 2, 1))
			{
				assert(false);
				return false;
			}
		}

		// AS他.
		if (!rt_st_.Initialize(&device_, &rt_state_object_))
		{
			std::cout << "[ERROR] Create gfx::RaytraceStructureManager" << std::endl;
			assert(false);
		}

	}


	// テストコード
	TestCode();

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

	// 操作系.
	{
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
						camera_pos_ += camera_pose_.GetColumn1() * delta_sec * 5.0f;
					}
					if (window_.Dep().GetVirtualKeyState()[VK_CONTROL])
					{
						camera_pos_ += -camera_pose_.GetColumn1() * delta_sec * 5.0f;
					}
					if (window_.Dep().GetVirtualKeyState()[vk_a + 'w' - 'a'])
					{
						camera_pos_ += camera_pose_.GetColumn2() * delta_sec * 5.0f;
					}
					if (window_.Dep().GetVirtualKeyState()[vk_a + 's' - 'a'])
					{
						camera_pos_ += -camera_pose_.GetColumn2() * delta_sec * 5.0f;
					}
					if (window_.Dep().GetVirtualKeyState()[vk_a + 'd' - 'a'])
					{
						camera_pos_ += camera_pose_.GetColumn0() * delta_sec * 5.0f;
					}
					if (window_.Dep().GetVirtualKeyState()[vk_a + 'a' - 'a'])
					{
						camera_pos_ += -camera_pose_.GetColumn0() * delta_sec * 5.0f;
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

	// 描画用シーン情報.
	ngl::gfx::SceneRepresentation frame_scene = {};
	{
		for (auto& e : mesh_comp_array_)
		{
			frame_scene.mesh_instance_array_.push_back(e.get());
		}
	}

	// カメラ設定.
	rt_st_.SetCameraInfo(camera_pos_, camera_pose_.GetColumn2(), camera_pose_.GetColumn1());



	ngl::u32 screen_w, screen_h;
	window_.Impl()->GetScreenSize(screen_w, screen_h);

	{
		// クリアカラー操作.
		clear_color_.x = static_cast<float>(cos(app_sec_ * 2.0f * 3.14159f / 2.0f)) * 0.5f + 0.5f;
		clear_color_.y = static_cast<float>(cos(app_sec_ * 2.0f * 3.14159f / 2.25f)) * 0.5f + 0.5f;
		clear_color_.z = static_cast<float>(cos(app_sec_ * 2.0f * 3.14159f / 2.5f)) * 0.5f + 0.5f;
	}

	// Render Loop
	{
		// Deviceのフレーム準備
		device_.ReadyToNewFrame();

		const auto swapchain_index = swapchain_.GetCurrentBufferIndex();
		{
			gfx_command_list_.Begin();



			// RenderTargetとして描画したTextureをSrvとして利用するテスト.
			// 初回に一度だけ描画する.
			if(!tex_rt_rendered_)
			{
				tex_rt_rendered_ = true;

				// Rtvへ遷移.
				gfx_command_list_.ResourceBarrier(&tex_rt_, tex_rt_state_, ngl::rhi::ResourceState::RenderTarget);
				tex_rt_state_ = ngl::rhi::ResourceState::RenderTarget;

				{
					// ターゲットテクスチャへフルスクリーン描画.
					D3D12_VIEWPORT viewport;
					viewport.MinDepth = 0.0f;
					viewport.MaxDepth = 1.0f;
					viewport.TopLeftX = 0.0f;
					viewport.TopLeftY = 0.0f;
					viewport.Width = static_cast<float>(tex_rt_.GetWidth());
					viewport.Height = static_cast<float>(tex_rt_.GetHeight());
					gfx_command_list_.SetViewports(1, &viewport);

					D3D12_RECT scissor_rect;
					scissor_rect.left = 0;
					scissor_rect.top = 0;
					scissor_rect.right = tex_rt_.GetWidth();
					scissor_rect.bottom = tex_rt_.GetHeight();
					gfx_command_list_.SetScissor(1, &scissor_rect);


					const auto* p_rt = &tex_rt_rtv_;
					gfx_command_list_.SetRenderTargets(&p_rt, 1, nullptr);

					gfx_command_list_.SetPipelineState(&sample_fullscr_proc_pso_);

					gfx_command_list_.SetPrimitiveTopology(ngl::rhi::PrimitiveTopology::TriangleList);
					// 計算でTriangleを生成するVSでDraw.
					gfx_command_list_.DrawInstanced(3, 1, 0, 0);
				}

				// Srvへ遷移.
				gfx_command_list_.ResourceBarrier(&tex_rt_, tex_rt_state_, ngl::rhi::ResourceState::ShaderRead);
				tex_rt_state_ = ngl::rhi::ResourceState::ShaderRead;
			}


			// Raytrace Structure ビルド.
			{
				rt_st_.UpdateOnRender(&device_, &gfx_command_list_, frame_scene);
			}


			// Raytrace Dispatch.
			{
				rt_st_.DispatchRay(&gfx_command_list_);
			}

			{
				{
					// Swapchain State to RenderTarget
					gfx_command_list_.ResourceBarrier(&swapchain_, swapchain_index, swapchain_resource_state_[swapchain_index], ngl::rhi::ResourceState::RenderTarget);
					swapchain_resource_state_[swapchain_index] = ngl::rhi::ResourceState::RenderTarget;
				}
				{
					// Dsv State
					gfx_command_list_.ResourceBarrier(&tex_depth_, tex_depth_state_, ngl::rhi::ResourceState::DepthWrite);
					tex_depth_state_ = ngl::rhi::ResourceState::DepthWrite;
				}

				// Rtvクリア.
				gfx_command_list_.ClearRenderTarget(&swapchain_rtvs_[swapchain_.GetCurrentBufferIndex()], clear_color_.data);
				// Dsvクリア.
				gfx_command_list_.ClearDepthTarget(&tex_depth_dsv_, 1.0f, 0, true, false);

				// Rtv, Dsv セット.
				{
					const auto* p_rtv = &swapchain_rtvs_[swapchain_.GetCurrentBufferIndex()];
					gfx_command_list_.SetRenderTargets(&p_rtv, 1, &tex_depth_dsv_);
				}

#if 1
				// Draw Polygon
				{

					D3D12_VIEWPORT viewport;
					viewport.MinDepth = 0.0f;
					viewport.MaxDepth = 1.0f;
					viewport.TopLeftX = 0.0f;
					viewport.TopLeftY = 0.0f;
					viewport.Width = static_cast<float>(screen_w);
					viewport.Height = static_cast<float>(screen_h);
					gfx_command_list_.SetViewports(1, &viewport);

					D3D12_RECT scissor_rect;
					scissor_rect.left = 0;
					scissor_rect.top = 0;
					scissor_rect.right = screen_w;
					scissor_rect.bottom = screen_h;
					gfx_command_list_.SetScissor(1, &scissor_rect);

					gfx_command_list_.SetPipelineState(&sample_pso_);

					{
						ngl::rhi::DescriptorSetDep empty_desc_set;

						// DescriptorSetに名前で定数バッファViewをセット
						sample_pso_.SetDescriptorHandle(&empty_desc_set, "CbSampleVs", cbv_sample_vs_.GetView().cpu_handle);
						sample_pso_.SetDescriptorHandle(&empty_desc_set, "CbSamplePs", cbv_sample_ps_.GetView().cpu_handle);

						// Raytraceの出力バッファをシェーダリソースに利用するテスト.
						sample_pso_.SetDescriptorHandle(&empty_desc_set, "TexPs", rt_st_.GetResultSrv()->GetView().cpu_handle);


						sample_pso_.SetDescriptorHandle(&empty_desc_set, "SmpPs", samp_.GetView().cpu_handle);

						// DescriptorSetでViewを設定.
						gfx_command_list_.SetDescriptorSet(&sample_pso_, &empty_desc_set);
					}

					gfx_command_list_.SetPrimitiveTopology(ngl::rhi::PrimitiveTopology::TriangleList);

					gfx_command_list_.SetVertexBuffers(0, 1, &vbv_sample_.GetView());


					gfx_command_list_.SetIndexBuffer(&ibv_sample_.GetView());
					gfx_command_list_.DrawIndexedInstanced(6, 1, 0, 0, 0);
				}
#endif
			}


			// Swapchain State to Present
			gfx_command_list_.ResourceBarrier(&swapchain_, swapchain_index, swapchain_resource_state_[swapchain_index], ngl::rhi::ResourceState::Present);
			swapchain_resource_state_[swapchain_index] = ngl::rhi::ResourceState::Present;

			gfx_command_list_.End();
		}

		// CommandList Submit
		ngl::rhi::GraphicsCommandListDep* command_lists[] =
		{
			&gfx_command_list_
		};
		graphics_queue_.ExecuteCommandLists(static_cast<unsigned int>(std::size(command_lists)), command_lists);

		// Present
		swapchain_.GetDxgiSwapChain()->Present(1, 0);

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


void AppGame::TestCode()
{
	ngl::text::FixedString<8> fixed_text0("abc");
	ngl::text::FixedString<8> fixed_text1("abcd");
	ngl::text::FixedString<8> fixed_text2("01234567");
	ngl::text::FixedString<8> fixed_text3("012345678");

	std::unordered_map<ngl::text::FixedString<8>, int> fixstr_map;
	auto in0 = fixstr_map.insert(std::pair<ngl::text::FixedString<8>, int>(fixed_text0, 0));
	auto in1 = fixstr_map.insert(std::pair<ngl::text::FixedString<8>, int>(fixed_text1, 1));
	auto in2 = fixstr_map.insert(std::pair<ngl::text::FixedString<8>, int>(fixed_text2, 2));
	auto in3 = fixstr_map.insert(std::pair<ngl::text::FixedString<8>, int>(fixed_text3, 3));

	auto find0 = fixstr_map.find("");
	auto find1 = fixstr_map.find("a");
	auto find2 = fixstr_map.find("abc");
	auto find3 = fixstr_map.find("01234567");
	auto find4 = fixstr_map.find("012345678");

	if(true)
	{
		bool is_uav_test = true;


		// Buffer生成テスト
		ngl::rhi::BufferDep buffer0;
		ngl::rhi::BufferDep::Desc buffer_desc0 = {};
		buffer_desc0.element_byte_size = sizeof(ngl::u64);
		buffer_desc0.bind_flag = (int)ngl::rhi::ResourceBindFlag::ShaderResource;
		buffer_desc0.element_count = 1;
		buffer_desc0.initial_state = ngl::rhi::ResourceState::General;
		if(is_uav_test)
		{
			// UAV用設定.
			buffer_desc0.bind_flag |= ngl::rhi::ResourceBindFlag::UnorderedAccess;
			// UAVはDefaultHeap必須
			buffer_desc0.heap_type = ngl::rhi::ResourceHeapType::Default;
		}
		else
		{
			// CPU->GPU Uploadリソース
			buffer_desc0.heap_type = ngl::rhi::ResourceHeapType::Upload;
		}

		if (!buffer0.Initialize(&device_, buffer_desc0))
		{
			std::cout << "[ERROR] Create rhi::Buffer" << std::endl;
			assert(false);
		}

		if (auto* buffer0_map = reinterpret_cast<ngl::u64*>(buffer0.Map()))
		{
			*buffer0_map = 111u;
			buffer0.Unmap();
		}

		auto* persistent_desc_allocator = device_.GetPersistentDescriptorAllocator();

		// SRV生成テスト. persistent上に作成.
		auto pd_srv = persistent_desc_allocator->Allocate();
		if (pd_srv.allocator)
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srv_desc.Format = DXGI_FORMAT_UNKNOWN;
			srv_desc.Buffer.FirstElement = 0;
			srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			srv_desc.Buffer.NumElements = 1;
			srv_desc.Buffer.StructureByteStride = sizeof(ngl::u64);

			device_.GetD3D12Device()->CreateShaderResourceView(buffer0.GetD3D12Resource() , &srv_desc, pd_srv.cpu_handle);

			// 解放
			persistent_desc_allocator->Deallocate(pd_srv);
		}
		// UAV生成テスト. persistent上に作成.
		if (is_uav_test)
		{
			auto pd_uav = persistent_desc_allocator->Allocate();
			if (pd_uav.allocator)
			{
				D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
				uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
				uav_desc.Format = DXGI_FORMAT_UNKNOWN;
				uav_desc.Buffer.CounterOffsetInBytes = 0;// カウンタバッファ不使用の場合はゼロ
				uav_desc.Buffer.FirstElement = 0;
				uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
				uav_desc.Buffer.NumElements = 1;
				uav_desc.Buffer.StructureByteStride = sizeof(ngl::u64);

				device_.GetD3D12Device()->CreateUnorderedAccessView(buffer0.GetD3D12Resource(), nullptr, &uav_desc, pd_uav.cpu_handle);

				// 解放
				persistent_desc_allocator->Deallocate(pd_uav);
			}
		}
	}

	if (true)
	{
		// Textureテスト
		ngl::rhi::TextureDep::Desc tex_desc00 = {};
		tex_desc00.heap_type = ngl::rhi::ResourceHeapType::Default;
		tex_desc00.type = ngl::rhi::TextureType::Texture3D;
		tex_desc00.width = 64;
		tex_desc00.height = 64;
		tex_desc00.depth = 64;
		tex_desc00.format = ngl::rhi::ResourceFormat::Format_R16_FLOAT;
		tex_desc00.bind_flag = (int)ngl::rhi::ResourceBindFlag::ShaderResource;
		
		//tex_desc00.bind_flag |= ngl::rhi::ResourceBindFlag::RenderTarget;
		tex_desc00.bind_flag |= ngl::rhi::ResourceBindFlag::UnorderedAccess;// UAV

		ngl::rhi::TextureDep tex00;
		if (!tex00.Initialize( &device_, tex_desc00))
		{
			std::cout << "[ERROR] Create rhi::TextureDep" << std::endl;
			assert(false);
		}
	}

	if (true)
	{
		// Samplerテスト
		ngl::rhi::SamplerDep::Desc samp_desc = {};
		samp_desc.AddressU = ngl::rhi::TextureAddressMode::Repeat;
		samp_desc.AddressV = ngl::rhi::TextureAddressMode::Repeat;
		samp_desc.AddressW = ngl::rhi::TextureAddressMode::Repeat;
		samp_desc.BorderColor[0] = 0.0f;
		samp_desc.BorderColor[1] = 0.0f;
		samp_desc.BorderColor[2] = 0.0f;
		samp_desc.BorderColor[3] = 0.0f;
		samp_desc.ComparisonFunc = ngl::rhi::CompFunc::Never;
		samp_desc.Filter = ngl::rhi::TextureFilterMode::Min_Point_Mag_Point_Mip_Linear;
		samp_desc.MaxAnisotropy = 0;
		samp_desc.MaxLOD = FLT_MAX;
		samp_desc.MinLOD = 0.0f;
		samp_desc.MipLODBias = 0.0f;

		// ダミー用Descriptorの1個分を除いた最大数まで確保するテスト.
		ngl::rhi::SamplerDep samp[1];
		for (auto&& e : samp)
		{
			if (!e.Initialize(&device_, samp_desc))
			{
				std::cout << "[ERROR] Create rhi::SamplerDep" << std::endl;
				assert(false);
			}
		}
	}

	if (true)
	{
		// PSO
		{
			ngl::rhi::GraphicsPipelineStateDep pso;

			ngl::rhi::GraphicsPipelineStateDep::Desc desc = {};
			desc.vs = &sample_vs_;
			desc.ps = &sample_ps_;

			desc.num_render_targets = 1;
			desc.render_target_formats[0] = ngl::rhi::ResourceFormat::Format_R10G10B10A2_UNORM;

			desc.depth_stencil_state.depth_enable = false;
			desc.depth_stencil_state.stencil_enable = false;

			desc.blend_state.target_blend_states[0].blend_enable = false;;

			// 入力レイアウト
			std::array<ngl::rhi::InputElement, 2> input_elem_data;
			desc.input_layout.num_elements = static_cast<ngl::u32>(input_elem_data.size());
			desc.input_layout.p_input_elements = input_elem_data.data();
			input_elem_data[0].semantic_name = "POSITION";
			input_elem_data[0].semantic_index = 0;
			input_elem_data[0].format = ngl::rhi::ResourceFormat::Format_R32G32B32A32_FLOAT;
			input_elem_data[0].stream_slot = 0;
			input_elem_data[0].element_offset = 0;
			input_elem_data[1].semantic_name = "TEXCOORD";
			input_elem_data[1].semantic_index = 0;
			input_elem_data[1].format = ngl::rhi::ResourceFormat::Format_R32G32_FLOAT;
			input_elem_data[1].stream_slot = 0;
			input_elem_data[1].element_offset = sizeof(float) * 4;

			if (!pso.Initialize(&device_, desc))
			{
				std::cout << "[ERROR] Create rhi::GraphicsPipelineState" << std::endl;
				assert(false);
			}

			// DescriptorSet設定テスト
			{
				// Buffer生成テスト
				struct CbTest
				{
					float cb_param0 = 1.111f;
					ngl::u32 cb_param1 = 0;
				};
				ngl::rhi::BufferDep buffer0;
				ngl::rhi::BufferDep::Desc buffer_desc0 = {};
				buffer_desc0.element_byte_size = sizeof(CbTest);
				buffer_desc0.element_count = 1;
				buffer_desc0.bind_flag = (int)ngl::rhi::ResourceBindFlag::ConstantBuffer;
				buffer_desc0.initial_state = ngl::rhi::ResourceState::General;
				// CPU->GPU Uploadリソース
				buffer_desc0.heap_type = ngl::rhi::ResourceHeapType::Upload;

				if (!buffer0.Initialize(&device_, buffer_desc0))
				{
					std::cout << "[ERROR] Create rhi::Buffer" << std::endl;
					assert(false);
				}

				if (auto* buffer0_map = reinterpret_cast<CbTest*>(buffer0.Map()))
				{
					buffer0_map->cb_param0 = 2.0f;
					buffer0_map->cb_param1 = 1;
					buffer0.Unmap();
				}

				auto* persistent_desc_allocator = device_.GetPersistentDescriptorAllocator();
				// CBV生成テスト. persistent上に作成.
				auto pd_cbv = persistent_desc_allocator->Allocate();
				if (pd_cbv.allocator)
				{
					D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {};
					cbv_desc.BufferLocation = buffer0.GetD3D12Resource()->GetGPUVirtualAddress();
					cbv_desc.SizeInBytes = buffer0.GetBufferSize();
					device_.GetD3D12Device()->CreateConstantBufferView(&cbv_desc, pd_cbv.cpu_handle);

				}
				// psoで名前解決をしてDescSetにハンドルを設定するテスト.
				ngl::rhi::DescriptorSetDep desc_set;
				pso.SetDescriptorHandle(&desc_set, "CbTest", pd_cbv.cpu_handle);

				// 一応解放しておく
				persistent_desc_allocator->Deallocate(pd_cbv);
			}
		}
	}

	// シェーダテスト
	{
#if 0
		// バイナリ読み込み.
		{
			ngl::file::FileObject file_obj;
			ngl::rhi::ShaderDep	shader00;
			ngl::rhi::ShaderReflectionDep reflect00;
			file_obj.ReadFile("./data/sample_vs.cso");
			if (!shader00.Initialize(&device_, ngl::rhi::ShaderStage::Vertex, file_obj.GetFileData(), file_obj.GetFileSize()))
			{
				std::cout << "[ERROR] Create rhi::ShaderDep" << std::endl;
				assert(false);
			}
			reflect00.Initialize(&device_, &shader00);
		}
		// バイナリ読み込み.
		{
			ngl::file::FileObject file_obj;
			ngl::rhi::ShaderDep	shader01;
			ngl::rhi::ShaderReflectionDep reflect00;
			file_obj.ReadFile("./data/sample_ps.cso");
			if (!shader01.Initialize(&device_, ngl::rhi::ShaderStage::Pixel, file_obj.GetFileData(), file_obj.GetFileSize()))
			{
				std::cout << "[ERROR] Create rhi::ShaderDep" << std::endl;
				assert(false);
			}
			reflect00.Initialize(&device_, &shader01);
		}
#endif

		// HLSLからコンパイルして初期化.
		ngl::rhi::ShaderDep shader00;
		ngl::rhi::ShaderReflectionDep reflect00;
		{
			ngl::rhi::ShaderDep::InitFileDesc shader_desc = {};
			shader_desc.shader_file_path = "./src/ngl/data/shader/sample_ps.hlsl";
			shader_desc.entry_point_name = "main_ps";
			shader_desc.stage = ngl::rhi::ShaderStage::Pixel;
			shader_desc.shader_model_version = "5_0";

			if (!shader00.Initialize(&device_, shader_desc))
			{
				std::cout << "[ERROR] Create rhi::ShaderDep" << std::endl;
				assert(false);
			}
			reflect00.Initialize(&device_, &shader00);

			auto cb0 = reflect00.GetCbInfo(0);
			auto cb1 = reflect00.GetCbInfo(1);
			auto cb0_var0 = reflect00.GetCbVariableInfo(0, 0);
			auto cb0_var1 = reflect00.GetCbVariableInfo(0, 1);
			auto cb0_var2 = reflect00.GetCbVariableInfo(0, 2);
			auto cb1_var0 = reflect00.GetCbVariableInfo(1, 0);

			float default_value;
			reflect00.GetCbDefaultValue(0, 0, default_value);
			std::cout << "cb default value " << default_value << std::endl;
		}
		// HLSLからコンパイルして初期化.
		ngl::rhi::ShaderDep shader01;
		ngl::rhi::ShaderReflectionDep reflect01;
		{
			ngl::rhi::ShaderDep::InitFileDesc shader_desc = {};
			shader_desc.shader_file_path = "./src/ngl/data/shader/sample_ps.hlsl";
			shader_desc.entry_point_name = "main_ps";
			shader_desc.stage = ngl::rhi::ShaderStage::Pixel;
			shader_desc.shader_model_version = "6_0";

			if (!shader01.Initialize(&device_, shader_desc))
			{
				std::cout << "[ERROR] Create rhi::ShaderDep" << std::endl;
				assert(false);
			}
			reflect01.Initialize(&device_, &shader01);

			float default_value;
			reflect01.GetCbDefaultValue(0, 0, default_value);
			std::cout << "cb default value " << default_value << std::endl;
		}

		// HLSLからコンパイルして初期化.
		ngl::rhi::ShaderDep shader02;
		ngl::rhi::ShaderReflectionDep reflect02;
		{
			ngl::rhi::ShaderDep::InitFileDesc shader_desc = {};
			shader_desc.shader_file_path = "./src/ngl/data/shader/sample_vs.hlsl";
			shader_desc.entry_point_name = "main_vs";
			shader_desc.stage = ngl::rhi::ShaderStage::Vertex;
			shader_desc.shader_model_version = "5_0";

			if (!shader02.Initialize(&device_, shader_desc))
			{
				std::cout << "[ERROR] Create rhi::ShaderDep" << std::endl;
				assert(false);
			}
			reflect02.Initialize(&device_, &shader02);

			float default_value;
			reflect02.GetCbDefaultValue(0, 0, default_value);
			std::cout << "cb default value " << default_value << std::endl;
		}

	}

	// PersistentDescriptorAllocatorテスト. 確保と解放を繰り返すテスト.
	{
		ngl::time::Timer::Instance().StartTimer("PersistentDescriptorAllocatorTest");

		ngl::rhi::PersistentDescriptorAllocator* persistent_desc_allocator = device_.GetPersistentDescriptorAllocator();

		std::vector<ngl::rhi::PersistentDescriptorInfo> debug_alloc_pd;
		for (ngl::u32 i = 0u; i  < (100000); ++i)
		{
			auto pd0 = persistent_desc_allocator->Allocate();

			if (0 != pd0.allocator)
			{
				debug_alloc_pd.push_back(pd0);
			}

			const auto dealloc_index = std::rand() % debug_alloc_pd.size();
#if 1
			if (debug_alloc_pd[dealloc_index].allocator)
			{
				// ランダムに選んだものがまだDeallocされていなければDealloc
				persistent_desc_allocator->Deallocate(debug_alloc_pd[dealloc_index]);
				debug_alloc_pd[dealloc_index] = {};
			}
#endif
		}

		std::cout << "PersistentDescriptorAllocatorTest time -> " << ngl::time::Timer::Instance().GetElapsedSec("PersistentDescriptorAllocatorTest") << std::endl;
	}

	if(true)
	{
		// フレームでのDescriptorマネージャ初期化
		ngl::rhi::FrameDescriptorManager*	frame_desc_man = device_.GetFrameDescriptorManager();

		// バッファリング数分のフレームDescriptorインターフェース初期化
		const ngl::u32 buffer_count = 3;
		std::vector<ngl::rhi::FrameCommandListDescriptorInterface> frame_desc_interface;
		frame_desc_interface.resize(buffer_count);
		for (auto&& e : frame_desc_interface)
		{
			ngl::rhi::FrameCommandListDescriptorInterface::Desc frame_desc_interface_desc = {};
			frame_desc_interface_desc.stack_size = 2000;
			e.Initialize( frame_desc_man, frame_desc_interface_desc);
		}

		// インターフェースからそのフレーム用のDescriptorを取得,解放するテスト.
		ngl::u32 frame_index = 0;
		for (auto f_i = 0u; f_i < 1000; ++f_i)
		{
			// マネージャのフレーム開始処理で過去フレーム確保分の解放(実際にはDeviceのフレーム開始処理で実行される).
			frame_desc_man->ResetFrameDescriptor(frame_index);

			// インターフェイスのフレーム開始処理.
			frame_desc_interface[frame_index].ReadyToNewFrame(frame_index);

			// 確保テスト.
			for (auto alloc_i = 0u; alloc_i < 2000; ++alloc_i)
			{
				D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle;
				D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle;
				frame_desc_interface[frame_index].Allocate(16, cpu_handle, gpu_handle);
			}
			frame_index = (frame_index + 1) % buffer_count;
		}
	}

	if(true)
	{
		ngl::rhi::FrameDescriptorHeapPagePool* frame_desc_page_pool = device_.GetFrameDescriptorHeapPagePool();

		// インターフェースからそのフレーム用のDescriptorを取得,解放するテスト.
		//ngl::u32 frame_index = 0;
		//for (auto f_i = 0u; f_i < 5; ++f_i)
		{
			ngl::rhi::FrameDescriptorHeapPageInterface fmra_page_interface_sampler;
			{
				ngl::rhi::FrameDescriptorHeapPageInterface::Desc desc = {};
				desc.type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
				fmra_page_interface_sampler.Initialize(frame_desc_page_pool, desc);
			}

			ngl::rhi::FrameDescriptorHeapPageInterface fmra_page_interface_srv;
			{
				ngl::rhi::FrameDescriptorHeapPageInterface::Desc desc = {};
				desc.type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
				fmra_page_interface_srv.Initialize(frame_desc_page_pool, desc);
			}

			for (auto alloc_i = 0u; alloc_i < 2000; ++alloc_i)
			{
				{
					D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle;
					D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle;
					fmra_page_interface_sampler.Allocate(16, cpu_handle, gpu_handle);
				}
				{
					D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle;
					D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle;
					fmra_page_interface_srv.Allocate(16, cpu_handle, gpu_handle);
				}
			}
		}
	}
}
