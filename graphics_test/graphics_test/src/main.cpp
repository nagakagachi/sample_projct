#include <iostream>

#include "ngl/boot/boot_application.h"
#include "ngl/util/unique_ptr.h"
#include "ngl/platform/window.h"


#include "ngl/util/time/timer.h"

#include "ngl/memory/tlsf_memory_pool.h"
#include "ngl/memory/tlsf_allocator.h"

#include "ngl/file/file.h"

#include "ngl/util/shared_ptr.h"




// ------------------------------------------------------------
// ------------------------------------------------------------
#include "ngl/platform/win/window.win.h"

#include <d3d12.h>
#if 1
#include <dxgi1_6.h>
#else
#include <dxgi1_4.h>
#endif


#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
// ------------------------------------------------------------
// ------------------------------------------------------------

// Test Rhi Device
class RhiDeviceDX12
{
public:
	using DXGI_FACTORY_TYPE = IDXGIFactory6;

	RhiDeviceDX12()
	{
	}
	~RhiDeviceDX12()
	{
		Finalize();
	}

	bool Initialize(ngl::platform::CoreWindow* window)
	{
		if (!window)
			return false;

		p_window_ = window;

		{
			if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&p_factory_))))
			{
				// DXGIファクトリ生成失敗.
				std::cout << "ERROR: Create DXGIFactory" << std::endl;
				return false;
			}
		}
		// TODO. アダプタ検索する場合はここでFactoryから.

		{
			D3D_FEATURE_LEVEL feature_levels[] =
			{
				D3D_FEATURE_LEVEL_12_1,
				D3D_FEATURE_LEVEL_12_0,
				D3D_FEATURE_LEVEL_11_1,
				D3D_FEATURE_LEVEL_11_0,
			};

			device_feature_level_ = {};
			for (auto l : feature_levels)
			{
				if (SUCCEEDED(D3D12CreateDevice(nullptr, l, IID_PPV_ARGS(&p_device_))))
				{
					device_feature_level_ = l;
					break;
				}
			}
			if (!p_device_)
			{
				// デバイス生成失敗.
				std::cout << "ERROR: Create Device" << std::endl;
				return false;
			}
		}
		return true;
	}
	void Finalize()
	{
		if (p_device_)
		{
			p_device_->Release();
			p_device_ = nullptr;
		}
		if (p_factory_)
		{
			p_factory_->Release();
			p_factory_ = nullptr;
		}
	}

	ngl::platform::CoreWindow* GetWindow()
	{
		return p_window_;
	}

	ID3D12Device* GetD3D12Device()
	{
		return p_device_;
	}
	DXGI_FACTORY_TYPE* GetDxgiFactory()
	{
		return p_factory_;
	}

private:
	ngl::platform::CoreWindow* p_window_ = nullptr;

	D3D_FEATURE_LEVEL device_feature_level_ = {};
	ID3D12Device* p_device_ = nullptr;
	DXGI_FACTORY_TYPE* p_factory_ = nullptr;
};

// Test Rhi CommandList
class RhiGraphicsCommandListDX12
{
public:
	RhiGraphicsCommandListDX12()
	{
	}
	~RhiGraphicsCommandListDX12()
	{
		Finalize();
	}
	bool Initialize(RhiDeviceDX12* p_device)
	{
		if (!p_device)
			return false;

		// Command Allocator
		if (FAILED(p_device->GetD3D12Device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&p_command_allocator_))))
		{
			std::cout << "ERROR: Create Command Allocator" << std::endl;
			return false;
		}

		// Command List
		if (FAILED(p_device->GetD3D12Device()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, p_command_allocator_, nullptr, IID_PPV_ARGS(&p_command_list_))))
		{
			std::cout << "ERROR: Create Command List" << std::endl;
			return false;
		}
		return true;
	}
	void Finalize()
	{
		if (p_command_list_)
		{
			p_command_list_->Release();
			p_command_list_ = nullptr;
		}
		if (p_command_allocator_)
		{
			p_command_allocator_->Release();
			p_command_allocator_ = nullptr;
		}
	}

private:
	ID3D12CommandAllocator* p_command_allocator_ = nullptr;
	ID3D12GraphicsCommandList* p_command_list_ = nullptr;
};


class RhiGraphicsCommandQueue
{
public:
	RhiGraphicsCommandQueue()
	{
	}
	~RhiGraphicsCommandQueue()
	{
		Finalize();
	}

	// TODO. ここでCommandQueue生成時に IGIESW .exe found in whitelist: NO というメッセージがVSログに出力される. 意味と副作用は現状不明.
	bool Initialize(RhiDeviceDX12* p_device)
	{
		if (!p_device)
			return false;

		D3D12_COMMAND_QUEUE_DESC desc = {};
		// Graphics用
		desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

		desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		desc.NodeMask = 0;
		desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;

		if (FAILED(p_device->GetD3D12Device()->CreateCommandQueue(&desc, IID_PPV_ARGS(&p_command_queue_))))
		{
			std::cout << "ERROR: Create Command Queue" << std::endl;
			return false;
		}

		return true;
	}

	void Finalize()
	{
		if (p_command_queue_)
		{
			p_command_queue_->Release();
			p_command_queue_ = nullptr;
		}
	}

	ID3D12CommandQueue* GetD3D12CommandQueue()
	{
		return p_command_queue_;
	}

private:
	ID3D12CommandQueue* p_command_queue_ = nullptr;
};


class RhiSwapChain
{
public:
	struct Desc
	{
		DXGI_FORMAT		format	= {};
		unsigned int	buffer_count = 2;
	};

	RhiSwapChain()
	{
	}
	~RhiSwapChain()
	{
		Finalize();
	}
	bool Initialize(RhiDeviceDX12* p_device, RhiGraphicsCommandQueue* p_graphics_command_queu, const Desc& desc)
	{
		if (!p_device || !p_graphics_command_queu)
			return false;

		// 依存部からHWND取得
		auto&& window_dep = p_device->GetWindow()->Dep();
		auto&& hwnd = window_dep.GetWindowHandle();
		unsigned int screen_w = 0;
		unsigned int screen_h = 0;
		p_device->GetWindow()->Impl()->GetScreenSize(screen_w, screen_h);


		DXGI_SWAP_CHAIN_DESC1 obj_desc = {};
		obj_desc.Width = screen_w;
		obj_desc.Height = screen_h;
		obj_desc.Format = desc.format;
		obj_desc.Stereo = false;
		obj_desc.SampleDesc.Count = 1;
		obj_desc.SampleDesc.Quality = 0;
		obj_desc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
		obj_desc.BufferCount = desc.buffer_count;

		obj_desc.Scaling = DXGI_SCALING_STRETCH;
		obj_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		obj_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

		obj_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;


		if (FAILED(p_device->GetDxgiFactory()->CreateSwapChainForHwnd(p_graphics_command_queu->GetD3D12CommandQueue(), hwnd, &obj_desc, nullptr, nullptr, (IDXGISwapChain1**)(&p_swapchain_))))
		{
			std::cout << "ERROR: Create Command Queue" << std::endl;
			return false;
		}

		return true;
	}
	void Finalize()
	{
		if (p_swapchain_)
		{
			p_swapchain_->Release();
			p_swapchain_ = nullptr;
		}
	}
private:
	IDXGISwapChain4* p_swapchain_ = nullptr;
};



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

	RhiDeviceDX12				device_;
	RhiGraphicsCommandQueue		graphics_queue_;
	RhiSwapChain				swap_chain_;


	RhiGraphicsCommandListDX12	gfx_command_list_;
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

	swap_chain_.Finalize();
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

	RhiSwapChain::Desc swap_chain_desc;
	swap_chain_desc.buffer_count = 2;
	swap_chain_desc.format = DXGI_FORMAT_R10G10B10A2_UNORM;
	if (!swap_chain_.Initialize(&device_, &graphics_queue_, swap_chain_desc))
	{
		std::cout << "ERROR: SwapChain Initialize" << std::endl;
		return false;
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

	return true;
}