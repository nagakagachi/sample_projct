#include <iostream>
#include <vector>


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


	ngl::rhi::ShaderDep							vs_sample_;
	ngl::rhi::ShaderDep							ps_sample_;

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
			swapchain_resource_state_[i] = ngl::rhi::ResourceState::Common;
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

	clear_color_[0] = 0.0f;
	clear_color_[1] = 0.0f;
	clear_color_[2] = 0.0f;
	clear_color_[3] = 1.0f;

	{
		{
			// Buffer生成テスト
			ngl::rhi::BufferDep buffer0;
			ngl::rhi::BufferDep::Desc buffer_desc0 = {};
			buffer_desc0.element_byte_size = sizeof(ngl::u64);
			buffer_desc0.element_count = 1;
			buffer_desc0.heap_type = ngl::rhi::ResourceHeapType::Upload;	// CPU->GPU Uploadリソース
			buffer_desc0.initial_state = ngl::rhi::ResourceState::General;

			if (!buffer0.Initialize(&device_, buffer_desc0))
			{
				std::cout << "ERROR: Create rhi::Buffer" << std::endl;
			}

			if (auto* buffer0_map = buffer0.Map<ngl::u64>())
			{
				*buffer0_map = 111u;
				buffer0.Unmap();
			}
		}
	}


	// シェーダ
	{
		// バイナリ読み込み.
		{
			ngl::file::FileObject file_obj;
			ngl::rhi::ShaderReflectionDep reflect00;
			file_obj.ReadFile("./data/sample_vs.cso");
			if (!vs_sample_.Initialize(&device_, file_obj.GetFileData(), file_obj.GetFileSize()))
			{
				std::cout << "ERROR: Create rhi::ShaderDep" << std::endl;
			}
			reflect00.Initialize(&device_, &vs_sample_);
			std::cout << "_" << std::endl;
		}
		// バイナリ読み込み.
		{
			ngl::file::FileObject file_obj;
			ngl::rhi::ShaderReflectionDep reflect00;
			file_obj.ReadFile("./data/sample_ps.cso");
			if (!ps_sample_.Initialize(&device_, file_obj.GetFileData(), file_obj.GetFileSize()))
			{
				std::cout << "ERROR: Create rhi::ShaderDep" << std::endl;
			}
			reflect00.Initialize(&device_, &ps_sample_);
			std::cout << "_" << std::endl;
		}

		// HLSLからコンパイルして初期化.
		ngl::rhi::ShaderDep shader00;
		ngl::rhi::ShaderReflectionDep reflect00;
		{
			ngl::rhi::ShaderDep::Desc shader_desc = {};
			shader_desc.shader_file_path = L"./src/ngl/resource/shader/sample_ps.hlsl";
			shader_desc.entry_point_name = "main_ps";
			shader_desc.stage = ngl::rhi::ShaderStage::Pixel;
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
			shader_desc.stage = ngl::rhi::ShaderStage::Pixel;
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

		std::cout << "_" << std::endl;
	}

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
			gfx_command_list_.ResourceBarrier(&swapchain_, swapchain_index, swapchain_resource_state_[swapchain_index], ngl::rhi::ResourceState::RenderTarget);
			swapchain_resource_state_[swapchain_index] = ngl::rhi::ResourceState::RenderTarget;

			gfx_command_list_.SetRenderTargetSingle(&swapchain_rtvs_[swapchain_.GetCurrentBufferIndex()]);

			gfx_command_list_.ClearRenderTarget(&swapchain_rtvs_[swapchain_.GetCurrentBufferIndex()], clear_color_);

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