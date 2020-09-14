
#include "rhi.d3d12.h"

namespace ngl
{
	namespace rhi
	{
		D3D12_RESOURCE_STATES ConvertResourceState(ngl::rhi::ResourceState v)
		{
			D3D12_RESOURCE_STATES ret = {};
			switch (v)
			{
			case ResourceState::Common:
			{
				ret = D3D12_RESOURCE_STATE_COMMON;
				break;
			}
			case ResourceState::General:
			{
				ret = D3D12_RESOURCE_STATE_GENERIC_READ;
				break;
			}
			case ResourceState::ConstantBuffer:
			{
				ret = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
				break;
			}
			case ResourceState::VertexBuffer:
			{
				ret = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
				break;
			}
			case ResourceState::IndexBuffer:
			{
				ret = D3D12_RESOURCE_STATE_INDEX_BUFFER;
				break;
			}
			case ResourceState::RenderTarget:
			{
				ret = D3D12_RESOURCE_STATE_RENDER_TARGET;
				break;
			}
			case ResourceState::ShaderRead:
			{
				ret = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				break;
			}
			case ResourceState::UnorderedAccess:
			{
				ret = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
				break;
			}
			case ResourceState::DepthWrite:
			{
				ret = D3D12_RESOURCE_STATE_DEPTH_WRITE;
				break;
			}
			case ResourceState::DepthRead:
			{
				ret = D3D12_RESOURCE_STATE_DEPTH_READ;
				break;
			}
			case ResourceState::IndirectArgument:
			{
				ret = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
				break;
			}
			case ResourceState::CopyDst:
			{
				ret = D3D12_RESOURCE_STATE_COPY_DEST;
				break;
			}
			case ResourceState::CopySrc:
			{
				ret = D3D12_RESOURCE_STATE_COPY_SOURCE;
				break;
			}
			case ResourceState::Present:
			{
				ret = D3D12_RESOURCE_STATE_PRESENT;
				break;
			}
			default:
			{
				std::cout << "ERROR : Invalid Resource State" << std::endl;
			}
			}
			return ret;
		}

		// -------------------------------------------------------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------------------------------------------------------
		DeviceDep::DeviceDep()
		{
		}
		DeviceDep::~DeviceDep()
		{
			Finalize();
		}

		bool DeviceDep::Initialize(ngl::platform::CoreWindow* window, bool enable_debug_layer)
		{
			if (!window)
				return false;

			p_window_ = window;

#if 1
			// DebugLayer有効化
			if (enable_debug_layer)
			{
				ID3D12Debug* debugController;
				if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
				{
					debugController->EnableDebugLayer();
					debugController->Release();
				}
			}
#endif

			{
#if 1
				if (enable_debug_layer)
				{
					// デバッグ情報有効Factory
					if (FAILED(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&p_factory_))))
					{
						// DXGIファクトリ生成失敗.
						std::cout << "ERROR: Create DXGIFactory" << std::endl;
						return false;
					}
				}
				else
#endif
				{
					// リリース版用Factory
					if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&p_factory_))))
					{
						// DXGIファクトリ生成失敗.
						std::cout << "ERROR: Create DXGIFactory" << std::endl;
						return false;
					}
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
		void DeviceDep::Finalize()
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

		ngl::platform::CoreWindow* DeviceDep::GetWindow()
		{
			return p_window_;
		}

		ID3D12Device* DeviceDep::GetD3D12Device()
		{
			return p_device_;
		}
		DeviceDep::DXGI_FACTORY_TYPE* DeviceDep::GetDxgiFactory()
		{
			return p_factory_;
		}
		// -------------------------------------------------------------------------------------------------------------------------------------------------


		// -------------------------------------------------------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------------------------------------------------------
		SwapChainDep::SwapChainDep()
		{
		}
		SwapChainDep::~SwapChainDep()
		{
			Finalize();
		}
		bool SwapChainDep::Initialize(DeviceDep* p_device, GraphicsCommandQueueDep* p_graphics_command_queu, const Desc& desc)
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
		void SwapChainDep::Finalize()
		{
			if (p_resources_)
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
		SwapChainDep::DXGI_SWAPCHAIN_TYPE* SwapChainDep::GetDxgiSwapChain()
		{
			return p_swapchain_;
		}
		unsigned int SwapChainDep::NumResource() const
		{
			return num_resource_;
		}
		ID3D12Resource* SwapChainDep::GetD3D12Resource(unsigned int index)
		{
			if (num_resource_ <= index)
				return nullptr;
			return p_resources_[index];
		}

		unsigned int SwapChainDep::GetCurrentBufferIndex() const
		{
			return p_swapchain_->GetCurrentBackBufferIndex();
		}
		// -------------------------------------------------------------------------------------------------------------------------------------------------


		// -------------------------------------------------------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------------------------------------------------------
		GraphicsCommandQueueDep::GraphicsCommandQueueDep()
		{
		}
		GraphicsCommandQueueDep::~GraphicsCommandQueueDep()
		{
			Finalize();
		}

		// TODO. ここでCommandQueue生成時に IGIESW .exe found in whitelist: NO というメッセージがVSログに出力される. 意味と副作用は現状不明.
		bool GraphicsCommandQueueDep::Initialize(DeviceDep* p_device)
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

		void GraphicsCommandQueueDep::Finalize()
		{
			if (p_command_queue_)
			{
				p_command_queue_->Release();
				p_command_queue_ = nullptr;
			}
		}

		void GraphicsCommandQueueDep::ExecuteCommandLists(unsigned int num_command_list, GraphicsCommandListDep** p_command_lists)
		{
			// 一時バッファに詰める
			p_command_list_array_.clear();
			for (auto i = 0u; i < num_command_list; ++i)
			{
				p_command_list_array_.push_back(p_command_lists[i]->GetD3D12GraphicsCommandList());
			}

			p_command_queue_->ExecuteCommandLists(num_command_list, &(p_command_list_array_[0]));
		}

		void GraphicsCommandQueueDep::Signal(FenceDep* p_fence, ngl::types::u64 fence_value)
		{
			if (!p_fence)
				return;
			p_command_queue_->Signal( p_fence->GetD3D12Fence(), fence_value);
		}

		ID3D12CommandQueue* GraphicsCommandQueueDep::GetD3D12CommandQueue()
		{
			return p_command_queue_;
		}
		// -------------------------------------------------------------------------------------------------------------------------------------------------



		// -------------------------------------------------------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------------------------------------------------------
		GraphicsCommandListDep::GraphicsCommandListDep()
		{
		}
		GraphicsCommandListDep::~GraphicsCommandListDep()
		{
			Finalize();
		}
		bool GraphicsCommandListDep::Initialize(DeviceDep* p_device)
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

			// 初回クローズ. これがないと初回フレームの開始時ResetでComError発生.
			p_command_list_->Close();

			return true;
		}
		void GraphicsCommandListDep::Finalize()
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

		void GraphicsCommandListDep::Begin()
		{
			// アロケータリセット
			p_command_allocator_->Reset();
			// コマンドリストリセット
			p_command_list_->Reset(p_command_allocator_, nullptr);
		}
		void GraphicsCommandListDep::End()
		{
			p_command_list_->Close();
		}

		void GraphicsCommandListDep::SetRenderTargetSingle(RenderTargetViewDep* p_rtv)
		{
			auto rtv = p_rtv->GetD3D12DescriptorHandle();
			p_command_list_->OMSetRenderTargets(1, &rtv, true, nullptr);
		}
		void GraphicsCommandListDep::ClearRenderTarget(RenderTargetViewDep* p_rtv, float(color)[4])
		{
			auto rtv = p_rtv->GetD3D12DescriptorHandle();
			p_command_list_->ClearRenderTargetView(rtv, color, 0u, nullptr);
		}

		// バリア
		void GraphicsCommandListDep::ResourceBarrier(SwapChainDep* p_swapchain, unsigned int buffer_index, ResourceState prev, ResourceState next)
		{
			if (!p_swapchain)
				return;
			if (prev == next)
				return;
			auto* resource = p_swapchain->GetD3D12Resource(buffer_index);

			D3D12_RESOURCE_STATES state_before = ConvertResourceState(prev);
			D3D12_RESOURCE_STATES state_after = ConvertResourceState(next);

			D3D12_RESOURCE_BARRIER desc = {};
			desc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			desc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

			desc.Transition.pResource = resource;
			desc.Transition.StateBefore = state_before;
			desc.Transition.StateAfter = state_after;
			// 現状は全サブリソースを対象.
			desc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

			p_command_list_->ResourceBarrier(1, &desc);
		}
		// -------------------------------------------------------------------------------------------------------------------------------------------------


		// -------------------------------------------------------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------------------------------------------------------
		RenderTargetViewDep::RenderTargetViewDep()
		{
		}
		RenderTargetViewDep::~RenderTargetViewDep()
		{
			Finalize();
		}
		// SwapChainからRTV作成.
		bool RenderTargetViewDep::Initialize(DeviceDep* p_device, SwapChainDep* p_swapchain, unsigned int buffer_index)
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
				auto* buffer = p_swapchain->GetD3D12Resource(buffer_index);
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

		void RenderTargetViewDep::Finalize()
		{
			if (p_heap_)
			{
				p_heap_->Release();
				p_heap_ = nullptr;
			}
		}

		D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetViewDep::GetD3D12DescriptorHandle() const
		{
			return p_heap_->GetCPUDescriptorHandleForHeapStart();
		}
		// -------------------------------------------------------------------------------------------------------------------------------------------------


		// -------------------------------------------------------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------------------------------------------------------
		FenceDep::FenceDep()
		{
		}
		FenceDep::~FenceDep()
		{
			Finalize();
		}

		bool FenceDep::Initialize(DeviceDep* p_device)
		{
			if (!p_device)
				return false;

			if (FAILED(p_device->GetD3D12Device()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&p_fence_))))
			{
				std::cout << "ERROR: Create Fence" << std::endl;
				return false;
			}

			return true;
		}
		void FenceDep::Finalize()
		{
			if (p_fence_)
			{
				p_fence_->Release();
				p_fence_ = nullptr;
			}
		}
		ID3D12Fence* FenceDep::GetD3D12Fence()
		{
			return p_fence_;
		}
		// -------------------------------------------------------------------------------------------------------------------------------------------------


		// -------------------------------------------------------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------------------------------------------------------
		WaitOnFenceSignalDep::WaitOnFenceSignalDep()
		{
			// イベント作成
			win_event_ = CreateEvent(nullptr, false, false, nullptr);
		}
		WaitOnFenceSignalDep::~WaitOnFenceSignalDep()
		{
			// イベント破棄
			CloseHandle(win_event_);
		}

		// 発行したSignalによるFence値が指定した値になるまで待機. プラットフォーム毎に異なる実装. WindowsではEventを利用.
		void WaitOnFenceSignalDep::Wait(FenceDep* p_fence, ngl::types::u64 complete_fence_value)
		{
			if (!p_fence)
				return;

			if (p_fence->GetD3D12Fence()->GetCompletedValue() != complete_fence_value)
			{
				// Fence値とイベントを関連付け
				p_fence->GetD3D12Fence()->SetEventOnCompletion(complete_fence_value, win_event_);

				// イベント発火待機
				WaitForSingleObject(win_event_, INFINITE);
			}
		}
		// -------------------------------------------------------------------------------------------------------------------------------------------------


		// -------------------------------------------------------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------------------------------------------------------
		BufferDep::BufferDep()
		{
		}
		BufferDep::~BufferDep()
		{
			Finalize();
		}

		bool BufferDep::Initialize(DeviceDep* p_device, const Desc& desc)
		{
			if (!p_device)
				return false;

			desc_ = desc;

			if (0 >= desc_.element_byte_size || 0 >= desc_.element_count)
				return false;

			D3D12_HEAP_FLAGS heap_flag = D3D12_HEAP_FLAG_NONE;
			D3D12_RESOURCE_STATES initial_state = ConvertResourceState(desc_.initial_state);
			D3D12_HEAP_PROPERTIES heap_prop = {};
			{
				switch (desc_.heap_type)
				{
				case ResourceHeapType::Default:
				{
					heap_prop.Type = D3D12_HEAP_TYPE_DEFAULT;
					break;
				}
				case ResourceHeapType::Upload:
				{
					heap_prop.Type = D3D12_HEAP_TYPE_UPLOAD;
					break;
				}
				case ResourceHeapType::Readback:
				{
					heap_prop.Type = D3D12_HEAP_TYPE_READBACK;
					break;
				}
				default:
				{
					heap_prop.Type = D3D12_HEAP_TYPE_DEFAULT;
				}
				}
				heap_prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
				heap_prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
				heap_prop.VisibleNodeMask = 0;
			}

			D3D12_RESOURCE_DESC resource_desc = {};
			{
				resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
				resource_desc.Alignment = 0;
				resource_desc.Width = static_cast<UINT64>(desc_.element_byte_size) * static_cast<UINT64>(desc_.element_count);
				resource_desc.Height = 1u;
				resource_desc.DepthOrArraySize = 1u;
				resource_desc.MipLevels = 1u;
				resource_desc.Format = DXGI_FORMAT_UNKNOWN;
				resource_desc.SampleDesc.Count = 1;
				resource_desc.SampleDesc.Quality = 0;
				resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
				resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
			}

			/*
				ステート制限
					Upload -> D3D12_RESOURCE_STATE_GENERIC_READ
					Readback -> D3D12_RESOURCE_STATE_COPY_DEST
					テクスチャ配置専用予約バッファ -> D3D12_RESOURCE_STATE_COMMON
			*/
			if (D3D12_HEAP_TYPE_UPLOAD == heap_prop.Type)
			{
				if (D3D12_RESOURCE_STATE_GENERIC_READ != initial_state)
				{
					std::cout << "ERROR: State of Upload Buffer must be ResourceState::General" << std::endl;
					return false;
				}
			}
			else if (D3D12_HEAP_TYPE_READBACK == heap_prop.Type)
			{
				if (D3D12_RESOURCE_STATE_COPY_DEST != initial_state)
				{
					std::cout << "ERROR: State of Readback Buffer must be ResourceState::CopyDst" << std::endl;
					return false;
				}
			}

			if (FAILED(p_device->GetD3D12Device()->CreateCommittedResource(&heap_prop, heap_flag, &resource_desc, initial_state, nullptr, IID_PPV_ARGS(&resource_))))
			{
				std::cout << "ERROR: CreateCommittedResource" << std::endl;
				return false;
			}

			return true;
		}
		void BufferDep::Finalize()
		{
			if (resource_)
			{
				resource_->Release();
				resource_ = nullptr;
			}
		}
		void BufferDep::Unmap()
		{
			if (!map_ptr_)
				return;

			// Uploadバッファ以外の場合はUnmap時に書き戻さないのでZero-Range指定.
			D3D12_RANGE write_range = {0, 0};
			if (ResourceHeapType::Upload == desc_.heap_type)
			{
				write_range = { 0, static_cast<SIZE_T>(desc_.element_byte_size) * static_cast<SIZE_T>(desc_.element_count) };
			}

			resource_->Unmap(0, &write_range);
			map_ptr_ = nullptr;
		}
		ID3D12Resource* BufferDep::GetD3D12Resource()
		{
			return resource_;
		}
		// -------------------------------------------------------------------------------------------------------------------------------------------------
	}
}