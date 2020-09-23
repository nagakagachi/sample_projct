#pragma once

#include "ngl/rhi/rhi.h"
#include "ngl/text/hash_text.h"


#include <iostream>
#include <vector>
#include <unordered_map>


#include "ngl/util/types.h"
#include "ngl/platform/win/window.win.h"

#include <d3d12.h>
#if 1
#include <dxgi1_6.h>
#else
#include <dxgi1_4.h>
#endif


#include <atlbase.h>

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


		class GraphicsCommandListDep;
		class GraphicsCommandQueueDep;

		class FenceDep;

		class DeviceDep;

		using ResourceViewName = ngl::text::FixedString<32>;

		enum class RootParameterType : u16
		{
			ConstantBuffer,
			ShaderResource,
			UnorderedAccess,
			Sampler,

			_Max
		};
		
		static const u32 k_cbv_table_size		= 16;
		static const u32 k_srv_table_size		= 48;
		static const u32 k_uav_table_size		= 16;
		static const u32 k_sampler_table_size	= 16;

		static constexpr u32 RootParameterTableSize(RootParameterType type)
		{
			const u32 type_size[] =
			{
				k_cbv_table_size,
				k_srv_table_size,
				k_uav_table_size,
				k_sampler_table_size,

				0,
			};
			static_assert(std::size(type_size)-1 == static_cast<size_t>(RootParameterType::_Max), "");
			return type_size[static_cast<u32>(type)];
		}


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
				bool				allow_uav = false;
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

			const u32 GetBufferSize() const { return allocated_byte_size_; }
			const Desc& GetDesc() const { return desc_; }

			ID3D12Resource* GetD3D12Resource();

		private:
			Desc	desc_ = {};
			u32		allocated_byte_size_ = 0;

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
				ShaderStage		stage = ShaderStage::VERTEX_SHADER;
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

			ID3D12RootSignature* GetD3D12RootSignature();

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


			GraphicsPipelineStateDep();
			~GraphicsPipelineStateDep();

			bool Initialize(DeviceDep* p_device, const Desc& desc);
			void Finalize();

		private:
			CComPtr<ID3D12PipelineState>			pso_;
			PipelineResourceViewLayoutDep			view_layout_;
		};


		// Pipelineへ設定するDescriptor群をまとめるオブジェクト.
		//	一旦このオブジェクトに描画に必要なDescriptorを設定し, CommapndListにこのオブジェクトを設定する際に動的DescriptorHeapから取得した連続DescriptorにコピーしてそれをPipelineにセットする.
		//	連続Descriptor対応のために各リソースタイプ毎に1Pipelineで使用できるテーブルサイズの上限(レジスタ番号の上限)がある. -> k_cbv_table_size他.
		//	リソース名でレジスタ番号を指定してDescriptorを設定したい場合は対応するPipelineの PipelineResourceViewLayoutDep がmapで保持しているのでそちらを利用する仕組みを作る.
		class DescriptorSetDep
		{
		private:
			template<u32 SIZE>
			struct Handles
			{
				D3D12_CPU_DESCRIPTOR_HANDLE	cpu_handles[SIZE] = {};
				u32							max_slot_count = 0;

				void Reset()
				{
					memset(cpu_handles, 0, sizeof(cpu_handles));
					max_slot_count = 0;
				}

				void SetHandle(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
				{
					assert(index < SIZE);
					cpu_handles[index] = handle;
					max_slot_count = std::max<u32>(max_slot_count, index + 1);
				}
			};

		public:
			DescriptorSetDep()
			{}
			~DescriptorSetDep()
			{}

			void Reset();

			inline void SetVsCbv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
			inline void SetVsSrv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
			inline void SetVsSampler(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
			inline void SetPsCbv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
			inline void SetPsSrv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
			inline void SetPsSampler(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
			inline void SetPsUav(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
			inline void SetGsCbv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
			inline void SetGsSrv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
			inline void SetGsSampler(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
			inline void SetHsCbv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
			inline void SetHsSrv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
			inline void SetHsSampler(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
			inline void SetDsCbv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
			inline void SetDsSrv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
			inline void SetDsSampler(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
			inline void SetCsCbv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
			inline void SetCsSrv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
			inline void SetCsSampler(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
			inline void SetCsUav(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);

			const Handles<k_cbv_table_size>& GetVsCbv() const { return vs_cbv_; }
			const Handles<k_srv_table_size>& GetVsSrv() const { return vs_srv_; }
			const Handles<k_sampler_table_size>& GetVsSampler() const { return vs_sampler_; }
			const Handles<k_cbv_table_size>& GetPsCbv() const { return ps_cbv_; }
			const Handles<k_srv_table_size>& GetPsSrv() const { return ps_srv_; }
			const Handles<k_sampler_table_size>& GetPsSampler() const { return ps_sampler_; }
			const Handles<k_uav_table_size>& GetPsUav() const { return ps_uav_; }
			const Handles<k_cbv_table_size>& GetGsCbv() const { return gs_cbv_; }
			const Handles<k_srv_table_size>& GetGsSrv() const { return gs_srv_; }
			const Handles<k_sampler_table_size>& GetGsSampler() const { return gs_sampler_; }
			const Handles<k_cbv_table_size>& GetHsCbv() const { return hs_cbv_; }
			const Handles<k_srv_table_size>& GetHsSrv() const { return hs_srv_; }
			const Handles<k_sampler_table_size>& GetHsSampler() const { return hs_sampler_; }
			const Handles<k_cbv_table_size>& GetDsCbv() const { return ds_cbv_; }
			const Handles<k_srv_table_size>& GetDsSrv() const { return ds_srv_; }
			const Handles<k_sampler_table_size>& GetDsSampler() const { return ds_sampler_; }
			const Handles<k_cbv_table_size>& GetCsCbv() const { return cs_cbv_; }
			const Handles<k_srv_table_size>& GetCsSrv() const { return cs_srv_; }
			const Handles<k_sampler_table_size>& GetCsSampler() const { return cs_sampler_; }
			const Handles<k_uav_table_size>& GetCsUav() const { return cs_uav_; }

		private:
			Handles<k_cbv_table_size>		vs_cbv_;
			Handles<k_srv_table_size>		vs_srv_;
			Handles<k_sampler_table_size>	vs_sampler_;
			Handles<k_cbv_table_size>		ps_cbv_;
			Handles<k_srv_table_size>		ps_srv_;
			Handles<k_sampler_table_size>	ps_sampler_;
			Handles<k_uav_table_size>		ps_uav_;
			Handles<k_cbv_table_size>		gs_cbv_;
			Handles<k_srv_table_size>		gs_srv_;
			Handles<k_sampler_table_size>	gs_sampler_;
			Handles<k_cbv_table_size>		hs_cbv_;
			Handles<k_srv_table_size>		hs_srv_;
			Handles<k_sampler_table_size>	hs_sampler_;
			Handles<k_cbv_table_size>		ds_cbv_;
			Handles<k_srv_table_size>		ds_srv_;
			Handles<k_sampler_table_size>	ds_sampler_;
			Handles<k_cbv_table_size>		cs_cbv_;
			Handles<k_srv_table_size>		cs_srv_;
			Handles<k_sampler_table_size>	cs_sampler_;
			Handles<k_uav_table_size>		cs_uav_;
		};

		inline void DescriptorSetDep::Reset()
		{
			vs_cbv_.Reset();
			vs_srv_.Reset();
			vs_sampler_.Reset();
			ps_cbv_.Reset();
			ps_srv_.Reset();
			ps_sampler_.Reset();
			ps_uav_.Reset();
			gs_cbv_.Reset();
			gs_srv_.Reset();
			gs_sampler_.Reset();
			hs_cbv_.Reset();
			hs_srv_.Reset();
			hs_sampler_.Reset();
			ds_cbv_.Reset();
			ds_srv_.Reset();
			ds_sampler_.Reset();
			cs_cbv_.Reset();
			cs_srv_.Reset();
			cs_sampler_.Reset();
			cs_uav_.Reset();
		}

		inline void DescriptorSetDep::SetVsCbv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
		{
			vs_cbv_.SetHandle(index, handle);
		}
		inline void DescriptorSetDep::SetVsSrv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
		{
			vs_srv_.SetHandle(index, handle);
		}
		inline void DescriptorSetDep::SetVsSampler(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
		{
			vs_sampler_.SetHandle(index, handle);
		}
		inline void DescriptorSetDep::SetPsCbv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
		{
			ps_cbv_.SetHandle(index, handle);
		}
		inline void DescriptorSetDep::SetPsSrv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
		{
			ps_srv_.SetHandle(index, handle);
		}
		inline void DescriptorSetDep::SetPsSampler(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
		{
			ps_sampler_.SetHandle(index, handle);
		}
		inline void DescriptorSetDep::SetPsUav(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
		{
			ps_uav_.SetHandle(index, handle);
		}
		inline void DescriptorSetDep::SetGsCbv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
		{
			gs_cbv_.SetHandle(index, handle);
		}
		inline void DescriptorSetDep::SetGsSrv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
		{
			gs_srv_.SetHandle(index, handle);
		}
		inline void DescriptorSetDep::SetGsSampler(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
		{
			gs_sampler_.SetHandle(index, handle);
		}
		inline void DescriptorSetDep::SetHsCbv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
		{
			hs_cbv_.SetHandle(index, handle);
		}
		inline void DescriptorSetDep::SetHsSrv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
		{
			hs_srv_.SetHandle(index, handle);
		}
		inline void DescriptorSetDep::SetHsSampler(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
		{
			hs_sampler_.SetHandle(index, handle);
		}
		inline void DescriptorSetDep::SetDsCbv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
		{
			ds_cbv_.SetHandle(index, handle);
		}
		inline void DescriptorSetDep::SetDsSrv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
		{
			ds_srv_.SetHandle(index, handle);
		}
		inline void DescriptorSetDep::SetDsSampler(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
		{
			ds_sampler_.SetHandle(index, handle);
		}
		inline void DescriptorSetDep::SetCsCbv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
		{
			cs_cbv_.SetHandle(index, handle);
		}
		inline void DescriptorSetDep::SetCsSrv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
		{
			cs_srv_.SetHandle(index, handle);
		}
		inline void DescriptorSetDep::SetCsSampler(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
		{
			cs_sampler_.SetHandle(index, handle);
		}
		inline void DescriptorSetDep::SetCsUav(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
		{
			cs_uav_.SetHandle(index, handle);
		}

	}
}