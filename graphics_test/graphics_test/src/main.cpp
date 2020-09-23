#include <iostream>
#include <vector>
#include <array>
#include <unordered_map>

#include "ngl/boot/boot_application.h"
#include "ngl/util/unique_ptr.h"
#include "ngl/platform/window.h"
#include "ngl/util/time/timer.h"
#include "ngl/memory/tlsf_memory_pool.h"
#include "ngl/memory/tlsf_allocator.h"
#include "ngl/file/file.h"
#include "ngl/util/shared_ptr.h"


// rhi
#include "ngl/rhi/d3d12/rhi.d3d12.h"
#include "ngl/rhi/d3d12/rhi_descriptor.d3d12.h"




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


	float clear_color_[4] = {0.0f};

	ngl::rhi::DeviceDep							device_;
	ngl::rhi::GraphicsCommandQueueDep			graphics_queue_;
	
	// SwapChain
	ngl::rhi::SwapChainDep						swapchain_;
	std::vector<ngl::rhi::RenderTargetViewDep>	swapchain_rtvs_;
	std::vector <ngl::rhi::ResourceState>		swapchain_resource_state_;

	// CommandQueue実行完了待機用Fence
	ngl::rhi::FenceDep							wait_fence_;
	// Fenceの初期値がゼロであるため待機用の値は1から開始.
	ngl::types::u64								wait_fence_value_ = 1;
	// CommandQueue実行完了待機用オブジェクト
	ngl::rhi::WaitOnFenceSignalDep				wait_signal_;



	ngl::rhi::GraphicsCommandListDep			gfx_command_list_;


	ngl::rhi::ShaderDep							sample_vs_;
	ngl::rhi::ShaderDep							sample_ps_;


	// cbv srv uav用PersistentDescriptorAllocator
	ngl::rhi::PersistentDescriptorAllocator persistent_cbv_srv_uav_allocator_;
};




int main()
{
	std::cout << "Hello World!" << std::endl;
	ngl::time::Timer::Instance().StartTimer("AppGameTime");

	{
		ngl::UniquePtr<ngl::boot::BootApplication> boot = ngl::boot::BootApplication::Create();
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
	gfx_command_list_.Finalize();

	swapchain_.Finalize();
	graphics_queue_.Finalize();
	device_.Finalize();
}

bool AppGame::Initialize()
{
	// ウィンドウ作成
	if(!window_.Initialize(_T("Test Window"), 1280, 720))
	{
		return false;
	}

	if (!device_.Initialize(&window_, true))
	{
		std::cout << "ERROR: Device Initialize" << std::endl;
		return false;
	}

	if (!graphics_queue_.Initialize(&device_))
	{
		std::cout << "ERROR: Command Queue Initialize" << std::endl;
		return false;
	}
	{
		ngl::rhi::SwapChainDep::Desc swap_chain_desc;
		swap_chain_desc.buffer_count = 2;
		swap_chain_desc.format = DXGI_FORMAT_R10G10B10A2_UNORM;
		if (!swapchain_.Initialize(&device_, &graphics_queue_, swap_chain_desc))
		{
			std::cout << "ERROR: SwapChain Initialize" << std::endl;
			return false;
		}

		swapchain_rtvs_.resize(swapchain_.NumResource());
		swapchain_resource_state_.resize(swapchain_.NumResource());
		for (auto i = 0u; i < swapchain_.NumResource(); ++i)
		{
			swapchain_rtvs_[i].Initialize( &device_, &swapchain_, i );
			swapchain_resource_state_[i] = ngl::rhi::ResourceState::COMMON;
		}

	}

	if (!gfx_command_list_.Initialize(&device_))
	{
		std::cout << "ERROR: CommandList Initialize" << std::endl;
		return false;
	}

	if (!wait_fence_.Initialize(&device_))
	{
		std::cout << "ERROR: Fence Initialize" << std::endl;
		return false;
	}

	// cbv srv uav用PersistentDescriptorAllocator初期化
	{
		ngl::rhi::PersistentDescriptorAllocator::Desc pda_desc;
		pda_desc.size = 65535;
		pda_desc.type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		if (!persistent_cbv_srv_uav_allocator_.Initialize(&device_, pda_desc))
		{
			std::cout << "ERROR: PersistentDescriptorAllocator Initialize" << std::endl;
			return false;
		}
	}

	clear_color_[0] = 0.0f;
	clear_color_[1] = 0.0f;
	clear_color_[2] = 0.0f;
	clear_color_[3] = 1.0f;


	{
		// HLSLからコンパイルして初期化.
		ngl::rhi::ShaderReflectionDep reflect02;
		{
			ngl::rhi::ShaderDep::Desc shader_desc = {};
			shader_desc.shader_file_path = L"./src/ngl/resource/shader/sample_vs.hlsl";
			shader_desc.entry_point_name = "main_vs";
			shader_desc.stage = ngl::rhi::ShaderStage::VERTEX_SHADER;
			shader_desc.shader_model_version = "5_0";

			if (!sample_vs_.Initialize(&device_, shader_desc))
			{
				std::cout << "ERROR: Create rhi::ShaderDep" << std::endl;
			}
			reflect02.Initialize(&device_, &sample_vs_);
		}
		// HLSLからコンパイルして初期化.
		ngl::rhi::ShaderReflectionDep reflect00;
		{
			ngl::rhi::ShaderDep::Desc shader_desc = {};
			shader_desc.shader_file_path = L"./src/ngl/resource/shader/sample_ps.hlsl";
			shader_desc.entry_point_name = "main_ps";
			shader_desc.stage = ngl::rhi::ShaderStage::PIXEL_SHADER;
			shader_desc.shader_model_version = "5_0";

			if (!sample_ps_.Initialize(&device_, shader_desc))
			{
				std::cout << "ERROR: Create rhi::ShaderDep" << std::endl;
			}
			reflect00.Initialize(&device_, &sample_ps_);
		}
	}

	// PSO
	ngl::rhi::GraphicsPipelineStateDep pso;
	{
		ngl::rhi::GraphicsPipelineStateDep::Desc desc = {};
		desc.vs = &sample_vs_;
		desc.ps = &sample_ps_;

		desc.num_render_targets = 1;
		desc.render_target_formats[0] = ngl::rhi::ResourceFormat::NGL_FORMAT_R10G10B10A2_UNORM;
		
		desc.depth_stencil_state.depth_enable = false;
		desc.depth_stencil_state.stencil_enable = false;

		desc.blend_state.target_blend_states[0].blend_enable = false;;

		// 入力レイアウト
		std::array<ngl::rhi::InputElement, 1> input_elem_data;
		desc.input_layout.num_elements = input_elem_data.size();
		desc.input_layout.p_input_elements = input_elem_data.data();
		input_elem_data[0].semantic_name = "POSITION";
		input_elem_data[0].semantic_index = 0;
		input_elem_data[0].format = ngl::rhi::ResourceFormat::NGL_FORMAT_R32G32B32A32_FLOAT;
		input_elem_data[0].stream_slot = 0;
		input_elem_data[0].element_offset = 0;

		if (!pso.Initialize(&device_, desc))
		{
			std::cout << "ERROR: Create rhi::GraphicsPipelineState" << std::endl;
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

		std::cout << "Frame Second: " << frame_sec_ << std::endl;

		// 再スタート
		ngl::time::Timer::Instance().StartTimer("app_frame_sec");
	}

	{
		auto c0 = static_cast<float>(cos(app_sec_ * 2.0f * 3.14159f / 2.0f));
		auto c1 = static_cast<float>(cos(app_sec_ * 2.0f * 3.14159f / 2.25f));
		auto c2 = static_cast<float>(cos(app_sec_ * 2.0f * 3.14159f / 2.5f));

		clear_color_[0] = c0 * 0.5f + 0.5f;
		clear_color_[1] = c1 * 0.5f + 0.5f;
		clear_color_[2] = c2 * 0.5f + 0.5f;
	}

	// Render Loop
	{
		const auto swapchain_index = swapchain_.GetCurrentBufferIndex();

		{
			gfx_command_list_.Begin();

			// Swapchain State to RenderTarget
			gfx_command_list_.ResourceBarrier(&swapchain_, swapchain_index, swapchain_resource_state_[swapchain_index], ngl::rhi::ResourceState::RENDER_TARGET);
			swapchain_resource_state_[swapchain_index] = ngl::rhi::ResourceState::RENDER_TARGET;

			gfx_command_list_.SetRenderTargetSingle(&swapchain_rtvs_[swapchain_.GetCurrentBufferIndex()]);

			gfx_command_list_.ClearRenderTarget(&swapchain_rtvs_[swapchain_.GetCurrentBufferIndex()], clear_color_);

			// Swapchain State to Present
			gfx_command_list_.ResourceBarrier(&swapchain_, swapchain_index, swapchain_resource_state_[swapchain_index], ngl::rhi::ResourceState::PRESENT);
			swapchain_resource_state_[swapchain_index] = ngl::rhi::ResourceState::PRESENT;

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


	{
		bool is_uav_test = false;


		// Buffer生成テスト
		ngl::rhi::BufferDep buffer0;
		ngl::rhi::BufferDep::Desc buffer_desc0 = {};
		buffer_desc0.element_byte_size = sizeof(ngl::u64);
		buffer_desc0.element_count = 1;
		buffer_desc0.initial_state = ngl::rhi::ResourceState::GENERAL;
		if(is_uav_test)
		{
			// UAV用設定.
			buffer_desc0.allow_uav = true;
			// UAVはDefaultHeap必須
			buffer_desc0.heap_type = ngl::rhi::ResourceHeapType::DEFAULT;
		}
		else
		{
			// CPU->GPU Uploadリソース
			buffer_desc0.heap_type = ngl::rhi::ResourceHeapType::UPLOAD;
		}

		if (!buffer0.Initialize(&device_, buffer_desc0))
		{
			std::cout << "ERROR: Create rhi::Buffer" << std::endl;
		}

		if (auto* buffer0_map = buffer0.Map<ngl::u64>())
		{
			*buffer0_map = 111u;
			buffer0.Unmap();
		}


		// CBV生成テスト. persistent上に作成.
		auto pd_cbv = persistent_cbv_srv_uav_allocator_.Allocate();
		if (pd_cbv.allocator)
		{
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {};
			cbv_desc.BufferLocation = buffer0.GetD3D12Resource()->GetGPUVirtualAddress();
			cbv_desc.SizeInBytes = buffer0.GetBufferSize();
			device_.GetD3D12Device()->CreateConstantBufferView(&cbv_desc, pd_cbv.cpu_handle);

			// 解放
			persistent_cbv_srv_uav_allocator_.Deallocate(pd_cbv);
		}

		// SRV生成テスト. persistent上に作成.
		auto pd_srv = persistent_cbv_srv_uav_allocator_.Allocate();
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
			persistent_cbv_srv_uav_allocator_.Deallocate(pd_srv);
		}
		// UAV生成テスト. persistent上に作成.
		if (is_uav_test)
		{
			auto pd_uav = persistent_cbv_srv_uav_allocator_.Allocate();
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
				persistent_cbv_srv_uav_allocator_.Deallocate(pd_uav);
			}
		}
	}

	// シェーダテスト
	{
// バイナリ読み込み.
		{
		ngl::file::FileObject file_obj;
		ngl::rhi::ShaderDep	shader00;
		ngl::rhi::ShaderReflectionDep reflect00;
		file_obj.ReadFile("./data/sample_vs.cso");
		if (!shader00.Initialize(&device_, file_obj.GetFileData(), file_obj.GetFileSize()))
		{
			std::cout << "ERROR: Create rhi::ShaderDep" << std::endl;
		}
		reflect00.Initialize(&device_, &shader00);
		std::cout << "_" << std::endl;
		}
		// バイナリ読み込み.
		{
			ngl::file::FileObject file_obj;
			ngl::rhi::ShaderDep	shader01;
			ngl::rhi::ShaderReflectionDep reflect00;
			file_obj.ReadFile("./data/sample_ps.cso");
			if (!shader01.Initialize(&device_, file_obj.GetFileData(), file_obj.GetFileSize()))
			{
				std::cout << "ERROR: Create rhi::ShaderDep" << std::endl;
			}
			reflect00.Initialize(&device_, &shader01);
			std::cout << "_" << std::endl;
		}

		// HLSLからコンパイルして初期化.
		ngl::rhi::ShaderDep shader00;
		ngl::rhi::ShaderReflectionDep reflect00;
		{
			ngl::rhi::ShaderDep::Desc shader_desc = {};
			shader_desc.shader_file_path = L"./src/ngl/resource/shader/sample_ps.hlsl";
			shader_desc.entry_point_name = "main_ps";
			shader_desc.stage = ngl::rhi::ShaderStage::PIXEL_SHADER;
			shader_desc.shader_model_version = "5_0";

			if (!shader00.Initialize(&device_, shader_desc))
			{
				std::cout << "ERROR: Create rhi::ShaderDep" << std::endl;
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
			ngl::rhi::ShaderDep::Desc shader_desc = {};
			shader_desc.shader_file_path = L"./src/ngl/resource/shader/sample_ps.hlsl";
			shader_desc.entry_point_name = "main_ps";
			shader_desc.stage = ngl::rhi::ShaderStage::PIXEL_SHADER;
			shader_desc.shader_model_version = "6_0";

			if (!shader01.Initialize(&device_, shader_desc))
			{
				std::cout << "ERROR: Create rhi::ShaderDep" << std::endl;
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
			ngl::rhi::ShaderDep::Desc shader_desc = {};
			shader_desc.shader_file_path = L"./src/ngl/resource/shader/sample_vs.hlsl";
			shader_desc.entry_point_name = "main_vs";
			shader_desc.stage = ngl::rhi::ShaderStage::VERTEX_SHADER;
			shader_desc.shader_model_version = "5_0";

			if (!shader02.Initialize(&device_, shader_desc))
			{
				std::cout << "ERROR: Create rhi::ShaderDep" << std::endl;
			}
			reflect02.Initialize(&device_, &shader02);

			float default_value;
			reflect02.GetCbDefaultValue(0, 0, default_value);
			std::cout << "cb default value " << default_value << std::endl;
		}

		std::cout << "_" << std::endl;
	}


	// PersistentDescriptorAllocatorテスト. 適当なサイズで初期化して確保と解放を繰り返すテスト.
	{
		ngl::time::Timer::Instance().StartTimer("PersistentDescriptorAllocatorTest");

		ngl::rhi::PersistentDescriptorAllocator persistent_desc_allocator0;

		ngl::rhi::PersistentDescriptorAllocator::Desc pda_desc;
		pda_desc.size = 65535;
		pda_desc.type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		if (!persistent_desc_allocator0.Initialize(&device_, pda_desc))
		{
			std::cout << "ERROR: Create rhi::PersistentDescriptorAllocator" << std::endl;
		}
		std::vector<ngl::rhi::PersistentDescriptorInfo> debug_alloc_pd;
		for (ngl::u32 i = 0u; i  < (pda_desc.size); ++i)
		{
			auto pd0 = persistent_desc_allocator0.Allocate();

			if (0 != pd0.allocator)
			{
				debug_alloc_pd.push_back(pd0);
			}

			const auto dealloc_index = std::rand() % debug_alloc_pd.size();
#if 1
			if (debug_alloc_pd[dealloc_index].allocator)
			{
				// ランダムに選んだものがまだDeallocされていなければDealloc
				persistent_desc_allocator0.Deallocate(debug_alloc_pd[dealloc_index]);
				debug_alloc_pd[dealloc_index] = {};
			}
#endif
		}

		std::cout << "PersistentDescriptorAllocatorTest time -> " << ngl::time::Timer::Instance().GetElapsedSec("PersistentDescriptorAllocatorTest") << std::endl;
		std::cout << "_" << std::endl;
	}

}
