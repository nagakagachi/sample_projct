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
	ngl::platform::CoreWindow	window_;



	ngl::rhi::DeviceDep					device_;
	ngl::rhi::GraphicsCommandQueueDep		graphics_queue_;
	ngl::rhi::SwapChainDep					swapchain_;

	std::vector<ngl::rhi::RenderTargetView> swapchain_rtvs_;

	ngl::rhi::GraphicsCommandListDep	gfx_command_list_;
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

	if (!device_.Initialize(&window_))
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

		swapchain_rtvs_.resize(swapchain_.NumBuffer());
		for (auto i = 0u; i < swapchain_.NumBuffer(); ++i)
		{
			swapchain_rtvs_[i].Initialize( &device_, &swapchain_, i );
		}

	}

	if (!gfx_command_list_.Initialize(&device_))
	{
		std::cout << "ERROR: CommandList Initialize" << std::endl;
		return false;
	}

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


	
	// Render Loop
	{
		gfx_command_list_.Begin();

		gfx_command_list_.SetRenderTargetSingle(&swapchain_rtvs_[swapchain_.GetCurrentBufferIndex()]);

		float clear_color[] = {1.0f, 0.0f, 0.0f, 1.0f};
		gfx_command_list_.ClearRenderTarget(&swapchain_rtvs_[swapchain_.GetCurrentBufferIndex()], clear_color);

		gfx_command_list_.End();

		ngl::rhi::GraphicsCommandListDep* command_lists[] =
		{
			&gfx_command_list_
		};
		graphics_queue_.ExecuteCommandLists(std::size(command_lists), command_lists);

		swapchain_.GetDxgiSwapChain()->Present(1, 0);

	}

	return true;
}