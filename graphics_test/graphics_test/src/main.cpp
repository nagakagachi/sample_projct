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
	RhiDeviceDX12()
	{
	}
	~RhiDeviceDX12()
	{
		Finalize();
	}

	bool Initialize(ngl::platform::CoreWindow* window)
	{
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

	ID3D12Device* GetD3D12Device()
	{
		return p_device_;
	}

private:
	D3D_FEATURE_LEVEL device_feature_level_ = {};
	ID3D12Device* p_device_ = nullptr;
	IDXGIFactory6* p_factory_ = nullptr;
	IDXGISwapChain4* p_swapchain_ = nullptr;
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

	RhiDeviceDX12			device_;

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