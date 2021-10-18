#pragma once

#include "ngl/rhi/rhi.h"
#include "rhi_util.d3d12.h"


#include <iostream>
#include <vector>
#include <unordered_map>


#include "ngl/util/unique_ptr.h"
#include "ngl/platform/win/window.win.h"

#include "rhi_descriptor.d3d12.h"

/*
	実装メモ

		DeviceDep
			各種リソース用Viewを作成するDescriptorHeapを保持(スレッドセーフに注意)
				CBV SRV UAV共用のHeap
				RTV用のHeap
				DSV用のHeap
				Sampler用のHeap(SamplerはD3D的に上限が低いのでキャッシュして使い回す対応が必要)

*/

namespace ngl
{
	namespace rhi
	{
		class SwapChainDep;
		class RenderTargetViewDep;

		class GraphicsCommandQueueDep;

		class FenceDep;

		class DeviceDep;

		class GraphicsCommandListDep;

		class PersistentDescriptorAllocator;
		struct PersistentDescriptorInfo;
		class FrameDescriptorManager;
		class FrameDescriptorHeapPagePool;
		class DescriptorSetDep;


		// Device
		class DeviceDep
		{
		public:
			using DXGI_FACTORY_TYPE = IDXGIFactory6;

			struct Desc
			{
				// swapchainバッファ数. 根幹のリソースバッファリング数に関わるのでDeviceのDescに設定する.
				u32		swapchain_buffer_count = 3;

				// リソースとペアで生成されるViewを保持するバッファのサイズ
				u32		persistent_descriptor_size	= 500000;
				// フレームで連続Descriptorを確保するためのバッファのサイズ
				u32		frame_descriptor_size		= 500000;
				bool	enable_debug_layer			= false;
			};

			DeviceDep();
			~DeviceDep();

			bool Initialize(ngl::platform::CoreWindow* window, const Desc& desc);
			void Finalize();

			void ReadyToNewFrame();
			// Deviceが管理するグローバルなフレームインデックスを取得.
			u64	 GetDeviceFrameIndex() const { return frame_index_; }


			const Desc& GetDesc() const { return desc_; }
			const u32 GetFrameBufferIndex() const { return buffer_index_; }


			ngl::platform::CoreWindow* GetWindow();

			ID3D12Device* GetD3D12Device();
			DXGI_FACTORY_TYPE* GetDxgiFactory();

			PersistentDescriptorAllocator* GetPersistentDescriptorAllocator()
			{
				return p_persistent_descriptor_allocator_.Get();
			}
			PersistentDescriptorAllocator* GetPersistentSamplerDescriptorAllocator()
			{
				return p_persistent_sampler_descriptor_allocator_.Get();
			}
			FrameDescriptorManager* GetFrameDescriptorManager()
			{
				return p_frame_descriptor_manager_.Get();
			}
			FrameDescriptorHeapPagePool* GetFrameDescriptorHeapPagePool()
			{
				return p_frame_descriptor_page_pool_.Get();
			}
		private:
			Desc	desc_ = {};

			ngl::platform::CoreWindow* p_window_ = nullptr;

			D3D_FEATURE_LEVEL device_feature_level_ = {};
			CComPtr<ID3D12Device> p_device_;
			CComPtr<DXGI_FACTORY_TYPE> p_factory_;

			// テスト中. フレームインデックス処理用.
			u64 frame_index_ = 0;

			u32	buffer_index_ = 0;

			// リソース用Descriptor確保用( CBV, SRV, UAV 用)
			ngl::UniquePtr <PersistentDescriptorAllocator>	p_persistent_descriptor_allocator_;

			// 生成したSampler用Descriptor確保用( Sampler 用)
			ngl::UniquePtr <PersistentDescriptorAllocator>	p_persistent_sampler_descriptor_allocator_;


			// フレームでのDescriptor確保用( CBV, SRV, UAV 用). 巨大な単一Heap管理. Samplerのハンドル数制限には対応していないが通常のリソースにはこちらのほうが効率的と思われる.
			ngl::UniquePtr<FrameDescriptorManager>			p_frame_descriptor_manager_;

			// フレームでのDescriptor確保用. こちらはPage単位で拡張していく. CBV,SRV,UAVおよびSamplerすべてで利用可能.
			ngl::UniquePtr<FrameDescriptorHeapPagePool>		p_frame_descriptor_page_pool_;

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
				ResourceFormat		format = ResourceFormat::Format_R10G10B10A2_UNORM;
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

			struct ResourceSlotInfo
			{
				ResourceViewName			name;
				RootParameterType			type = RootParameterType::_Max;
				s32							bind_point = -1;

			};

			ShaderReflectionDep();
			~ShaderReflectionDep();

			bool Initialize(DeviceDep* p_device, const ShaderDep* p_shader);
			void Finalize();


			u32 NumInputParamInfo() const;
			const InputParamInfo* GetInputParamInfo(u32 index) const;

			u32 NumCbInfo() const;
			const CbInfo* GetCbInfo(u32 index) const;
			const CbVariableInfo* GetCbVariableInfo(u32 index, u32 variable_index) const;

			template<typename T>
			bool GetCbDefaultValue(u32 index, u32 variable_index, T& out) const
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

			u32 NumResourceSlotInfo() const;
			const ResourceSlotInfo* GetResourceSlotInfo(u32 index) const;


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

			std::vector<ResourceSlotInfo>	resource_slot_;
		};

		/*
			ResourceViewとレジスタのマッピング. このオブジェクトを利用してリソース名からDescriptorSetの対応するスロットへのDescriptor設定をする.
			実質RootSignature.
			このシステムで利用するシェーダリソースのレジスタ番号は以下のような制限とする.

				CBV		0-15
				SRV		0-47
				UAV		0-15
				Sampler	0-15
		*/
		class PipelineResourceViewLayoutDep
		{
		public:
			struct Desc
			{
				ShaderDep* vs = nullptr;
				ShaderDep* hs = nullptr;
				ShaderDep* ds = nullptr;
				ShaderDep* gs = nullptr;
				ShaderDep* ps = nullptr;
				ShaderDep* cs = nullptr;
			};

			// RootSignatureで定義されたあるシェーダステージのあるリソースタイプのテーブルインデックスを保持
			//	commandlist->SetGraphicsRootDescriptorTable での書き込み先テーブル番号となる.
			struct ResourceTable
			{
				// 何番目のDescriptorTableがVSのCBVテーブルか
				s8		vs_cbv_table = -1;
				// 何番目のDescriptorTableがVSのSRVテーブルか
				s8		vs_srv_table = -1;
				// 何番目のDescriptorTableがVSのSamplerテーブルか
				s8		vs_sampler_table = -1;

				s8		hs_cbv_table = -1;
				s8		hs_srv_table = -1;
				s8		hs_sampler_table = -1;

				s8		ds_cbv_table = -1;
				s8		ds_srv_table = -1;
				s8		ds_sampler_table = -1;

				s8		gs_cbv_table = -1;
				s8		gs_srv_table = -1;
				s8		gs_sampler_table = -1;

				// 何番目のDescriptorTableがPSのCBVテーブルか
				s8		ps_cbv_table = -1;
				// 何番目のDescriptorTableがPSのSRVテーブルか
				s8		ps_srv_table = -1;
				// 何番目のDescriptorTableがPSのSamplerテーブルか
				s8		ps_sampler_table = -1;
				// 何番目のDescriptorTableがPSのUAVテーブルか
				s8		ps_uav_table = -1;

				s8		cs_cbv_table = -1;
				s8		cs_srv_table = -1;
				s8		cs_sampler_table = -1;
				s8		cs_uav_table = -1;

			};	// struct InputIndex


			struct ShaderStageSlot
			{
				RootParameterType	type = RootParameterType::_Max;
				s16					slot = -1;
			};
			struct Slot
			{
				ShaderStageSlot vs_stage = {};
				ShaderStageSlot hs_stage = {};
				ShaderStageSlot ds_stage = {};
				ShaderStageSlot gs_stage = {};
				ShaderStageSlot ps_stage = {};
				ShaderStageSlot cs_stage = {};
			};

			PipelineResourceViewLayoutDep();
			~PipelineResourceViewLayoutDep();

			bool Initialize(DeviceDep* p_device, const Desc& desc);
			void Finalize();

			const ResourceTable& GetResourceTable() const
			{
				return resource_table_;
			}

			// 名前でDescriptorSetへハンドル設定
			void SetDescriptorHandle(DescriptorSetDep* p_desc_set, const char* name, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle) const;

			ID3D12RootSignature* GetD3D12RootSignature();
			const ID3D12RootSignature* GetD3D12RootSignature() const;

		private:
			CComPtr<ID3D12RootSignature>	root_signature_;

			// 頂点シェーダリフレクションは入力レイアウト情報などのために保持する
			ShaderReflectionDep		vs_reflection_;

			// リソース名とスロット(register)の対応情報map.
			std::unordered_map<ResourceViewName, Slot> slot_map_;

			// CommandListにDescriptorをセットする際の各シェーダステージ/リソースタイプの対応するRootTableインデックス.
			ResourceTable					resource_table_;
		};

		// パイプラインステート.
		class GraphicsPipelineStateDep
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
				RasterizerState		rasterizer_state = {};
				DepthStencilState	depth_stencil_state = {};

				InputLayout				input_layout = {};
				PrimitiveTopologyType	primitive_topology_type = PrimitiveTopologyType::Triangle;

				u32					num_render_targets = 0;
				ResourceFormat		render_target_formats[8] = {};
				ResourceFormat		depth_stencil_format = ResourceFormat::Format_D24_UNORM_S8_UINT;
				SampleDesc			sample_desc = {};
				u32					node_mask = 0;
			};


			GraphicsPipelineStateDep();
			~GraphicsPipelineStateDep();

			bool Initialize(DeviceDep* p_device, const Desc& desc);
			void Finalize();

			const PipelineResourceViewLayoutDep* GetPipelineResourceViewLayout() const
			{
				return &view_layout_;
			}

			// 名前でDescriptorSetへハンドル設定
			void SetDescriptorHandle(DescriptorSetDep* p_desc_set, const char* name, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle) const;

			ID3D12PipelineState* GetD3D12PipelineState();
			ID3D12RootSignature* GetD3D12RootSignature();
			const ID3D12PipelineState* GetD3D12PipelineState() const;
			const ID3D12RootSignature* GetD3D12RootSignature() const;
		private:
			CComPtr<ID3D12PipelineState>			pso_;
			PipelineResourceViewLayoutDep			view_layout_;
		};
	}
}