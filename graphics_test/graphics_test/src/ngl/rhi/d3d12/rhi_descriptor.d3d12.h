#pragma once


#include <iostream>
#include <vector>
#include <queue>
#include <array>
#include <unordered_map>
#include <mutex>


#include "ngl/rhi/rhi.h"
#include "rhi_util.d3d12.h"

#include "rhi.d3d12.h"

#include "ngl/text/hash_text.h"
#include "ngl/util/types.h"


namespace ngl
{
	namespace rhi
	{
		class DeviceDep;
		class PersistentDescriptorAllocator;
		class FrameDescriptorManager;
		class FrameDescriptorInterface;


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
				// 設定された最大のレジスタインデックス. このシステムでは必ず0開始連番でバインドするため, 最大レジスタインデックスを保持しておいてCommandListに積む際にFrameDescriptorを確保する.
				int							max_use_register_index = -1;

				void Reset()
				{
					memset(cpu_handles, 0, sizeof(cpu_handles));
					max_use_register_index = -1;
				}

				void SetHandle(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
				{
					assert(index < SIZE);
					cpu_handles[index] = handle;
					max_use_register_index = std::max<int>(max_use_register_index, static_cast<int>(index));
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


		struct PersistentDescriptorInfo
		{
			static constexpr u32 k_invalid_allocation_index = ~u32(0);

			static bool IsValid(const PersistentDescriptorInfo& v)
			{
				return (k_invalid_allocation_index != v.allocation_index && nullptr != v.allocator);
			}
			bool IsValid() const
			{
				return IsValid(*this);
			}

			// アロケーション元
			PersistentDescriptorAllocator*	allocator = nullptr;
			u32								allocation_index = k_invalid_allocation_index;

			D3D12_CPU_DESCRIPTOR_HANDLE	cpu_handle = {};
			D3D12_GPU_DESCRIPTOR_HANDLE	gpu_handle = {};
		};

		/*
			リソースのDescriptorを配置する目的のグローバルなHeapを管理する
			ここで管理されているDescriptorは直接描画には利用されず,別実装のFrameDescriptorHeap上に描画直前にCopyDescriptorsでコピーされて利用される.

			mutexによって Allocate と Deallocate はスレッドセーフ.

		*/
		class PersistentDescriptorAllocator
		{
		public:
			struct Desc
			{
				D3D12_DESCRIPTOR_HEAP_TYPE	type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
				u32							allocate_descriptor_count_ = 500000;
			};

			PersistentDescriptorAllocator();
			~PersistentDescriptorAllocator();

			bool Initialize(DeviceDep* p_device, const Desc& desc);
			void Finalize();

			PersistentDescriptorInfo	Allocate();
			void						Deallocate(const PersistentDescriptorInfo& v);

			PersistentDescriptorInfo	GetDefaultPersistentDescriptor() const
			{
				return default_persistent_descriptor_;
			}

		private:
			std::mutex					mutex_;

			// オブジェクト初期化情報
			Desc		desc_ = {};

			// 要素のアロケーション状態フラグ.
			u32					num_use_flag_elem_ = 0;
			std::vector<u32>	use_flag_bit_array_;
			// 空き検索を効率化するために前回のAllocateで確保した位置の次のインデックスを保持しておく. シーケンシャルな確保が効率化されるだけで根本的な高速化にはならないので注意.
			u32					last_allocate_index_ = 0;
			// アロケーションされている要素数
			u32					num_allocated_ = 0;

			// デバッグ用の端数部マスク
			u32					tail_fraction_bit_mask_ = 0;

			// Heap本体
			CComPtr<ID3D12DescriptorHeap>	p_heap_ = nullptr;
			// Headの定義情報
			D3D12_DESCRIPTOR_HEAP_DESC		heap_desc_{};
			// Heap上の要素アドレスサイズ
			u32								handle_increment_size_ = 0;
			// CPU/GPUハンドル先頭
			D3D12_CPU_DESCRIPTOR_HANDLE		cpu_handle_start_ = {};
			D3D12_GPU_DESCRIPTOR_HANDLE		gpu_handle_start_ = {};


			// 安全のために未使用スロットへコピーするための空のデフォルトDescriptor.
			PersistentDescriptorInfo		default_persistent_descriptor_;

		private:
			static constexpr u32 k_num_flag_elem_bit_ = sizeof(decltype(*use_flag_bit_array_.data())) * 8;
		};

		/*
			FrameDescriptorManagerは巨大なサイズを確保し, FrameDescriptorInterfaceからの要求に応じて連続領域を切り出して貸し出す役割. スレッドセーフ
			切り出す時にフレーム番号を一緒に指定することで、特定のフレームで確保されたまとまった連続領域を纏めて解放できるようにしたい

			FrameDescriptorInterface 固定サイズのスタックのリストを持ち、要求に応じてStackから連続したDescriptorを貸し出す　非スレッドセーフ
			要求数が所持しているStack上から貸し出せない場合は新しいStackとして固定数の連続領域をFrameDescriptorManagerから借り受ける
			FrameDescriptorManagerから借り受ける際にフレーム番号を指定し、描画が完了して使い終わった後にそのフレームでFrameDescriptorManagerから取得された連続領域を纏めて開放するために使う


			SamplerはHeapのサイズがAPIで制限されているため別途クラスを用意する.
		*/
		class FrameDescriptorManager
		{
			friend class FrameDescriptorInterface;
		public:
			static const u32	k_invalid_frame_index = ~u32(0);
			struct Desc
			{
				D3D12_DESCRIPTOR_HEAP_TYPE	type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
				// 一括確保するDescriptor数
				u32							allocate_descriptor_count_ = 500000;
			};

			FrameDescriptorManager();
			~FrameDescriptorManager();

			bool Initialize(DeviceDep* p_device, const Desc& desc);
			void Finalize();

			void ResetFrameDescriptor(u32 frame_index);

			u32	GetHandleIncrementSize() const 
			{
				return handle_increment_size_;
			}


			ID3D12DescriptorHeap* GetD3D12DescriptorHeap()
			{
				return p_heap_;
			}

		private:
			// FrameDescriptorInterface からの呼び出し用.
			bool AllocateFrameDescriptorArray(u32 frame_index, u32 count, D3D12_CPU_DESCRIPTOR_HANDLE& alloc_cpu_handle_head, D3D12_GPU_DESCRIPTOR_HANDLE& alloc_gpu_handle_head);

		private:
			std::mutex		mutex_;

			Desc								desc_;

			// Heap本体
			CComPtr<ID3D12DescriptorHeap>		p_heap_ = nullptr;
			D3D12_DESCRIPTOR_HEAP_DESC			heap_desc_{};
			// Heap上の要素アドレスサイズ
			u32									handle_increment_size_ = 0;
			// CPU/GPUハンドル先頭
			D3D12_CPU_DESCRIPTOR_HANDLE			cpu_handle_start_ = {};
			D3D12_GPU_DESCRIPTOR_HANDLE			gpu_handle_start_ = {};
			
			struct FrameDescriptorRangeListNode
			{
				u32					frame_index	= k_invalid_frame_index;
				u32					start_index	= 0;
				u32					end_index	= 0;

				struct FrameDescriptorRangeListNode* prev		 = nullptr;
				struct FrameDescriptorRangeListNode* next		= nullptr;


				void RemoveFromList()
				{
					if (prev)
						prev->next = next;
					if (next)
						next->prev = prev;

					prev = nullptr;
					next = nullptr;
				}
			};

			// SwapChainのバッファリング数と同じだけのフレーム数でリセットされるレンジ情報なので1024以上も断片化することは無いはず.
			std::array<FrameDescriptorRangeListNode, 1024>	range_node_pool_;
			FrameDescriptorRangeListNode*					frame_descriptor_range_head_;
			FrameDescriptorRangeListNode*					free_node_list_ = nullptr;
		};

		/*
			各スレッドのCommandListが保持するFrameDescriptor取得用インターフェイス
				ある程度大きな範囲で連続DescriptorをFrameDescriptorManagerから取得して使っていく. 足りなくなれば追加で FrameDescriptorManager から取得する.
		*/
		class FrameDescriptorInterface
		{
		public:
			struct Desc
			{
				u32						stack_size = 2000;
			};

			FrameDescriptorInterface();
			~FrameDescriptorInterface();

			bool Initialize(FrameDescriptorManager* p_manager, const Desc& desc);
			void Finalize();

			/*
					ReadyToNewFrame(frame_index);
					frame_index = (frame_index + 1)%buffer_count;
			*/
			void ReadyToNewFrame(u32 frame_index);

			bool Allocate(u32 count, D3D12_CPU_DESCRIPTOR_HANDLE& alloc_cpu_handle_head, D3D12_GPU_DESCRIPTOR_HANDLE& alloc_gpu_handle_head);

			FrameDescriptorManager* GetFrameDescriptorManager()
			{
				return p_manager_;
			}

		private:
			Desc								desc_ = {};

			FrameDescriptorManager*				p_manager_ = nullptr;
			
			u32									frame_index_ = ~u32(0);

			u32									cur_stack_use_count_ = 0;
			D3D12_CPU_DESCRIPTOR_HANDLE			cur_stack_cpu_handle_start_ = {};
			D3D12_GPU_DESCRIPTOR_HANDLE			cur_stack_gpu_handle_start_ = {};

			// 確認用にフレームで確保したStackの履歴を保存
			std::vector<std::tuple<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE>> stack_history_;
		};



		// 巨大な単一HeapではなくPage単位で確保するHeapManager. 一つのHeapのサイズに制限があるSampler向けだが, CBV_SRV_UAVでも利用可能.
		// 返却されたPageの利用可能リストへの移動などの処理はDeviceのフレームインデックスと協調して自動的に実行される. (Allocate時についでに処理している).
		class FrameDescriptorHeapPagePool
		{
		public:
			static constexpr u32 k_max_sampler_heap_handle_count = 2048;// D3D12の制限2048.

			// HeapTypeの種類数の定義のためだけの意味のない定義.
			static constexpr D3D12_DESCRIPTOR_HEAP_TYPE k_heap_types[] =
			{
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
			};
			static constexpr auto k_num_heap_types = std::size(k_heap_types);


			static constexpr u32 GetHeapTypeIndex(D3D12_DESCRIPTOR_HEAP_TYPE t)
			{
				assert(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV == t || D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER == t);

				return (D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV == t)? 0 : 1;
			}

			FrameDescriptorHeapPagePool();
			~FrameDescriptorHeapPagePool();

			bool Initialize(DeviceDep* p_device);
			void Finalize();

			// Pageを割り当て.
			ID3D12DescriptorHeap* AllocatePage(D3D12_DESCRIPTOR_HEAP_TYPE t);
			// Pageを返却.
			void DeallocatePage(D3D12_DESCRIPTOR_HEAP_TYPE t, ID3D12DescriptorHeap* p_heap);
 
			u32 GetPageSize(D3D12_DESCRIPTOR_HEAP_TYPE t) const;
			u32	GetHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE t) const;

		private:
			DeviceDep* p_device_ = {};

			// index 0 -> CBV_SRV_UAV
			// index 1 -> SAMPLER
			std::mutex		mutex_[k_num_heap_types] = {};

			u32 page_size_[k_num_heap_types] = {};

			u32 handle_increment_size_[k_num_heap_types] = {};

			// 生成したHeapは解放のためにすべてスマートポインタで保持.
			std::vector<CComPtr<ID3D12DescriptorHeap>>	created_pool_[k_num_heap_types];
			std::queue<ID3D12DescriptorHeap*>			available_pool_[k_num_heap_types];
			// 返却時のフレームインデックスをペアで格納.
			std::queue<std::pair<u64, ID3D12DescriptorHeap*>> retired_pool_[k_num_heap_types];

		};

		/*
			CommandListがそれぞれ保持するフレーム毎のDescriptorHeap管理.
			内部ではPoolへの参照をもち, Poolから一定サイズのHeap単位をPageとして取得して利用する.
			
			FrameDescriptorInterface との違いは巨大な単一のHeapではなくPage単位のHeapを利用する点.
			これによりHeap辺り2048のハンドルまでという制限のあるSamplerのDescriptorにも対応している.
		*/
		class FrameDescriptorHeapPageInterface
		{
		public:
			struct Desc
			{
				// D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV or D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER.
				D3D12_DESCRIPTOR_HEAP_TYPE	type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			};


			FrameDescriptorHeapPageInterface();
			~FrameDescriptorHeapPageInterface();

			bool Initialize(FrameDescriptorHeapPagePool* p_pool, const Desc& desc);
			void Finalize();

			const FrameDescriptorHeapPagePool* GetPool() const { return p_pool_; }
			FrameDescriptorHeapPagePool* GetPool() { return p_pool_; }

			bool Allocate(u32 handle_count, D3D12_CPU_DESCRIPTOR_HANDLE& alloc_cpu_handle_head, D3D12_GPU_DESCRIPTOR_HANDLE& alloc_gpu_handle_head);
			ID3D12DescriptorHeap* GetD3D12DescriptorHeap() { return p_cur_heap_; }

		private:
			Desc							desc_ = {};
			FrameDescriptorHeapPagePool*	 p_pool_ = {};

			u32								use_handle_count_ = 0;
			ID3D12DescriptorHeap*			p_cur_heap_ = {};
			D3D12_CPU_DESCRIPTOR_HANDLE		cur_cpu_handle_start_ = {};
			D3D12_GPU_DESCRIPTOR_HANDLE		cur_gpu_handle_start_ = {};
		};


	}
}