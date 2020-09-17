#pragma once

#include <iostream>
#include <vector>

#include "ngl/util/types.h"
#include "ngl/platform/win/window.win.h"

#include <d3d12.h>
#if 1
#include <dxgi1_6.h>
#else
#include <dxgi1_4.h>
#endif


#include <atlbase.h>


namespace ngl
{
	namespace rhi
	{
		class SwapChainDep;
		class RenderTargetViewDep;


		class GraphicsCommandListDep;
		class GraphicsCommandQueueDep;

		class FenceDep;

		class DeviceDep;


		enum class ResourceState
		{
			Common,
			General,
			ConstantBuffer,
			VertexBuffer,
			IndexBuffer,
			RenderTarget,
			ShaderRead,
			UnorderedAccess,
			DepthWrite,
			DepthRead,
			IndirectArgument,
			CopyDst,
			CopySrc,
			Present,
		};
		D3D12_RESOURCE_STATES ConvertResourceState(ngl::rhi::ResourceState v);


		enum class ResourceHeapType
		{
			Default,
			Upload,
			Readback,
		};

		enum class ShaderStage
		{
			Vertex,
			Pixel,
			Mesh,			// 未実装
			Amplification,	// 未実装
		};


		// Device
		class DeviceDep
		{
		public:
			using DXGI_FACTORY_TYPE = IDXGIFactory6;

			DeviceDep();
			~DeviceDep();

			bool Initialize(ngl::platform::CoreWindow* window, bool enable_debug_layer);
			void Finalize();

			ngl::platform::CoreWindow* GetWindow();

			ID3D12Device* GetD3D12Device();
			DXGI_FACTORY_TYPE* GetDxgiFactory();

		private:
			ngl::platform::CoreWindow* p_window_ = nullptr;

			D3D_FEATURE_LEVEL device_feature_level_ = {};
			CComPtr<ID3D12Device> p_device_;
			CComPtr<DXGI_FACTORY_TYPE> p_factory_;
		};


		// Graphics Command Queue
		class GraphicsCommandQueueDep
		{
		public:
			GraphicsCommandQueueDep();
			~GraphicsCommandQueueDep();

			// TODO. ここでCommandQueue生成時に IGIESW .exe found in whitelist: NO というメッセージがVSログに出力される. 意味と副作用は現状不明.
			bool Initialize(DeviceDep* p_device);
			void Finalize();

			void ExecuteCommandLists(unsigned int num_command_list, GraphicsCommandListDep** p_command_lists);
			void Signal(FenceDep* p_fence, ngl::types::u64 fence_value);


			ID3D12CommandQueue* GetD3D12CommandQueue();
		private:
			CComPtr<ID3D12CommandQueue> p_command_queue_;
			std::vector<ID3D12CommandList*> p_command_list_array_;
		};


		// CommandList
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

			void SetRenderTargetSingle(RenderTargetViewDep* p_rtv);
			void ClearRenderTarget(RenderTargetViewDep* p_rtv, float(color)[4]);

			void ResourceBarrier(SwapChainDep* p_swapchain, unsigned int buffer_index, ResourceState prev, ResourceState next);
		private:
			CComPtr<ID3D12CommandAllocator>		p_command_allocator_;
			CComPtr<ID3D12GraphicsCommandList>	p_command_list_;
		};

		// フェンス
		class FenceDep
		{
		public:
			FenceDep();
			~FenceDep();

			bool Initialize(DeviceDep* p_device);
			void Finalize();

			ID3D12Fence* GetD3D12Fence();
		private:
			CComPtr<ID3D12Fence> p_fence_;
		};


		// CommandQueue実行完了待機用オブジェクト.
		class WaitOnFenceSignalDep
		{
		public:
			WaitOnFenceSignalDep();
			~WaitOnFenceSignalDep();

			// 発行したSignalによるFence値が指定した値になるまで待機. プラットフォーム毎に異なる実装. WindowsではEventを利用.
			void Wait(FenceDep* p_fence, ngl::types::u64 complete_fence_value);

		private:
			HANDLE win_event_;
		};


		// スワップチェイン
		class SwapChainDep
		{
		public:
			using DXGI_SWAPCHAIN_TYPE = IDXGISwapChain4;

			struct Desc
			{
				DXGI_FORMAT		format = {};
				unsigned int	buffer_count = 2;
			};

			SwapChainDep();
			~SwapChainDep();
			bool Initialize(DeviceDep* p_device, GraphicsCommandQueueDep* p_graphics_command_queu, const Desc& desc);
			void Finalize();

			unsigned int GetCurrentBufferIndex() const;

			DXGI_SWAPCHAIN_TYPE* GetDxgiSwapChain();
			unsigned int NumResource() const;
			ID3D12Resource* GetD3D12Resource(unsigned int index);

		private:
			CComPtr<DXGI_SWAPCHAIN_TYPE> p_swapchain_;

			CComPtr<ID3D12Resource>* p_resources_ = nullptr;
			unsigned int num_resource_ = 0;
		};


		// RenderTargetView
		// TODO. 現状はView一つに付きHeap一つを確保している. 最終的にはHeapプールから確保するようにしたい.
		class RenderTargetViewDep
		{
		public:
			RenderTargetViewDep();
			~RenderTargetViewDep();

			// SwapChainからRTV作成.
			bool Initialize(DeviceDep* p_device, SwapChainDep* p_swapchain, unsigned int buffer_index);
			void Finalize();

			D3D12_CPU_DESCRIPTOR_HANDLE GetD3D12DescriptorHandle() const;
		private:
			CComPtr<ID3D12DescriptorHeap> p_heap_;
		};

		// Buffer
		class BufferDep
		{
		public:
			struct Desc
			{
				ngl::u32			element_byte_size = 0;
				ngl::u32			element_count = 0;
				ResourceHeapType	heap_type = ResourceHeapType::Default;
				ResourceState		initial_state = ResourceState::General;
			};

			BufferDep();
			~BufferDep();

			bool Initialize(DeviceDep* p_device, const Desc& desc);
			void Finalize();

			template<typename T = void>
			T* Map()
			{
				// DefaultヒープリソースはMap不可
				if (ResourceHeapType::Default == desc_.heap_type)
				{
					std::cout << "ERROR: Default Buffer can not Mapped" << std::endl;
					return nullptr;
				}
				if (map_ptr_)
				{
					// Map済みの場合はそのまま返す.
					return reinterpret_cast<T*>(map_ptr_);
				}

				// Readbackバッファ以外の場合はMap時に以前のデータを読み取らないようにZero-Range指定.
				D3D12_RANGE read_range = { 0, 0 };
				if (ResourceHeapType::Readback == desc_.heap_type)
				{
					read_range = { 0, static_cast<SIZE_T>(desc_.element_byte_size) * static_cast<SIZE_T>(desc_.element_count) };
				}

				if (FAILED(resource_->Map(0, &read_range, &map_ptr_)))
				{
					std::cout << "ERROR: Resouce Map" << std::endl;
					map_ptr_ = nullptr;
					return nullptr;
				}
				return reinterpret_cast<T*>(map_ptr_);
			}

			void Unmap();

			ID3D12Resource* GetD3D12Resource();

		private:
			Desc	desc_ = {};

			void*		map_ptr_ = nullptr;

			CComPtr<ID3D12Resource> resource_;
		};



		// D3D12では単純にシェーダバイナリを保持するだけ
		class ShaderDep
		{
		public:
			ShaderDep();
			virtual ~ShaderDep();

			// コンパイル済みシェーダバイナリから初期化
			bool Initialize(DeviceDep* p_device, const void* shader_binary_ptr, u32 shader_binary_size);

			struct Desc
			{
				// シェーダファイルパス.
				// "./sample.hlsl"
				const wchar_t*	shader_file_path = nullptr;
				// エントリポイント名. 
				// "main_ps"
				const char*		entry_point_name = nullptr;
				// シェーダステージ.
				ShaderStage		stage = ShaderStage::Vertex;
				// シェーダモデル文字列.
				// "4_0", "5_0", "5_1" etc.
				const char*		shader_model_version = nullptr;

				// コンパイルオプション設定.
				bool			option_debug_mode = false;
				bool			option_enable_validation = false;
				bool			option_enable_optimization = false;
				bool			option_matrix_row_major = false;

				// TODO. インクルードやDefineオプションなど

			};
			// ファイルから
			bool Initialize(DeviceDep* p_device, const Desc& desc);


			void Finalize();
		private:
			std::vector<u8>	data_;
		};

	}
}