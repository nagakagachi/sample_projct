#pragma once


#include <iostream>
#include <vector>
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
				u32							size = 128;
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

	}
}