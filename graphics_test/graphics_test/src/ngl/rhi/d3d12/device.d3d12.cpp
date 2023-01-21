
#include "device.d3d12.h"
#include "command_list.d3d12.h"

#include <array>
#include <algorithm>

// for wchar convert.
#include <stdlib.h>


#ifdef _DEBUG
#include <system_error>
#endif

// lib
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")


// for compiler
#include <d3dcompiler.h>
// lib
#pragma comment(lib, "d3dcompiler.lib")

#include <dxcapi.h>
#pragma comment(lib, "dxcompiler.lib")


namespace
{

	// wchar_t to char.
	// ビルドを通すために _CRT_SECURE_NO_WARNINGS が必要.
	int mbs_to_wcs(wchar_t* dst, int dst_len, const char* src)
	{
		size_t cnt = 0;
		mbstowcs_s(&cnt, dst, dst_len, src, dst_len);
		return static_cast<int>(cnt);
	}
	// char to wchar_t.
	// ビルドを通すために _CRT_SECURE_NO_WARNINGS が必要.
	int wcs_to_mb(char* dst, int dst_len, const wchar_t* src)
	{
		size_t cnt = 0;
		wcstombs_s(&cnt, dst, dst_len, src, dst_len);
		return static_cast<int>(cnt);
	}

}

namespace ngl
{
	namespace rhi
	{
		static uint8_t MaskCount(uint8_t V)
		{
			static const uint8_t Count[16] = 
			{
			  0, 1, 1, 2,
			  1, 2, 2, 3,
			  1, 2, 2, 3,
			  2, 3, 3, 4
			};
			if(0 <= V && V <= 0xF)
				return Count[V];
			return 0;
		}

		static D3D12_SHADER_VISIBILITY ConvertShaderVisibility(ShaderStage v)
		{
			switch (v)
			{
			case ShaderStage::Vertex:
				return D3D12_SHADER_VISIBILITY_VERTEX;

			case ShaderStage::Hull:
				return D3D12_SHADER_VISIBILITY_HULL;

			case ShaderStage::Domain:
				return D3D12_SHADER_VISIBILITY_DOMAIN;

			case ShaderStage::Geometry:
				return D3D12_SHADER_VISIBILITY_GEOMETRY;

			case ShaderStage::Pixel:
				return D3D12_SHADER_VISIBILITY_PIXEL;

			case ShaderStage::Compute:
				return D3D12_SHADER_VISIBILITY_ALL;

			default:
				return D3D12_SHADER_VISIBILITY_ALL;
			}
		}

		namespace helper
		{
			bool SerializeAndCreateRootSignature(DeviceDep* p_device, const D3D12_ROOT_SIGNATURE_DESC& desc, CComPtr<ID3D12RootSignature>& out_root_signature)
			{
				auto device = p_device->GetD3D12Device();
				CComPtr<ID3DBlob> blob;
				CComPtr<ID3DBlob> error;

				if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error)))
				{
					std::cout << "[ERROR] SerializeRootSignature" << std::endl;
					std::wcout << static_cast<wchar_t*>(error->GetBufferPointer()) << std::endl;
					assert(false);
					return false;
				}
				if (FAILED(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&out_root_signature))))
				{
					std::cout << "[ERROR] CreateRootSignature" << std::endl;
					assert(false);
					return false;
				}
				return true;
			}

			bool SerializeAndCreateRootSignature(CComPtr<ID3D12RootSignature>& out_root_signature, DeviceDep* p_device, D3D12_ROOT_PARAMETER* p_param_array, uint32_t num_param, D3D12_ROOT_SIGNATURE_FLAGS flag)
			{
				D3D12_ROOT_SIGNATURE_DESC root_signature_desc = {};
				root_signature_desc.NumParameters = num_param;
				root_signature_desc.pParameters = p_param_array;
				root_signature_desc.Flags = flag; // Global (Not Local).
				return SerializeAndCreateRootSignature(p_device, root_signature_desc, out_root_signature);
			}
			bool SerializeAndCreateLocalRootSignature(CComPtr<ID3D12RootSignature>& out_root_signature, DeviceDep* p_device, D3D12_ROOT_PARAMETER* p_param_array, uint32_t num_param, D3D12_ROOT_SIGNATURE_FLAGS flag)
			{
				D3D12_ROOT_SIGNATURE_DESC root_signature_desc = {};
				root_signature_desc.NumParameters = num_param;
				root_signature_desc.pParameters = p_param_array;
				root_signature_desc.Flags = flag | D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE; // Local.
				return SerializeAndCreateRootSignature(p_device, root_signature_desc, out_root_signature);
			}
		}



		// -------------------------------------------------------------------------------------------------------------------------------------------------
		IDevice* RhiObjectBase::GetParentDeviceInreface()
		{
			return p_parent_device_;
		}
		const IDevice* RhiObjectBase::GetParentDeviceInreface() const
		{
			return p_parent_device_;
		}
		DeviceDep* RhiObjectBase::GetParentDevice()
		{
			return p_parent_device_;
		}
		const DeviceDep* RhiObjectBase::GetParentDevice() const
		{
			return p_parent_device_;
		}
		void RhiObjectBase::InitializeRhiObject(DeviceDep* p_device)
		{
			p_parent_device_ = p_device;
		}
		// -------------------------------------------------------------------------------------------------------------------------------------------------



		// -------------------------------------------------------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------------------------------------------------------
		DeviceDep::DeviceDep()
		{
		}
		DeviceDep::~DeviceDep()
		{
			Finalize();
		}

		bool DeviceDep::Initialize(ngl::platform::CoreWindow* window, const Desc& desc)
		{
			if (!window)
				return false;

			desc_ = desc;

			p_window_ = window;

#if 1
			// DebugLayer有効化
			if (desc_.enable_debug_layer)
			{
				CComPtr<ID3D12Debug> debugController;
				if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
				{
					debugController->EnableDebugLayer();
				}
			}
#endif

			{
#if 1
				if (desc_.enable_debug_layer)
				{
					// デバッグ情報有効Factory
					if (FAILED(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&p_factory_))))
					{
						// DXGIファクトリ生成失敗.
						std::cout << "[ERROR] Create DXGIFactory" << std::endl;
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
						std::cout << "[ERROR] Create DXGIFactory" << std::endl;
						return false;
					}
				}
			}
			// TODO. アダプタ検索する場合はここでFactoryから.



			D3D_FEATURE_LEVEL feature_levels[] =
			{
				D3D_FEATURE_LEVEL_12_1,
				D3D_FEATURE_LEVEL_12_0,
				D3D_FEATURE_LEVEL_11_1,
				D3D_FEATURE_LEVEL_11_0,
			};

			device_feature_level_ = {};
			device_dxr_tier_ = D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
			p_device5_ = {};

			{
				bool end_create_device = false;

				// Find the HW adapter
				CComPtr<IDXGIAdapter1> pAdapter;
				for (uint32_t i = 0; !end_create_device && DXGI_ERROR_NOT_FOUND != p_factory_->EnumAdapters1(i, &pAdapter); i++)
				{
					DXGI_ADAPTER_DESC1 desc;
					pAdapter->GetDesc1(&desc);

					// Skip SW adapters
					if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;


					for (auto l : feature_levels)
					{
						if (SUCCEEDED(D3D12CreateDevice(pAdapter, l, IID_PPV_ARGS(&p_device_))))
						{
							D3D12_FEATURE_DATA_D3D12_OPTIONS5 features5;
							HRESULT hr = p_device_->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features5, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5));
							if (SUCCEEDED(hr) && features5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
							{
								// DXR対応ならDeviceInterfaceの問い合わせ.
								if (FAILED(p_device_->QueryInterface(IID_PPV_ARGS(&p_device5_))))
								{
									std::cout << "[ERROR] Failed QueryInterface for ID3D12Device5" << std::endl;
									continue;
								}

								// 目的のDeviceが生成できれば完了.
								device_dxr_tier_ = features5.RaytracingTier;
								device_feature_level_ = l;
								end_create_device = true;

								break;
							}
						}
					}
				}
			}

			// PersistentDescriptorManager初期化
			{
				p_persistent_descriptor_allocator_.reset(new PersistentDescriptorAllocator());
				PersistentDescriptorAllocator::Desc pda_desc = {};
				pda_desc.type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
				pda_desc.allocate_descriptor_count_ = desc_.persistent_descriptor_size;

				if (!p_persistent_descriptor_allocator_->Initialize(this, pda_desc))
				{
					std::cout << "[ERROR] Create PersistentDescriptorAllocator" << std::endl;
					return false;
				}
			}

			// Sampler用PersistentDescriptorManager初期化
			{
				p_persistent_sampler_descriptor_allocator_.reset(new PersistentDescriptorAllocator());
				PersistentDescriptorAllocator::Desc pda_desc = {};
				pda_desc.type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;// Sampler用Heap
				// SamplerのDescriptorHeapは2048個分までの制限がある. Samplerはパラメータの組み合わせがほぼ決まっているので使い切ることは無いはず.
				// また,RootSignature固定化のため無効なレジスタがある場合に設定するダミーDescriptorを一つ確保するため実際に利用できる最大数は -1.
				// と思ったがHeapの確保自体は2048より多くできるようだ. 実際にDescriptorを作成した際に一つのHeap内に2048個までしか同時に存在できないということか?.
				pda_desc.allocate_descriptor_count_ = 2048;
				if (!p_persistent_sampler_descriptor_allocator_->Initialize(this, pda_desc))
				{
					std::cout << "[ERROR] Create PersistentDescriptorAllocator" << std::endl;
					return false;
				}
			}

#if NGL_DYNAMIC_DESCRIPTOR_MANAGER_REPLACE
			// DynamicDescriptorManager初期化
			{
				p_dynamic_descriptor_manager_.reset(new DynamicDescriptorManager());
				DynamicDescriptorManager::Desc fdm_desc = {};
				fdm_desc.allocate_descriptor_count_ = desc_.frame_descriptor_size;
				fdm_desc.type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

				if (!p_dynamic_descriptor_manager_->Initialize(this, fdm_desc))
				{
					std::cout << "[ERROR] Create DynamicDescriptorManager" << std::endl;
					return false;
				}
			}
#else
			// FrameDescriptorManager初期化
			{
				p_frame_descriptor_manager_.reset(new FrameDescriptorManager());
				FrameDescriptorManager::Desc fdm_desc = {};
				fdm_desc.allocate_descriptor_count_ = desc_.frame_descriptor_size;
				fdm_desc.type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

				if (!p_frame_descriptor_manager_->Initialize(this, fdm_desc))
				{
					std::cout << "[ERROR] Create FrameDescriptorManager" << std::endl;
					return false;
				}
			}
#endif

			// FrameDescriptorHeapPagePool初期化
			{
				p_frame_descriptor_page_pool_.reset(new FrameDescriptorHeapPagePool());
				if (!p_frame_descriptor_page_pool_->Initialize(this))
				{
					std::cout << "[ERROR] Create FrameDescriptorHeapPagePool" << std::endl;
					return false;
				}
			}

			// Gabage Collector.
			{
				if (!gb_.Initialize())
				{
					std::cout << "[ERROR] Initialize RHI GabageCollector" << std::endl;
					return false;
				}
			}

			return true;
		}
		void DeviceDep::Finalize()
		{
			gb_.Finalize();

			p_device_ = nullptr;
			p_factory_ = nullptr;
		}

		void DeviceDep::ReadyToNewFrame()
		{
			++frame_index_;

			buffer_index_ = (buffer_index_ + 1) % desc_.swapchain_buffer_count;

#if NGL_DYNAMIC_DESCRIPTOR_MANAGER_REPLACE
			p_dynamic_descriptor_manager_->ReadyToNewFrame((u32)frame_index_);
#else
			// Frame用にCommandListが確保したGlobal Descriptor領域をフレームフリップインデックスで纏めて解放する.
			// これにより各CommandListが個別に解放する必要がなくフレーム毎に確保することに専念できる.
			// ただしこのインデックスのものはここで自動解放されるため, FrameDescriptorAllocInterfaceを使って直接IDを指定して確保した際のIDがフレームインデックスとかぶると意図せず解放されて破綻するので注意.
			p_frame_descriptor_manager_->ResetFrameDescriptor(buffer_index_);
#endif

			gb_.ReadyToNewFrame();
		}

		// 破棄待ちオブジェクトの処理.
		void DeviceDep::ExecuteFrameGabageCollect()
		{
			// ガベコレ. 実際にはここを別スレッドに逃がすことも考えられる.
			gb_.Execute();
		}

		// 派生Deviceクラスで実装.
		void DeviceDep::DestroyRhiObject(IRhiObject* p)
		{
			gb_.Enqueue(p);
		}


		ngl::platform::CoreWindow* DeviceDep::GetWindow()
		{
			return p_window_;
		}

		ID3D12Device* DeviceDep::GetD3D12Device()
		{
			return p_device_;
		}
		ID3D12Device5* DeviceDep::GetD3D12DeviceForDxr()
		{
			return p_device5_;
		}
		DeviceDep::DXGI_FACTORY_TYPE* DeviceDep::GetDxgiFactory()
		{
			return p_factory_;
		}
		bool DeviceDep::IsSupportDxr() const
		{
			return D3D12_RAYTRACING_TIER_NOT_SUPPORTED != device_dxr_tier_;
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

			const auto device_desc = p_device->GetDesc();

			DXGI_SWAP_CHAIN_DESC1 obj_desc = {};
			obj_desc.Width = screen_w;
			obj_desc.Height = screen_h;
			obj_desc.Format = ConvertResourceFormat(desc.format);
			obj_desc.Stereo = false;
			obj_desc.SampleDesc.Count = 1;
			obj_desc.SampleDesc.Quality = 0;
			obj_desc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
			obj_desc.BufferCount = device_desc.swapchain_buffer_count;

			obj_desc.Scaling = DXGI_SCALING_STRETCH;
			obj_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			obj_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

			obj_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;


			// 一時オブジェクトでSwapchain生成
			IDXGISwapChain1* p_tmp_swap;
			if (FAILED(p_device->GetDxgiFactory()->CreateSwapChainForHwnd(p_graphics_command_queu->GetD3D12CommandQueue(), hwnd, &obj_desc, nullptr, nullptr, &p_tmp_swap)))
			{
				std::cout << "[ERROR] Create Command Queue" << std::endl;
				return false;
			}

			// QueryInterfaceでIDXGISwapChain4要求
			if (FAILED(p_tmp_swap->QueryInterface(IID_PPV_ARGS(&p_swapchain_))))
			{
				return false;
			}
			// 一時オブジェクト破棄
			p_tmp_swap->Release();


			// Resource取得
			num_resource_ = device_desc.swapchain_buffer_count;
			p_resources_ = new CComPtr<ID3D12Resource> [num_resource_];
			for (auto i = 0u; i < num_resource_; ++i)
			{
				if (FAILED(p_swapchain_->GetBuffer(i, IID_PPV_ARGS(&p_resources_[i]))))
				{
					std::cout << "[ERROR] Get SwapChain Buffer " << i << std::endl;
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
					p_resources_[i] = nullptr;
				}
				delete[] p_resources_;
				p_resources_ = nullptr;
			}
			p_swapchain_ = nullptr;
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
				std::cout << "[ERROR] Create Command Queue" << std::endl;
				return false;
			}

			return true;
		}

		void GraphicsCommandQueueDep::Finalize()
		{
			p_command_queue_ = nullptr;
		}

		void GraphicsCommandQueueDep::ExecuteCommandLists(unsigned int num_command_list, GraphicsCommandListDep** p_command_lists)
		{
			// 一時バッファに詰める
			p_command_list_array_.clear();
			for (auto i = 0u; i < num_command_list; ++i)
			{
				p_command_list_array_.push_back(p_command_lists[i]->GetD3D12GraphicsCommandList());
			}
			try
			{
				p_command_queue_->ExecuteCommandLists(num_command_list, &(p_command_list_array_[0]));
			}
			catch (...)
			{
				// D3D12で稀に発生するcom_errorをキャッチ
				std::cout << "[ngl][GraphicsCommandQueueDep] ExecuteCommandLists: catch exception." << std::endl;
				OutputDebugString(_T("[ngl][GraphicsCommandQueueDep] ExecuteCommandLists: catch exception."));
			}

			p_command_list_array_.clear();
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
				std::cout << "[ERROR] Create Fence" << std::endl;
				return false;
			}

			return true;
		}
		void FenceDep::Finalize()
		{
			p_fence_ = nullptr;
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
		ShaderDep::ShaderDep()
		{
		}
		ShaderDep::~ShaderDep()
		{
			Finalize();
		}

		// コンパイル済みシェーダバイナリから初期化
		bool ShaderDep::Initialize(DeviceDep* p_device, ShaderStage stage, const void* shader_binary_ptr, u32 shader_binary_size)
		{
			if (!p_device)
				return false;
			if (!shader_binary_ptr)
				return false;

			// 内部でメモリ確保
			data_.resize(shader_binary_size);
			memcpy(data_.data(), shader_binary_ptr, shader_binary_size);
			// ステージ保存.
			stage_ = stage;

			return true;
		}
		// ファイルからコンパイル.
		bool ShaderDep::Initialize(DeviceDep* p_device, const InitFileDesc& desc)
		{
			auto include_object = D3D_COMPILE_STANDARD_FILE_INCLUDE;

			auto stage_id = static_cast<int>(desc.stage);

			// DXRのShaderLibの場合はコンパイル時にentry_pointを指定しないためentry_point_nameはチェックしない.
			if (!p_device || !desc.shader_file_path || !desc.shader_model_version)
			{
				return false;
			}

			// 処理用のwchar化.
			constexpr int k_len_wstr_len = 256;
			wchar_t shader_file_path_ws[k_len_wstr_len];
			assert(k_len_wstr_len > strlen(desc.shader_file_path));
			mbs_to_wcs(shader_file_path_ws, (int)std::size(shader_file_path_ws), desc.shader_file_path);

			// シェーダステージ名_シェーダモデル名 の文字列を生成.
			char shader_model_name[32];
			{
				static const char* shader_stage_names[] =
				{
					"vs",	// Vertex.
					"hs",	// Hull.
					"ds",	// Domain.
					"gs",	// Geometry.
					"ps",	// Pixel.

					"cs",	// Compute.

					"lib",	// Shader Library (for DXR).
				};
				static_assert(static_cast<int>(ShaderStage::_Max) == std::size(shader_stage_names),"Shader Stage Name Array Size is Invalid");

				size_t shader_model_name_len = 0;

				const auto shader_model_base_len = strlen(shader_stage_names[stage_id]);
				const auto shader_model_version_len = strlen(desc.shader_model_version);
				if (sizeof(shader_model_name) <= (shader_model_base_len + 1 + shader_model_version_len + 1))
				{
					return false;
				}

				
				std::copy_n(shader_stage_names[stage_id], shader_model_base_len, shader_model_name);
				shader_model_name_len += shader_model_base_len;

				std::copy_n("_", 1, shader_model_name + shader_model_name_len);
				shader_model_name_len += 1;

				std::copy_n(desc.shader_model_version, shader_model_version_len, shader_model_name + shader_model_name_len);
				shader_model_name_len += shader_model_version_len;

				shader_model_name[shader_model_name_len] = 0;
				shader_model_name_len += 1;
			}


			bool result = true;

			// 先にdxcによるコンパイルを試みる.
			{
				bool compile_success = true;
				// shader model profile name.  char -> wchar
				wchar_t shader_model_name_w[64];
				mbs_to_wcs(shader_model_name_w, (int)std::size(shader_model_name_w), shader_model_name);

				wchar_t shader_entry_point_name_w[128] = L"\0";
				if (desc.entry_point_name)
				{
					mbs_to_wcs(shader_entry_point_name_w, (int)std::size(shader_entry_point_name_w), desc.entry_point_name);
				}

				CComPtr<IDxcLibrary> dxc_library;
				HRESULT hr = DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&dxc_library));
				if (FAILED(hr))
					compile_success &= false;

				CComPtr<IDxcCompiler> dxc_compiler;
				if (compile_success)
				{
					hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxc_compiler));
					if (FAILED(hr))
						compile_success &= false;
				}

				CComPtr<IDxcBlobEncoding> sourceBlob;
				if (compile_success)
				{
					uint32_t codePage = CP_UTF8;
					hr = dxc_library->CreateBlobFromFile(shader_file_path_ws, &codePage, &sourceBlob);
					if (FAILED(hr))
					{
#ifdef _DEBUG
						std::cout << "[ERROR] " << std::system_category().message(hr) << " " << desc.shader_file_path << std::endl;
#endif
						compile_success &= false;
					}
				}

				CComPtr<IDxcOperationResult> dxc_result;
				if (compile_success)
				{
					hr = dxc_compiler->Compile(
						sourceBlob,
						shader_file_path_ws,
						shader_entry_point_name_w,
						shader_model_name_w,	// "PS_6_0"
						NULL, 0,				// pArguments, argCount
						NULL, 0,				// pDefines, defineCount
						NULL,					// pIncludeHandler
						&dxc_result				// ppResult
					);

					if (SUCCEEDED(hr))
						dxc_result->GetStatus(&hr);
					if (FAILED(hr))
					{
						if (dxc_result)
						{
							CComPtr<IDxcBlobEncoding> errorsBlob;
							hr = dxc_result->GetErrorBuffer(&errorsBlob);
							if (SUCCEEDED(hr) && errorsBlob)
							{
								// FXCで失敗した場合は後段でDXCによるコンパイルを試みる.
								wprintf(L"[ERROR] Compilation failed with errors:\n%hs\n",
									(const char*)errorsBlob->GetBufferPointer());
							}
						}

						compile_success &= false;
					}
				}
				if (compile_success)
				{
					// 成功
					CComPtr<IDxcBlob> code;
					dxc_result->GetResult(&code);
					compile_success = Initialize(p_device, desc.stage, code->GetBufferPointer(), (u32)code->GetBufferSize());
				}

				result = compile_success;
			}

			// dxcでのコンパイルに失敗した場合はd3dcompilerでのコンパイルを試みる
			if (!result)
			{
				bool compile_success = true;

				const int flag_debug = (!desc.option_debug_mode) ? 0 : D3DCOMPILE_DEBUG;
				const int flag_validation = (!desc.option_enable_validation) ? D3DCOMPILE_SKIP_VALIDATION : 0;
				const int flag_optimization = (!desc.option_enable_optimization) ? D3DCOMPILE_SKIP_OPTIMIZATION : 0;
				const int flag_matrix_row_major = (!desc.option_matrix_row_major) ? D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR : D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

				// フラグ.
				const int commpile_flag = flag_debug | flag_validation | flag_optimization | flag_matrix_row_major;

				CComPtr<ID3DBlob> p_compile_data;
				CComPtr<ID3DBlob>  error_blob;
				auto hr = D3DCompileFromFile(shader_file_path_ws, nullptr, include_object, desc.entry_point_name, shader_model_name, commpile_flag, 0, &p_compile_data, &error_blob);
				if (FAILED(hr))
				{
					// 失敗.
					if (HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) == hr)
					{
						// ファイルパス不正
						std::cout << "[ERROR] Shader File not found" << std::endl;
					}
					if (error_blob)
					{
						std::string error_message;
						error_message.resize(error_blob->GetBufferSize());
						std::copy_n(static_cast<char*>(error_blob->GetBufferPointer()), error_blob->GetBufferSize(), error_message.begin());

						std::cout << "[ERROR] " << error_message << std::endl;
					}

					compile_success = false;
				}
				if (compile_success)
				{
					compile_success = Initialize(p_device, desc.stage, p_compile_data->GetBufferPointer(), (u32)p_compile_data->GetBufferSize());
				}

				result = compile_success;
			}

			return result;
		}
		void ShaderDep::Finalize()
		{
			if (0 < data_.size())
			{
				std::vector<u8> temp{};
				data_.swap(temp);
			}
		}
		u32		ShaderDep::GetShaderBinarySize() const
		{
			return static_cast<u32>(data_.size());
		}
		const void* ShaderDep::GetShaderBinaryPtr() const
		{
			if(0 < GetShaderBinarySize())
				return reinterpret_cast<const void*>(data_.data());
			return nullptr;
		}
		// -------------------------------------------------------------------------------------------------------------------------------------------------


		ShaderReflectionDep::ShaderReflectionDep()
		{
		}
		ShaderReflectionDep::~ShaderReflectionDep()
		{
			Finalize();
		}


		bool ShaderReflectionDep::Initialize(DeviceDep* p_device, const ShaderDep* p_shader)
		{
			if (!p_device || !p_shader || !p_shader->GetShaderBinaryPtr())
				return true;

			const auto bin_size = p_shader->GetShaderBinarySize();
			const auto bin_ptr = p_shader->GetShaderBinaryPtr();


			// ShaderReflectionかLibraryReflectionのどちらか.
			CComPtr<ID3D12ShaderReflection> shader_reflect;
			CComPtr<ID3D12LibraryReflection> lib_reflect;
			{
				bool hresult = true;

				// 最初にDxcApiを試行する
				// ShaderModel6以降はDxcAPIを利用する
				CComPtr<IDxcLibrary> lib;
				hresult = SUCCEEDED(DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&lib)));

				CComPtr<IDxcBlobEncoding> binBlob{};
				if (hresult)
				{
					hresult = SUCCEEDED(lib->CreateBlobWithEncodingOnHeapCopy(bin_ptr, bin_size, CP_ACP, &binBlob));
				}
				CComPtr<IDxcContainerReflection> refl;
				if (hresult)
				{
					hresult = SUCCEEDED(DxcCreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(&refl)));
				}
				UINT shdIndex = 0;
				if (hresult)
				{
					// シェーダーバイナリデータをロードし、DXILチャンクブロック（のインデックス）を得る.
					hresult = SUCCEEDED(refl->Load(binBlob));
					if (hresult)
					{
#ifndef MAKEFOURCC
#define MAKEFOURCC(a, b, c, d) (unsigned int)((unsigned char)(a) | (unsigned char)(b) << 8 | (unsigned char)(c) << 16 | (unsigned char)(d) << 24)
#endif
						hresult = SUCCEEDED(refl->FindFirstPartKind(MAKEFOURCC('D', 'X', 'I', 'L'), &shdIndex));
#undef MAKEFOURCC
					}
				}

				// リフレクションインターフェース取得.
				if (hresult)
				{
					// ShaderReflectionをDxcAPIで取得を試行.
					hresult = SUCCEEDED(refl->GetPartReflection(shdIndex, IID_PPV_ARGS(&shader_reflect)));

					// ShaderReflection取得に失敗した場合はLibraryReflectionの取得を試みる.
					// 先にLibraryReflectionの取得をするとlibプロファイルではない場合も取得できてしまうらしいのでShaderReflection取得に失敗した場合に試行する.
					if (!hresult)
					{
						hresult = SUCCEEDED(refl->GetPartReflection(shdIndex, IID_PPV_ARGS(&lib_reflect)));
					}
				}
				if (!hresult)
				{
					// DxcApiで取得できなかった場合はD3DRefrectを試行.
					// (ShaderModel5以前はD3DReflectを利用する
					hresult = SUCCEEDED(D3DReflect(bin_ptr, bin_size, IID_PPV_ARGS(&shader_reflect)));
				}

				if (!hresult)
				{
					assert(hresult);
					// ShaderでもLibraryでもなく取得に失敗.
					return false;
				}
			}

#if 0
			// LibraryReflectionのテスト(DXR).
			if (lib_reflect.p)
			{
				// プロファイル違いでもGetDescできる上にメモリアクセス違反が発生するらしいのでunitonでメモリ範囲確保して安全にする.
				// https://blog.techlab-xe.net/dxc-shader-reflection/
				union {
					D3D12_SHADER_DESC shader_desc;
					D3D12_LIBRARY_DESC lib_desc;
				} LibraryReflectionDesc;

				// DXR等のLibプロファイルから情報取得.
				if (FAILED(lib_reflect->GetDesc(&LibraryReflectionDesc.lib_desc)))
				{
					assert(false);
					return false;
				}
				for (u32 fi = 0; fi < LibraryReflectionDesc.lib_desc.FunctionCount; ++fi)
				{
					// raygeneration や closesthit 等の関数毎に情報取得できる.
					auto p_function = lib_reflect->GetFunctionByIndex(fi);

					D3D12_FUNCTION_DESC func_desc = {};
					if (SUCCEEDED(p_function->GetDesc(&func_desc)))
					{
						// ここで取得できる関数名は先頭に 謎のコードが挿入されていたり終端が無かったりと正しくない( 2022/11/06 )
						std::cout << "ShaderLibFUnc: " << func_desc.Name << std::endl;
					}
				}
			}
#endif

			// ----------------------------------------------------------------
			// ID3D12ShaderReflectionが取得できればそこから情報取得
			if (shader_reflect.p)
			{
				D3D12_SHADER_DESC shader_desc = {};
				bool hresult = SUCCEEDED(shader_reflect->GetDesc(&shader_desc));
				if (hresult)
				{
					// Constant Buffer
					{
						cb_.resize(shader_desc.ConstantBuffers);
						cb_variable_offset_.resize(shader_desc.ConstantBuffers);
						// 事前に必要なVariable数を計算
						int num_var_total = 0;
						int size_default_var_total = 0;
						for (auto i = 0u; i < cb_.size(); ++i)
						{
							cb_[i] = {};
							cb_variable_offset_[i] = 0;
							if (auto* cb_info = shader_reflect->GetConstantBufferByIndex(i))
							{
								D3D12_SHADER_BUFFER_DESC cb_desc;
								if (SUCCEEDED(cb_info->GetDesc(&cb_desc)))
								{
									cb_[i].index = i;
									cb_[i].size = cb_desc.Size;
									cb_[i].num_member = cb_desc.Variables;
									size_t copy_name_len = std::min<>(std::strlen(cb_desc.Name), sizeof(cb_[i].name));
									std::copy_n(cb_desc.Name, copy_name_len, cb_[i].name);

									cb_variable_offset_[i] = num_var_total;
									num_var_total += cb_[i].num_member;

									size_default_var_total += cb_[i].size;
								}
							}
						}

						// variable数分確保
						cb_variable_.resize(num_var_total);
						// デフォルト値バッファ確保
						cb_default_value_buffer_.resize(size_default_var_total);

						// 改めてVariable情報を取得.
						int variable_index = 0;
						int default_value_buffer_offset = 0;
						for (auto i = 0u; i < cb_.size(); ++i)
						{
							if (auto* cb_info = shader_reflect->GetConstantBufferByIndex(i))
							{
								D3D12_SHADER_BUFFER_DESC cb_desc;
								if (SUCCEEDED(cb_info->GetDesc(&cb_desc)))
								{
									// Variables
									for (auto mi = 0u; mi < cb_desc.Variables; ++mi)
									{
										if (auto* cb_member = cb_info->GetVariableByIndex(mi))
										{
											D3D12_SHADER_VARIABLE_DESC var_desc;
											if (SUCCEEDED(cb_member->GetDesc(&var_desc)))
											{
												// ConstantBuffer内変数のオフセット
												cb_variable_[variable_index].offset = var_desc.StartOffset;
												cb_variable_[variable_index].size = var_desc.Size;
												size_t copy_name_len = std::min<>(std::strlen(var_desc.Name), sizeof(cb_variable_[variable_index].name));
												std::copy_n(var_desc.Name, copy_name_len, cb_variable_[variable_index].name);

												// デフォルト値バッファ内の開始オフセット保存
												cb_variable_[variable_index].default_value_offset = default_value_buffer_offset;
												// オフセットすすめる
												default_value_buffer_offset += cb_variable_[variable_index].size;

												// デフォルト値を確保したバッファに詰めていく.
												{
													u8* dst_ptr = &cb_default_value_buffer_[cb_variable_[variable_index].default_value_offset];
													const auto size = cb_variable_[variable_index].size;
													if (var_desc.DefaultValue)
													{
														// デフォルト値があればコピー
														const u8* src_ptr = reinterpret_cast<const u8*>(var_desc.DefaultValue);
														std::copy(src_ptr, src_ptr + size, dst_ptr);
													}
													else
													{
														// デフォルト値がなければゼロセット
														std::memset(dst_ptr, 0x00, size);
													}
												}
											}
										}

										++variable_index;
									}
								}
							}
						}
					}

					// Input Output
					{
						std::cout << "Shader Input Param" << std::endl;
						std::cout << "	Num Param = " << shader_desc.InputParameters << std::endl;
						input_param_.resize(shader_desc.InputParameters);
						for (auto i = 0u; i < shader_desc.InputParameters; ++i)
						{
							auto& tar = input_param_[i];

							std::cout << "	Param " << i << std::endl;
							D3D12_SIGNATURE_PARAMETER_DESC input_desc;
							if (SUCCEEDED(shader_reflect->GetInputParameterDesc(i, &input_desc)))
							{
								std::cout << "		SemanticName = " << input_desc.SemanticName << std::endl;
								std::cout << "		SemanticIndex = " << input_desc.SemanticIndex << std::endl;
								std::cout << "		ComponentType = " << input_desc.ComponentType << std::endl;

								tar.semantic_index = input_desc.SemanticIndex;
								size_t copy_name_len = std::min<>(std::strlen(input_desc.SemanticName), sizeof(tar.semantic_name));
								std::copy_n(input_desc.SemanticName, copy_name_len, tar.semantic_name);

								tar.num_component = MaskCount(input_desc.Mask);
							}
						}
					}

					// リソーススロット一覧作成.
					{
						int valid_slot_count = 0;
						resource_slot_.resize(shader_desc.BoundResources);
						for (u32 i = 0; i < shader_desc.BoundResources; i++)
						{
							D3D12_SHADER_INPUT_BIND_DESC bd;
							shader_reflect->GetResourceBindingDesc(i, &bd);

							// リソース種別取得
							RootParameterType paramType = RootParameterType::_Max;
							switch (bd.Type)
							{
							case D3D_SHADER_INPUT_TYPE::D3D_SIT_CBUFFER:
								paramType = RootParameterType::ConstantBuffer; break;
							case D3D_SHADER_INPUT_TYPE::D3D_SIT_SAMPLER:
								paramType = RootParameterType::Sampler; break;
							case D3D_SHADER_INPUT_TYPE::D3D_SIT_TEXTURE:
							case D3D_SHADER_INPUT_TYPE::D3D_SIT_STRUCTURED:
							case D3D_SHADER_INPUT_TYPE::D3D_SIT_BYTEADDRESS:
								paramType = RootParameterType::ShaderResource; break;
							case D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_RWTYPED:
							case D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_RWSTRUCTURED:
							case D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_RWBYTEADDRESS:
							case D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
							case D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_APPEND_STRUCTURED:
							case D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_CONSUME_STRUCTURED:
								paramType = RootParameterType::UnorderedAccess; break;
							default:
								std::cout << "[ERROR] ShaderReflection. Failed to Get BoundResources " << i << " ." << std::endl;
								break;;
							}

							if (RootParameterType::_Max != paramType)
							{
								resource_slot_[valid_slot_count].type = paramType;
								resource_slot_[valid_slot_count].bind_point = bd.BindPoint;
								resource_slot_[valid_slot_count].name.Set(bd.Name, static_cast<unsigned int>(std::strlen(bd.Name)));

								++valid_slot_count;
							}
						}
						// 取得できなかったリソースがあった場合に切り詰める.
						resource_slot_.resize(valid_slot_count);
					}
				}
			}

			return true;
		}
		void ShaderReflectionDep::Finalize()
		{
		}

		u32 ShaderReflectionDep::NumInputParamInfo() const
		{
			return static_cast<u32>(input_param_.size());
		}
		const ShaderReflectionDep::InputParamInfo* ShaderReflectionDep::GetInputParamInfo(u32 index) const
		{
			if (input_param_.size() <= index)
				return nullptr;
			return &input_param_[index];
		}
		u32 ShaderReflectionDep::NumCbInfo() const
		{
			return static_cast<u32>(cb_.size());
		}
		const ShaderReflectionDep::CbInfo* ShaderReflectionDep::GetCbInfo(u32 index) const
		{
			if (cb_.size() <= index)
				return nullptr;
			return &cb_[index];
		}
		const ShaderReflectionDep::CbVariableInfo* ShaderReflectionDep::GetCbVariableInfo(u32 index, u32 variable_index) const
		{
			auto* cb = GetCbInfo(index);
			if (!cb || cb->num_member <= variable_index)
				return nullptr;
			const auto i = cb_variable_offset_[index] + variable_index;
			return &cb_variable_[i];
		}
		u32 ShaderReflectionDep::NumResourceSlotInfo() const
		{
			return static_cast<u32>(resource_slot_.size());
		}
		const ShaderReflectionDep::ResourceSlotInfo* ShaderReflectionDep::GetResourceSlotInfo(u32 index) const
		{
			if (resource_slot_.size() <= index)
				return nullptr;
			return &resource_slot_[index];
		}
		// -------------------------------------------------------------------------------------------------------------------------------------------------


		// -------------------------------------------------------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------------------------------------------------------
		PipelineResourceViewLayoutDep::PipelineResourceViewLayoutDep()
		{
		}
		PipelineResourceViewLayoutDep::~PipelineResourceViewLayoutDep()
		{
			Finalize();
		}

		bool PipelineResourceViewLayoutDep::Initialize(DeviceDep* p_device, const Desc& desc)
		{
			if (!p_device)
				return false;


			// Layout情報取得
			auto func_setup_slot = [](ShaderStage stage, DeviceDep* p_device, const ShaderReflectionDep& p_reflection, std::unordered_map<ResourceViewName, Slot>& slot_map)
			{
				auto SetRegisterIndex = [](Slot& slot, u32 bind_point, RootParameterType type, ShaderStage shader_stage)
				{
					switch (shader_stage)
					{
					case ShaderStage::Vertex:   slot.vs_stage.slot = bind_point; slot.vs_stage.type = type; break;
					case ShaderStage::Pixel:    slot.ps_stage.slot = bind_point; slot.ps_stage.type = type; break;
					case ShaderStage::Geometry: slot.gs_stage.slot = bind_point; slot.gs_stage.type = type; break;
					case ShaderStage::Hull:     slot.hs_stage.slot = bind_point; slot.hs_stage.type = type; break;
					case ShaderStage::Domain:   slot.ds_stage.slot = bind_point; slot.ds_stage.type = type; break;
					case ShaderStage::Compute:  slot.cs_stage.slot = bind_point; slot.cs_stage.type = type; break;
					}
				};

				const auto num_slot = p_reflection.NumResourceSlotInfo();
				for (auto i = 0u; i < num_slot; ++i)
				{
					if (const auto* slot_info = p_reflection.GetResourceSlotInfo(i))
					{
						auto itr = slot_map.find(slot_info->name);
						if (itr != slot_map.end())
						{
							// 登録済み
							SetRegisterIndex(itr->second, slot_info->bind_point, slot_info->type, stage);
						}
						else
						{
							// 未登録
							Slot new_slot;
							SetRegisterIndex(new_slot, slot_info->bind_point, slot_info->type, stage);
							slot_map[slot_info->name] = new_slot;
						}
					}
				}

				return true;
			};
			{
				if (desc.vs)
				{
					// vsの入力レイアウト情報が必要なのでvsのreflectionはメンバ変数に保持
					if( !vs_reflection_.Initialize(p_device, desc.vs) || !func_setup_slot(ShaderStage::Vertex, p_device, vs_reflection_, slot_map_))
						return false;
				}
				if (desc.hs)
				{
					ShaderReflectionDep		reflection;
					if (!reflection.Initialize(p_device, desc.hs) || !func_setup_slot(ShaderStage::Hull, p_device, reflection, slot_map_))
						return false;
				}
				if (desc.ds)
				{
					ShaderReflectionDep		reflection;
					if (!reflection.Initialize(p_device, desc.ds) || !func_setup_slot(ShaderStage::Domain, p_device, reflection, slot_map_))
						return false;
				}
				if (desc.gs)
				{
					ShaderReflectionDep		reflection;
					if (!reflection.Initialize(p_device, desc.gs) || !func_setup_slot(ShaderStage::Geometry, p_device, reflection, slot_map_))
						return false;
				}
				if (desc.ps)
				{
					ShaderReflectionDep		reflection;
					if (!reflection.Initialize(p_device, desc.ps) || !func_setup_slot(ShaderStage::Pixel, p_device, reflection, slot_map_))
						return false;
				}
				if (desc.cs)
				{
					ShaderReflectionDep		reflection;
					if (!reflection.Initialize(p_device, desc.cs) || !func_setup_slot(ShaderStage::Compute, p_device, reflection, slot_map_))
						return false;
				}
			}

			D3D12_ROOT_SIGNATURE_DESC root_signature_desc = {};
			/*
				RootSignatureのDescriptorTableはもんしょさんのコピー戦略を参考.
				一律で
						Table0 : CBV 16個
						Table1 : SRV 48個
						Table2 : Sampler 16個
						Table3 : UAV 16個

				というレイアウトとし、実行時には適切なサイズのHeapの適切な位置にCopyDescriptorsをする
			*/
			constexpr auto num_shader_stage = static_cast<int>(ShaderStage::_Max);

			const std::array<D3D12_DESCRIPTOR_RANGE, 4> fixed_range_infos =
			{
				{
				{ D3D12_DESCRIPTOR_RANGE_TYPE_CBV, RootParameterTableSize(RootParameterType::ConstantBuffer), 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
				{ D3D12_DESCRIPTOR_RANGE_TYPE_SRV, RootParameterTableSize(RootParameterType::ShaderResource), 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
				{ D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, RootParameterTableSize(RootParameterType::Sampler), 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
				{ D3D12_DESCRIPTOR_RANGE_TYPE_UAV, RootParameterTableSize(RootParameterType::UnorderedAccess), 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
				}
			};
			auto func_set_table_to_param = []
			(D3D12_DESCRIPTOR_RANGE* p_range_array, D3D12_ROOT_PARAMETER* p_param_array, u32 table, ShaderStage stage, const D3D12_DESCRIPTOR_RANGE& descriptor_range)
			{
				p_range_array[table] = descriptor_range;
				p_param_array[table].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				p_param_array[table].DescriptorTable.NumDescriptorRanges = 1;
				p_param_array[table].DescriptorTable.pDescriptorRanges = &p_range_array[table];
				p_param_array[table].ShaderVisibility = ConvertShaderVisibility(stage);
			};

			std::array<D3D12_DESCRIPTOR_RANGE, num_shader_stage* fixed_range_infos.size()> ranges;
			std::array<D3D12_ROOT_PARAMETER, num_shader_stage* fixed_range_infos.size()>  rootParameters;
			{
				// フラグ初期化. 全シェーダステージ無視フラグで初期化しておき, 有効なシェーダステージがあれば無視フラグを除去していく.
				root_signature_desc.Flags =
					D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS
					| D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
					| D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
					| D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
					| D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS
					;

				// 頂点シェーダリフレクションの入力レイアウト情報からフラグ追加.
				if(desc.vs)
				{
					// 頂点入力の有無フラグ追加.
					root_signature_desc.Flags |= (0 < vs_reflection_.NumInputParamInfo()) ? D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT : D3D12_ROOT_SIGNATURE_FLAG_NONE;
				}
				// Descriptor
				int root_table_index = 0;
				{
					// VS
					if (desc.vs)
					{
						resource_table_.vs_cbv_table = root_table_index;
						func_set_table_to_param(ranges.data(), rootParameters.data(), root_table_index, ShaderStage::Vertex, fixed_range_infos[0]);
						++root_table_index;
						resource_table_.vs_srv_table = root_table_index;
						func_set_table_to_param(ranges.data(), rootParameters.data(), root_table_index, ShaderStage::Vertex, fixed_range_infos[1]);
						++root_table_index;
						resource_table_.vs_sampler_table = root_table_index;
						func_set_table_to_param(ranges.data(), rootParameters.data(), root_table_index, ShaderStage::Vertex, fixed_range_infos[2]);
						++root_table_index;
						// VSはUAV無し

						root_signature_desc.Flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;
					}

					// HS
					if (desc.hs)
					{
						resource_table_.hs_cbv_table = root_table_index;
						func_set_table_to_param(ranges.data(), rootParameters.data(), root_table_index, ShaderStage::Hull, fixed_range_infos[0]);
						++root_table_index;
						resource_table_.hs_srv_table = root_table_index;
						func_set_table_to_param(ranges.data(), rootParameters.data(), root_table_index, ShaderStage::Hull, fixed_range_infos[1]);
						++root_table_index;
						resource_table_.hs_sampler_table = root_table_index;
						func_set_table_to_param(ranges.data(), rootParameters.data(), root_table_index, ShaderStage::Hull, fixed_range_infos[2]);
						++root_table_index;
						// HSはUAV無し

						root_signature_desc.Flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
					}

					// DS
					if (desc.ds)
					{
						resource_table_.ds_cbv_table = root_table_index;
						func_set_table_to_param(ranges.data(), rootParameters.data(), root_table_index, ShaderStage::Domain, fixed_range_infos[0]);
						++root_table_index;
						resource_table_.ds_srv_table = root_table_index;
						func_set_table_to_param(ranges.data(), rootParameters.data(), root_table_index, ShaderStage::Domain, fixed_range_infos[1]);
						++root_table_index;
						resource_table_.ds_sampler_table = root_table_index;
						func_set_table_to_param(ranges.data(), rootParameters.data(), root_table_index, ShaderStage::Domain, fixed_range_infos[2]);
						++root_table_index;
						// DSはUAV無し

						root_signature_desc.Flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
					}

					// GS
					if (desc.gs)
					{
						resource_table_.gs_cbv_table = root_table_index;
						func_set_table_to_param(ranges.data(), rootParameters.data(), root_table_index, ShaderStage::Geometry, fixed_range_infos[0]);
						++root_table_index;
						resource_table_.gs_srv_table = root_table_index;
						func_set_table_to_param(ranges.data(), rootParameters.data(), root_table_index, ShaderStage::Geometry, fixed_range_infos[1]);
						++root_table_index;
						resource_table_.gs_sampler_table = root_table_index;
						func_set_table_to_param(ranges.data(), rootParameters.data(), root_table_index, ShaderStage::Geometry, fixed_range_infos[2]);
						++root_table_index;
						// GSはUAV無し

						root_signature_desc.Flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
					}

					// PS
					if (desc.ps)
					{
						resource_table_.ps_cbv_table = root_table_index;
						func_set_table_to_param(ranges.data(), rootParameters.data(), root_table_index, ShaderStage::Pixel, fixed_range_infos[0]);
						++root_table_index;
						resource_table_.ps_srv_table = root_table_index;
						func_set_table_to_param(ranges.data(), rootParameters.data(), root_table_index, ShaderStage::Pixel, fixed_range_infos[1]);
						++root_table_index;
						resource_table_.ps_sampler_table = root_table_index;
						func_set_table_to_param(ranges.data(), rootParameters.data(), root_table_index, ShaderStage::Pixel, fixed_range_infos[2]);
						++root_table_index;
						resource_table_.ps_uav_table = root_table_index;
						func_set_table_to_param(ranges.data(), rootParameters.data(), root_table_index, ShaderStage::Pixel, fixed_range_infos[3]);
						++root_table_index;

						root_signature_desc.Flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
					}

					// CS
					if (desc.cs)
					{
						resource_table_.cs_cbv_table = root_table_index;
						func_set_table_to_param(ranges.data(), rootParameters.data(), root_table_index, ShaderStage::Compute, fixed_range_infos[0]);
						++root_table_index;
						resource_table_.cs_srv_table = root_table_index;
						func_set_table_to_param(ranges.data(), rootParameters.data(), root_table_index, ShaderStage::Compute, fixed_range_infos[1]);
						++root_table_index;
						resource_table_.cs_sampler_table = root_table_index;
						func_set_table_to_param(ranges.data(), rootParameters.data(), root_table_index, ShaderStage::Compute, fixed_range_infos[2]);
						++root_table_index;
						resource_table_.cs_uav_table = root_table_index;
						func_set_table_to_param(ranges.data(), rootParameters.data(), root_table_index, ShaderStage::Compute, fixed_range_infos[3]);
						++root_table_index;

						root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
					}
				}
				root_signature_desc.NumParameters = root_table_index;
				root_signature_desc.pParameters = rootParameters.data();
			}

			helper::SerializeAndCreateRootSignature(p_device, root_signature_desc, root_signature_);

			return true;
		}
		void PipelineResourceViewLayoutDep::Finalize()
		{
			root_signature_ = nullptr;
		}
		// 名前でDescriptorSetへハンドル設定
		void PipelineResourceViewLayoutDep::SetDescriptorHandle(DescriptorSetDep* p_desc_set, const char* name, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle) const
		{
			assert(p_desc_set);

			auto find = slot_map_.find(name);
			if (find != slot_map_.end())
			{
				// VS
				{
					const auto& slot_info = find->second.vs_stage;
					if (0 <= slot_info.slot)
					{
						switch (slot_info.type)
						{
						case RootParameterType::ConstantBuffer:
						{
							p_desc_set->SetVsCbv(slot_info.slot, cpu_handle); break;
						}
						case RootParameterType::ShaderResource:
						{
							p_desc_set->SetVsSrv(slot_info.slot, cpu_handle); break;
						}
						case RootParameterType::UnorderedAccess:
						{
							assert(false); break;
						}
						case RootParameterType::Sampler:
						{
							p_desc_set->SetVsSampler(slot_info.slot, cpu_handle); break;
						}
						default:
							break;
						}
					}
				}
				// HS
				{
					const auto& slot_info = find->second.hs_stage;
					if (0 <= slot_info.slot)
					{
						switch (slot_info.type)
						{
						case RootParameterType::ConstantBuffer:
						{
							p_desc_set->SetHsCbv(slot_info.slot, cpu_handle); break;
						}
						case RootParameterType::ShaderResource:
						{
							p_desc_set->SetHsSrv(slot_info.slot, cpu_handle); break;
						}
						case RootParameterType::UnorderedAccess:
						{
							assert(false); break;
						}
						case RootParameterType::Sampler:
						{
							p_desc_set->SetHsSampler(slot_info.slot, cpu_handle); break;
						}
						default:
							break;
						}
					}
				}
				// DS
				{
					const auto& slot_info = find->second.ds_stage;
					if (0 <= slot_info.slot)
					{
						switch (slot_info.type)
						{
						case RootParameterType::ConstantBuffer:
						{
							p_desc_set->SetDsCbv(slot_info.slot, cpu_handle); break;
						}
						case RootParameterType::ShaderResource:
						{
							p_desc_set->SetDsSrv(slot_info.slot, cpu_handle); break;
						}
						case RootParameterType::UnorderedAccess:
						{
							assert(false); break;
						}
						case RootParameterType::Sampler:
						{
							p_desc_set->SetDsSampler(slot_info.slot, cpu_handle); break;
						}
						default:
							break;
						}
					}
				}
				// GS
				{
					const auto& slot_info = find->second.gs_stage;
					if (0 <= slot_info.slot)
					{
						switch (slot_info.type)
						{
						case RootParameterType::ConstantBuffer:
						{
							p_desc_set->SetGsCbv(slot_info.slot, cpu_handle); break;
						}
						case RootParameterType::ShaderResource:
						{
							p_desc_set->SetGsSrv(slot_info.slot, cpu_handle); break;
						}
						case RootParameterType::UnorderedAccess:
						{
							assert(false); break;
						}
						case RootParameterType::Sampler:
						{
							p_desc_set->SetGsSampler(slot_info.slot, cpu_handle); break;
						}
						default:
							break;
						}
					}
				}
				// PS
				{
					const auto& slot_info = find->second.ps_stage;
					if (0 <= slot_info.slot)
					{
						switch (slot_info.type)
						{
						case RootParameterType::ConstantBuffer:
						{
							p_desc_set->SetPsCbv(slot_info.slot, cpu_handle); break;
						}
						case RootParameterType::ShaderResource:
						{
							p_desc_set->SetPsSrv(slot_info.slot, cpu_handle); break;
						}
						case RootParameterType::UnorderedAccess:
						{
							p_desc_set->SetPsUav(slot_info.slot, cpu_handle); break;
						}
						case RootParameterType::Sampler:
						{
							p_desc_set->SetPsSampler(slot_info.slot, cpu_handle); break;
						}
						default:
							break;
						}
					}
				}
				// CS
				{
					const auto& slot_info = find->second.cs_stage;
					if (0 <= slot_info.slot)
					{
						switch (slot_info.type)
						{
						case RootParameterType::ConstantBuffer:
						{
							p_desc_set->SetCsCbv(slot_info.slot, cpu_handle); break;
						}
						case RootParameterType::ShaderResource:
						{
							p_desc_set->SetCsSrv(slot_info.slot, cpu_handle); break;
						}
						case RootParameterType::UnorderedAccess:
						{
							p_desc_set->SetCsUav(slot_info.slot, cpu_handle); break;
						}
						case RootParameterType::Sampler:
						{
							p_desc_set->SetCsSampler(slot_info.slot, cpu_handle); break;
						}
						default:
							break;
						}
					}
				}
			}
		}

		ID3D12RootSignature* PipelineResourceViewLayoutDep::GetD3D12RootSignature()
		{
			return root_signature_;
		}
		const ID3D12RootSignature* PipelineResourceViewLayoutDep::GetD3D12RootSignature() const
		{
			return root_signature_;
		}
		// -------------------------------------------------------------------------------------------------------------------------------------------------


		// -------------------------------------------------------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------------------------------------------------------
		GraphicsPipelineStateDep::GraphicsPipelineStateDep()
		{
		}
		GraphicsPipelineStateDep::~GraphicsPipelineStateDep()
		{
			Finalize();
		}

		bool GraphicsPipelineStateDep::Initialize(DeviceDep* p_device, const Desc& desc)
		{
			if (!p_device)
				return false;

			// よくやるミスのチェック
#if defined(_DEBUG)
			{
				bool any_writemask_nonzero = false;
				for (auto&& bs_target : desc.blend_state.target_blend_states)
				{
					any_writemask_nonzero |= (bs_target.write_mask != 0);
				}
				if (!any_writemask_nonzero)
				{
					// write_maskの設定忘れで一切描画されないことがよくあるため警告
					std::cout << "[WARNING] Graphics PipelineStateObject : All WriteMask is Zero." << std::endl;
				}
			}
#endif

			// 入力エレメント定義配列を一時的に確保して設定し,そのメモリを指定する.
			std::vector<D3D12_INPUT_ELEMENT_DESC> input_elem_descs;
			input_elem_descs.resize(desc.input_layout.num_elements);

			
			D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
			if (desc.vs)
			{
				pso_desc.VS.BytecodeLength = desc.vs->GetShaderBinarySize();
				pso_desc.VS.pShaderBytecode = desc.vs->GetShaderBinaryPtr();
			}
			if (desc.ps)
			{
				pso_desc.PS.BytecodeLength = desc.ps->GetShaderBinarySize();
				pso_desc.PS.pShaderBytecode = desc.ps->GetShaderBinaryPtr();
			}
			if (desc.ds)
			{
				pso_desc.DS.BytecodeLength = desc.ds->GetShaderBinarySize();
				pso_desc.DS.pShaderBytecode = desc.ds->GetShaderBinaryPtr();
			}
			if (desc.hs)
			{
				pso_desc.HS.BytecodeLength = desc.hs->GetShaderBinarySize();
				pso_desc.HS.pShaderBytecode = desc.hs->GetShaderBinaryPtr();
			}
			if (desc.gs)
			{
				pso_desc.GS.BytecodeLength = desc.gs->GetShaderBinarySize();
				pso_desc.GS.pShaderBytecode = desc.gs->GetShaderBinaryPtr();
			}
			pso_desc.StreamOutput = {};
			// BlendState
			{
				pso_desc.BlendState.AlphaToCoverageEnable = desc.blend_state.alpha_to_coverage_enable;
				pso_desc.BlendState.IndependentBlendEnable = desc.blend_state.independent_blend_enable;
				for (auto i = 0u; i < std::size(pso_desc.BlendState.RenderTarget); ++i)
				{
					const auto& src = desc.blend_state.target_blend_states[i];
					auto& dst = pso_desc.BlendState.RenderTarget[i];

					dst.BlendEnable = src.blend_enable;
					dst.BlendOp = ConvertBlendOp(src.color_op);
					dst.SrcBlend = ConvertBlendFactor(src.src_color_blend);
					dst.DestBlend = ConvertBlendFactor(src.dst_color_blend);

					dst.BlendOpAlpha = ConvertBlendOp(src.alpha_op);
					dst.SrcBlendAlpha = ConvertBlendFactor(src.src_alpha_blend);
					dst.DestBlendAlpha = ConvertBlendFactor(src.dst_alpha_blend);

					dst.RenderTargetWriteMask = (D3D12_COLOR_WRITE_ENABLE_ALL < src.write_mask)? D3D12_COLOR_WRITE_ENABLE_ALL : src.write_mask;

					// LogicOpは現状サポート外.
					dst.LogicOp = {};
					dst.LogicOpEnable = false;
				}
			}

			pso_desc.SampleMask = desc.sample_mask;
			// RasterizerState
			{
				const auto& src = desc.rasterizer_state;
				auto& dst = pso_desc.RasterizerState;

				dst.AntialiasedLineEnable = src.antialiased_line_enable;
				dst.ConservativeRaster = (0 != src.conservative_raster)? D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON : D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

				dst.FillMode = ConvertFillMode(src.fill_mode);
				dst.CullMode = ConvertCullMode(src.cull_mode);
				dst.FrontCounterClockwise = src.front_counter_clockwise;

				dst.DepthBias = src.depth_bias;
				dst.DepthBiasClamp = src.depth_bias_clamp;
				dst.DepthClipEnable = src.depth_clip_enable;
				dst.SlopeScaledDepthBias = src.slope_scale_depth_bias;

				dst.ForcedSampleCount = src.force_sample_count;
				dst.MultisampleEnable = src.multi_sample_enable;
			}

			// DepthStencilState
			{
				const auto& src = desc.depth_stencil_state;
				auto& dst = pso_desc.DepthStencilState;
				{
					dst.BackFace.StencilDepthFailOp = ConvertStencilOp(src.back_face.stencil_depth_fail_op);
					dst.BackFace.StencilFailOp = ConvertStencilOp(src.back_face.stencil_fail_op);
					dst.BackFace.StencilPassOp = ConvertStencilOp(src.back_face.stencil_pass_op);
					dst.BackFace.StencilFunc = ConvertComparisonFunc(src.back_face.stencil_func);
				}
				{
					dst.FrontFace.StencilDepthFailOp = ConvertStencilOp(src.front_face.stencil_depth_fail_op);
					dst.FrontFace.StencilFailOp = ConvertStencilOp(src.front_face.stencil_fail_op);
					dst.FrontFace.StencilPassOp = ConvertStencilOp(src.front_face.stencil_pass_op);
					dst.FrontFace.StencilFunc = ConvertComparisonFunc(src.front_face.stencil_func);
				}

				dst.DepthEnable = src.depth_enable;
				dst.DepthFunc = ConvertComparisonFunc(src.depth_func);
				dst.DepthWriteMask = (src.depth_write_mask)? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
				
				dst.StencilEnable = src.stencil_enable;
				dst.StencilReadMask = src.stencil_read_mask;
				dst.StencilWriteMask = src.stencil_write_mask;
			}

			// InputLayout
			{
				const auto& src = desc.input_layout;
				auto& dst = pso_desc.InputLayout;

				dst.NumElements = src.num_elements;
				for (auto i = 0u; i < src.num_elements; ++i)
				{
					const auto& s_elem = src.p_input_elements[i];
					auto& d_elem = input_elem_descs[i];

					d_elem.SemanticName = s_elem.semantic_name;
					d_elem.SemanticIndex = s_elem.semantic_index;
					d_elem.Format = ConvertResourceFormat(s_elem.format);
					d_elem.InputSlot = s_elem.stream_slot;
					d_elem.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
					d_elem.AlignedByteOffset = s_elem.element_offset;
					d_elem.InstanceDataStepRate = 0;
				}
				// メモリを指定.
				dst.pInputElementDescs = input_elem_descs.data();
			}

			pso_desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

			pso_desc.PrimitiveTopologyType = ConvertPrimitiveTopologyType(desc.primitive_topology_type);

			pso_desc.NumRenderTargets = desc.num_render_targets;
			for (auto i = 0u; i < desc.num_render_targets; ++i)
			{
				pso_desc.RTVFormats[i] = ConvertResourceFormat(desc.render_target_formats[i]);
			}
			pso_desc.DSVFormat = ConvertResourceFormat(desc.depth_stencil_format);

			pso_desc.SampleDesc.Count = desc.sample_desc.count;
			pso_desc.SampleDesc.Quality = desc.sample_desc.quality;

			pso_desc.NodeMask = desc.node_mask;

			pso_desc.CachedPSO.CachedBlobSizeInBytes = 0;
			pso_desc.CachedPSO.pCachedBlob = nullptr;

			pso_desc.Flags = D3D12_PIPELINE_STATE_FLAGS::D3D12_PIPELINE_STATE_FLAG_NONE;

			// RootSignature
			{
				PipelineResourceViewLayoutDep::Desc root_signature_desc = {};
				root_signature_desc.vs = desc.vs;
				root_signature_desc.hs = desc.hs;
				root_signature_desc.ds = desc.ds;
				root_signature_desc.gs = desc.gs;
				root_signature_desc.ps = desc.ps;
				view_layout_.Initialize(p_device, root_signature_desc);

				// RootSignature設定
				pso_desc.pRootSignature = view_layout_.GetD3D12RootSignature();
			}

			if (FAILED(p_device->GetD3D12Device()->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_))))
			{
				std::cout << "[ERROR] CreateGraphicsPipelineState" << std::endl;
				return false;
			}

			return true;
		}
		void GraphicsPipelineStateDep::Finalize()
		{
			pso_ = nullptr;
			view_layout_.Finalize();
		}

		// 名前でDescriptorSetへハンドル設定
		void GraphicsPipelineStateDep::SetDescriptorHandle(DescriptorSetDep* p_desc_set, const char* name, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle) const
		{
			view_layout_.SetDescriptorHandle(p_desc_set, name, cpu_handle);
		}


		ID3D12PipelineState* GraphicsPipelineStateDep::GetD3D12PipelineState()
		{
			return pso_;
		}
		ID3D12RootSignature* GraphicsPipelineStateDep::GetD3D12RootSignature()
		{
			return view_layout_.GetD3D12RootSignature();
		}
		const ID3D12PipelineState* GraphicsPipelineStateDep::GetD3D12PipelineState() const
		{
			return pso_;
		}
		const ID3D12RootSignature* GraphicsPipelineStateDep::GetD3D12RootSignature() const
		{
			return view_layout_.GetD3D12RootSignature();
		}
	}
}