

#include "rhi_descriptor.d3d12.h"

#include <array>
#include <algorithm>

#include "rhi_util.d3d12.h"
#include "ngl/util/bit_operation.h"


namespace ngl
{
	namespace rhi
	{
		PersistentDescriptorAllocator::PersistentDescriptorAllocator()
		{
		}
		PersistentDescriptorAllocator::~PersistentDescriptorAllocator()
		{
			Finalize();
		}

		bool PersistentDescriptorAllocator::Initialize(DeviceDep* p_device, const Desc& desc)
		{
			assert(p_device);
			assert(0 < desc.size);

			desc_ = desc;
			last_allocate_index_ = 0;
			num_allocated_ = 0;
			tail_fraction_bit_mask_ = 0;

			// Heap作成
			{
				heap_desc_ = {};
				heap_desc_.Type = desc.type;
				heap_desc_.NumDescriptors = desc.size;
				heap_desc_.NodeMask = 0;
				// このHeap上のDescriptorは直接描画に利用しないためNone指定.
				heap_desc_.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

				if (FAILED(p_device->GetD3D12Device()->CreateDescriptorHeap(&heap_desc_, IID_PPV_ARGS(&p_heap_))))
				{
					std::cout << "ERROR: Create DescriptorHeap" << std::endl;
					return false;
				}

				heap_increment_size_ = p_device->GetD3D12Device()->GetDescriptorHandleIncrementSize(heap_desc_.Type);
				cpu_handle_start_ = p_heap_->GetCPUDescriptorHandleForHeapStart();
				gpu_handle_start_ = p_heap_->GetGPUDescriptorHandleForHeapStart();
			}

			{
				// 管理用情報構築
				//constexpr auto num_flag_elem_bit = sizeof(decltype(*use_flag_bit_array_.data())) * 8;
				num_use_flag_elem_ = (desc.size + (k_num_flag_elem_bit_ - 1)) / k_num_flag_elem_bit_;
				use_flag_bit_array_.resize(num_use_flag_elem_);
				
				// リセット

				// NOTE:
				//	使用ビットフラグの総ビット数は実際の確保可能サイズよりも大きいため、末尾チェックを簡略化するために事前に末尾の使用不可ビットは1にしておく.
				//	ランタイムでこの部分を0にすると不正な位置にAllocationできると判断されてしまうため上書きしないように！
				// 一旦すべて0クリアした後に端数部を1で埋める.
				std::fill_n(use_flag_bit_array_.data(), num_use_flag_elem_, 0u);
				// 端数
				if (0 < desc.size)
				{
					const u32 fraction = desc.size - k_num_flag_elem_bit_ * (num_use_flag_elem_ - 1);
					if (0 < fraction && fraction < k_num_flag_elem_bit_)
					{
						const u32 lower_mask = (1u << fraction) - 1;
						tail_fraction_bit_mask_ = ~lower_mask;

						// 末尾要素の端数部が使用されないようにマスクを書き込み. ここの部分が操作中にゼロになることは発生し得ない. 発生した場合はバグ.
						use_flag_bit_array_[num_use_flag_elem_ - 1] = tail_fraction_bit_mask_;
					}
				}
			}

			return true;
		}
		void PersistentDescriptorAllocator::Finalize()
		{
			// CComPtrで解放
			p_heap_ = nullptr;
		}

		/*
			確保
		*/
		PersistentDescriptorInfo PersistentDescriptorAllocator::Allocate()
		{
			PersistentDescriptorInfo ret = {};

			constexpr u32 full_mask = ~u32(0);
			u32 find = full_mask;


			// ロック
			std::lock_guard<std::mutex> lock(mutex_);

			// ---------------------------------------------------------------------------------------------------------------------------------
			// 念の為端数部のマスクが書き換わっていないかチェック
			assert(tail_fraction_bit_mask_ == (tail_fraction_bit_mask_ & use_flag_bit_array_[num_use_flag_elem_ - 1]));

			// 空きbitがある要素を探す
			// last_allocate_index_を使って連続Allocateを多少マシにする. 毎回0から検索する場合の3倍程度は速い. 更に高速化する場合は32要素のどこかに空きがあれば対応する1bitが0になるような階層bit列などを実装する.
			// allocate_index_から使用ビットフラグ要素インデックスに変換.
			const u32 last_allocate_flag_elem_index = last_allocate_index_ / k_num_flag_elem_bit_;
			// 最初に last_allocate_flag_elem_index から末端へ検索( last_allocate_flag_elem_index+1でないのはDeallocateされている可能性があるため )
			for (auto i = last_allocate_flag_elem_index; i < num_use_flag_elem_; ++i)
			{
				if (full_mask != use_flag_bit_array_[i])
				{
					find = i;
					break;
				}
			}
			if (num_use_flag_elem_ <= find)
			{
				// 見つからなければ0からlast_allocate_flag_elem_indexまで検索
				for (auto i = 0u; i < last_allocate_flag_elem_index; ++i)
				{
					if (full_mask != use_flag_bit_array_[i])
					{
						find = i;
						break;
					}
				}
			}

			// 空きが見つからなかったら即終了
			if (num_use_flag_elem_ <= find)
			{
				std::cout << "PersistentDescriptorAllocator::Allocate: Failed to Allocate" << std::endl;
				return ret;
			}

			// 反転ビットのLSBを空きとして利用する.
			const s32 empty_bit_pos = LeastSignificantBit64(~(static_cast<u64>(use_flag_bit_array_[find])));
			assert(0 <= empty_bit_pos && empty_bit_pos < 32);
			// アロケーション位置計算
			const u32 allocation_index = (find * k_num_flag_elem_bit_) + empty_bit_pos;
			// アロケーション位置が確保しているHeap要素数を超えている場合は失敗 ( 32bit使用ビットフラグは32単位だがHeap自体は1単位なので末端でありうる )
			// 使用ビットフラグの末端の無効部分を1で埋めておけばここのチェックは不要だが誤ってゼロで埋めたりするとバグになるので素直に実装する.
			if (desc_.size <= allocation_index)
			{
				std::cout << "PersistentDescriptorAllocator::Allocate: Failed to Allocate" << std::endl;
				return ret;
			}

			// 最後にAllocateしたインデックスを保存(使用ビットフラグ要素インデックスではないので注意)
			last_allocate_index_ = allocation_index;
			// 使用ビットを建てる
			use_flag_bit_array_[find] |= (1 << empty_bit_pos);

			assert(desc_.size >= num_allocated_);
			// アロケーション数加算
			++num_allocated_;
			// ---------------------------------------------------------------------------------------------------------------------------------


			// 戻り値セットアップ
			ret.allocator = this;
			ret.allocation_index = allocation_index;
			// ハンドルセット
			ret.cpu_handle = cpu_handle_start_;
			ret.gpu_handle = gpu_handle_start_;
			// アドレスオフセット
			const auto handle_offset = heap_increment_size_ * ret.allocation_index;
			ret.cpu_handle.ptr += static_cast<size_t>(handle_offset);
			ret.gpu_handle.ptr += static_cast<size_t>(handle_offset);

			return ret;
		}

		/*
			解放
		*/
		void PersistentDescriptorAllocator::Deallocate(const PersistentDescriptorInfo& v)
		{
			// 割当元が自身でない要素の破棄をリクエストされたらアサート
			assert(v.allocator == this && v.allocation_index < desc_.size);
			if ((this != v.allocator) || (desc_.size <= v.allocation_index))
				return;

			// アロケーションインデックスからビット位置計算.
			const u32 flag_elem_index = v.allocation_index / k_num_flag_elem_bit_;
			const u32 flag_elem_bit_pos = v.allocation_index - (flag_elem_index * k_num_flag_elem_bit_);
			const u32 flag_elem_mask = (1 << flag_elem_bit_pos);

			// ロック
			std::lock_guard<std::mutex> lock(mutex_);

			// ---------------------------------------------------------------------------------------------------------------------------------
			// 破棄対象であるにも関わらず未使用状態の場合はアサート
			assert(0 != (use_flag_bit_array_[flag_elem_index] & flag_elem_mask));
			if (0 != (use_flag_bit_array_[flag_elem_index] & flag_elem_mask))
			{
				// フラグを落とす
				use_flag_bit_array_[flag_elem_index] &= ~(1 << flag_elem_bit_pos);

				assert(0 < num_allocated_);
				// アロケーション数減算
				--num_allocated_;
			}
			// ---------------------------------------------------------------------------------------------------------------------------------
		}
	}
}