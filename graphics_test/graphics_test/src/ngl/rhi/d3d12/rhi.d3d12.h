#pragma once

#include <iostream>
#include <vector>

#include "ngl/rhi/rhi.h"

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
				ResourceHeapType	heap_type = ResourceHeapType::DEFAULT;
				ResourceState		initial_state = ResourceState::GENERAL;
			};

			BufferDep();
			~BufferDep();

			bool Initialize(DeviceDep* p_device, const Desc& desc);
			void Finalize();

			template<typename T = void>
			T* Map()
			{
				// DefaultヒープリソースはMap不可
				if (ResourceHeapType::DEFAULT == desc_.heap_type)
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
				if (ResourceHeapType::READBACK == desc_.heap_type)
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
				ShaderStage		stage = ShaderStage::VERTEX;
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

			u32		GetShaderBinarySize() const;
			const void*	GetShaderBinaryPtr() const;
		private:
			std::vector<u8>	data_;
		};

		/*
			ShaderReflectionによって取得した情報

			note: 現時点ではDXCでコンパイルしたシェーダの定数バッファデフォルト値が取得できない(ShaderModel5以下の場合はD3DCompilerによるコンパイルをしてデフォルト値も取得できる)
		*/
		class ShaderReflectionDep
		{
		public:
			struct CbInfo
			{
				// Cb名
				char	name[64];
				// cb Slot Index
				u16		index;
				// cb Byte Size
				u16		size;
				u16		num_member;
			};
			struct CbVariableInfo
			{
				// CbVariable名
				char	name[64];
				// Constant Buffer内のByte Size
				u16		offset;
				// Constant Buffer内のByte Offset
				u16		size;
				// 外部のデフォルト値バッファ内の対応する位置へのオフセット.
				u16		default_value_offset;
			};

			struct InputParamInfo
			{
				char	semantic_name[32];
				u8		semantic_index;

				// if float3 then 3. if int4 then 4.
				u8		num_component;
			};

			ShaderReflectionDep();
			~ShaderReflectionDep();

			bool Initialize(DeviceDep* p_device, ShaderDep* p_shader);
			void Finalize();

			const CbInfo* GetCbInfo(u32 index);
			const CbVariableInfo* GetCbVariableInfo(u32 index, u32 variable_index);

			template<typename T>
			bool GetCbDefaultValue(u32 index, u32 variable_index, T& out)
			{
				if (cb_.size() <= index || cb_[index].num_member <= variable_index)
					return false;
				const auto cb_var_offset = cb_variable_offset_[index] + variable_index;
				if (cb_variable_[cb_var_offset].size != sizeof(T))
					return false;

				const auto* src = &cb_default_value_buffer_[cb_variable_[cb_var_offset].default_value_offset];
				std::memcpy(&out, src, cb_variable_[cb_var_offset].size );
				return true;
			}


		private:
			// Constant Buffer
			// cb
			std::vector<CbInfo> cb_;
			// cb index to start index on variable_.
			std::vector<u32> cb_variable_offset_;
			// variable array.
			std::vector<CbVariableInfo> cb_variable_;
			// デフォルト値バッファ.
			std::vector<u8> cb_default_value_buffer_;

			// Input Output
			std::vector<InputParamInfo>	input_param_;
		};


		class GraphicsPipelineState
		{
		public:
			struct Desc
			{
				ShaderDep*	vs = nullptr;
				ShaderDep*	ps = nullptr;
				ShaderDep*	ds = nullptr;
				ShaderDep*	hs = nullptr;
				ShaderDep*	gs = nullptr;

				BlendState			blend_state = {};
				u32					sample_mask = ~(u32(0));
				RasterizerState		rasterizer_staet = {};
				DepthStencilState	depth_stencil_state = {};

				InputLayout				input_layout = {};
				PrimitiveTopologyType	primitive_topology_type = PrimitiveTopologyType::PRIMITIVE_TOPOLOGY_TRIANGLE;

				u32					num_render_targets = 0;
				ResourceFormat		render_target_formats[8] = {};
				ResourceFormat		depth_stencil_format = ResourceFormat::NGL_FORMAT_D24_UNORM_S8_UINT;
				SampleDesc			sample_desc = {};
				u32					node_mask = 0;
			};

			GraphicsPipelineState();
			~GraphicsPipelineState();

			bool Initialize(DeviceDep* p_device, const Desc& desc);
			void Finalize();

		private:
			CComPtr<ID3D12PipelineState>	pso_;
		};
	}
}