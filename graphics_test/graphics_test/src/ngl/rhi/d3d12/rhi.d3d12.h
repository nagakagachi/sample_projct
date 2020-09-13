#pragma once

#include <iostream>
#include <vector>


#include "ngl/platform/win/window.win.h"

#include <d3d12.h>
#if 1
#include <dxgi1_6.h>
#else
#include <dxgi1_4.h>
#endif


#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")


namespace ngl
{
	namespace rhi
	{
		class RenderTargetView;


		// Test Rhi Device
		class DeviceDep
		{
		public:
			using DXGI_FACTORY_TYPE = IDXGIFactory6;

			DeviceDep()
			{
			}
			~DeviceDep()
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
		class GraphicsCommandListDep
		{
		public:
			GraphicsCommandListDep();
			~GraphicsCommandListDep();
			bool Initialize(DeviceDep* p_device);
			void Finalize();

			ID3D12GraphicsCommandList* GetD3D12GraphicsCommandList()
			{
				return p_command_list_;
			}

			void Begin();
			void End();
			void SetRenderTargetSingle(RenderTargetView* p_rtv);
			void ClearRenderTarget(RenderTargetView* p_rtv, float(color)[4]);
		private:
			ID3D12CommandAllocator* p_command_allocator_ = nullptr;
			ID3D12GraphicsCommandList* p_command_list_ = nullptr;
		};


		class GraphicsCommandQueueDep
		{
		public:
			GraphicsCommandQueueDep()
			{
			}
			~GraphicsCommandQueueDep()
			{
				Finalize();
			}

			// TODO. ここでCommandQueue生成時に IGIESW .exe found in whitelist: NO というメッセージがVSログに出力される. 意味と副作用は現状不明.
			bool Initialize(DeviceDep* p_device)
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

			void ExecuteCommandLists(unsigned int num_command_list, GraphicsCommandListDep** p_command_lists)
			{
				// 一時バッファに詰める
				p_command_list_array_.clear();
				for (auto i = 0u; i < num_command_list; ++i)
				{
					p_command_list_array_.push_back(p_command_lists[i]->GetD3D12GraphicsCommandList());
				}

				p_command_queue_->ExecuteCommandLists(num_command_list, &(p_command_list_array_[0]));
			}

		private:
			ID3D12CommandQueue* p_command_queue_ = nullptr;
			std::vector<ID3D12CommandList*> p_command_list_array_;
		};


		class SwapChainDep
		{
		public:
			using DXGI_SWAPCHAIN_TYPE = IDXGISwapChain4;

			struct Desc
			{
				DXGI_FORMAT		format = {};
				unsigned int	buffer_count = 2;
			};

			SwapChainDep()
			{
			}
			~SwapChainDep()
			{
				Finalize();
			}
			bool Initialize(DeviceDep* p_device, GraphicsCommandQueueDep* p_graphics_command_queu, const Desc& desc)
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

				// Resource取得
				num_resource_ = desc.buffer_count;
				p_resources_ = new ID3D12Resource * [num_resource_];
				memset(p_resources_, 0x00, sizeof(*p_resources_) * num_resource_);
				for (auto i = 0u; i < num_resource_; ++i)
				{
					if (FAILED(p_swapchain_->GetBuffer(i, IID_PPV_ARGS(&p_resources_[i]))))
					{
						std::cout << "ERROR: Get SwapChain Buffer " << i << std::endl;
					}
				}

				return true;
			}
			void Finalize()
			{
				if(p_resources_)
				{
					for (auto i = 0u; i < num_resource_; ++i)
					{
						if (p_resources_[i])
						{
							p_resources_[i]->Release();
							p_resources_[i] = nullptr;
						}
					}
					delete[] p_resources_;
					p_resources_ = nullptr;
				}
				if (p_swapchain_)
				{
					p_swapchain_->Release();
					p_swapchain_ = nullptr;
				}
			}
			DXGI_SWAPCHAIN_TYPE* GetDxgiSwapChain()
			{
				return p_swapchain_;
			}
			unsigned int NumBuffer() const
			{
				return num_resource_;
			}
			ID3D12Resource* GetBuffer(unsigned int index)
			{
				if (num_resource_ <= index)
					return nullptr;
				return p_resources_[index];
			}

			unsigned int GetCurrentBufferIndex() const
			{
				return p_swapchain_->GetCurrentBackBufferIndex();
			}

		private:
			DXGI_SWAPCHAIN_TYPE* p_swapchain_ = nullptr;

			ID3D12Resource** p_resources_ = nullptr;
			unsigned int num_resource_ = 0;
		};

		// TODO. 現状はView一つに付きHeap一つを確保している. 最終的にはHeapプールから確保するようにしたい.
		class RenderTargetView
		{
		public:
			RenderTargetView()
			{
			}
			~RenderTargetView()
			{
				Finalize();
			}
			// SwapChainからRTV作成.
			bool Initialize(DeviceDep* p_device, SwapChainDep* p_swapchain, unsigned int buffer_index)
			{
				if (!p_device || !p_swapchain)
					return false;

				{
					D3D12_DESCRIPTOR_HEAP_DESC desc = {};
					desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
					desc.NumDescriptors = 1;
					desc.NodeMask = 0;
					desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

					if (FAILED(p_device->GetD3D12Device()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&p_heap_))))
					{
						std::cout << "ERROR: Create DescriptorHeap" << std::endl;
						return false;
					}
				}
				{
					auto* buffer = p_swapchain->GetBuffer(buffer_index);
					if (!buffer)
					{
						std::cout << "ERROR: Invalid Buffer Index" << std::endl;
						return false;
					}

					auto handle_head = p_heap_->GetCPUDescriptorHandleForHeapStart();
					p_device->GetD3D12Device()->CreateRenderTargetView(buffer, nullptr, handle_head);
				}

				return true;
			}

			void Finalize()
			{
				if (p_heap_)
				{
					p_heap_->Release();
					p_heap_ = nullptr;
				}
			}

			D3D12_CPU_DESCRIPTOR_HANDLE GetD3D12DescriptorHandle() const
			{
				return p_heap_->GetCPUDescriptorHandleForHeapStart();
			}

		private:
			ID3D12DescriptorHeap* p_heap_ = nullptr;
		};

	}
}