#pragma once


#include <iostream>
#include <vector>
#include <array>
#include <unordered_map>
#include <mutex>


#include "ngl/rhi/rhi.h"

#include "rhi.d3d12.h"

#include "ngl/text/hash_text.h"
#include "ngl/util/types.h"


namespace ngl
{
	namespace rhi
	{
		class PersistentDescriptorAllocator;
		class FrameDescriptorManager;
		class FrameDescriptorInterface;

		struct PersistentDescriptorInfo
		{
			// アロケーション元
			PersistentDescriptorAllocator*	allocator = nullptr;
			u32								allocation_index = ~u32(0);

			D3D12_CPU_DESCRIPTOR_HANDLE	cpu_handle = {};
			D3D12_GPU_DESCRIPTOR_HANDLE	gpu_handle = {};
		};

		/*
			リソースのDescriptorを配置する目的のHeapを管理する
			ここで管理されているDescriptorは直接描画には利用されず,別実装のFrameDescriptorHeap上に描画直前にCopyDescriptorsでコピーされて利用される.

			mutexによって Allocate と Deallocate はスレッドセーフ.

		*/
		class PersistentDescriptorAllocator
		{
		public:
			struct Desc
			{
				D3D12_DESCRIPTOR_HEAP_TYPE	type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
				u32							size = 50000;
			};

			PersistentDescriptorAllocator();
			~PersistentDescriptorAllocator();

			bool Initialize(DeviceDep* p_device, const Desc& desc);
			void Finalize();

			PersistentDescriptorInfo	Allocate();
			void						Deallocate(const PersistentDescriptorInfo& v);

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
			u32								heap_increment_size_ = 0;
			// CPU/GPUハンドル先頭
			D3D12_CPU_DESCRIPTOR_HANDLE		cpu_handle_start_ = {};
			D3D12_GPU_DESCRIPTOR_HANDLE		gpu_handle_start_ = {};

		private:
			static constexpr u32 k_num_flag_elem_bit_ = sizeof(decltype(*use_flag_bit_array_.data())) * 8;
		};

		/*
			CommandListがPipelineにDescriptorを設定する際に連続したDescriptorのテーブルを提供するためのオブジェクト
			非スレッドセーフであり、マルチスレッドでCommand生成をする場合にはCommandList毎にこのオブジェクトを割り当てる
			SwapChainのバッファリング数と同じだけのバッファリングをし、最も古いものをリセットして使いまわしていく.


			FrameDescriptorManagerは巨大なサイズを確保し,要求に応じて連続領域を切り出して貸し出す役割. スレッドセーフ
			切り出す時にフレーム番号を一緒に指定することで、特定のフレームで確保されたまとまった連続領域を纏めて解放できるようにしたい

			FrameDescriptorInterface 固定サイズのスタックのリストを持ち、要求に応じてStackから連続したDescriptorを貸し出す　非スレッドセーフ
			要求数が所持しているStack上から貸し出せない場合は新しいStackとして固定数の連続領域をFrameDescriptorManagerから借り受ける
			FrameDescriptorManagerから借り受ける際にフレーム番号を指定し、描画が完了して使い終わった後にそのフレームでFrameDescriptorManagerから取得された連続領域を纏めて開放するために使う


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
				u32							allocate_descriptor_count_ = 100000;
			};

			FrameDescriptorManager();
			~FrameDescriptorManager();

			bool Initialize(DeviceDep* p_device, const Desc& desc);
			void Finalize();

			void ResetFrameDescriptor(u32 frame_index);

			u32	GetHeapIncrementSize() const 
			{
				return heap_increment_size_;
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
			u32									heap_increment_size_ = 0;
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
			// ある程度大きな範囲で連続DescriptorをFrameDescriptorManagerから取得して使っていく. 足りなくなれば追加で FrameDescriptorManager から取得する.
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

			void ReadyToNewFrame(u32 frame_index);

			bool Allocate(u32 count, D3D12_CPU_DESCRIPTOR_HANDLE& alloc_cpu_handle_head, D3D12_GPU_DESCRIPTOR_HANDLE& alloc_gpu_handle_head);

		private:
			Desc								desc_ = {};

			FrameDescriptorManager*				p_manager_ = nullptr;
			
			u32									frame_index_ = ~u32(0);

			u32									cur_stack_use_count_ = 0;
			D3D12_CPU_DESCRIPTOR_HANDLE			cur_stack_cpu_handle_start_ = {};
			D3D12_GPU_DESCRIPTOR_HANDLE			cur_stack_gpu_handle_start_ = {};

			// デバッグ用にフレームで確保したStackの履歴を保存
			std::vector<std::tuple<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE>> stack_history_;
		};
	}
}