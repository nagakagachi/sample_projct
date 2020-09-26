

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
		// -------------------------------------------------------------------------------------------------------------------------------------------------


		// -------------------------------------------------------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------------------------------------------------------
		FrameDescriptorManager::FrameDescriptorManager()
		{
		}
		FrameDescriptorManager::~FrameDescriptorManager()
		{
			Finalize();
		}

		bool FrameDescriptorManager::Initialize(DeviceDep* p_device, const Desc& desc)
		{
			assert(nullptr != p_device);
			if (!p_device)
				return false;
			assert(0 < desc.allocate_descriptor_count_);
			if (0 >= desc.allocate_descriptor_count_)
				return false;

			desc_ = desc;

			// Heap作成
			{
				heap_desc_ = {};
				heap_desc_.Type = desc.type;
				heap_desc_.NumDescriptors = desc.allocate_descriptor_count_;
				heap_desc_.NodeMask = 0;
				// このHeap上のDescriptorは直接描画に利用しないためNone指定.
				heap_desc_.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

				if (FAILED(p_device->GetD3D12Device()->CreateDescriptorHeap(&heap_desc_, IID_PPV_ARGS(&p_heap_))))
				{
					std::cout << "ERROR: Create DescriptorHeap" << std::endl;
					return false;
				}

				heap_increment_size_ = p_device->GetD3D12Device()->GetDescriptorHandleIncrementSize(heap_desc_.Type);
				cpu_handle_start_ = p_heap_->GetCPUDescriptorHandleForHeapStart();
				gpu_handle_start_ = p_heap_->GetGPUDescriptorHandleForHeapStart();
			}

			// フリーレンジセットアップ
			for (int i = 0; i < range_node_pool_.size(); ++i)
			{
				range_node_pool_[i] = {};
				range_node_pool_[i].frame_index = k_invalid_frame_index;
				range_node_pool_[i].start_index = 0;
				range_node_pool_[i].end_index = 0;
				if (i > 0)
				{
					range_node_pool_[i].prev = &range_node_pool_[i - 1];
				}
				if (i < range_node_pool_.size() - 1)
				{
					range_node_pool_[i].next = &range_node_pool_[i + 1];
				}
			}
			// フリーノードリスト登録.
			free_node_list_ = &range_node_pool_[0];


			// 初期レンジ全域未使用のためにフリーリストから一つ取得.
			frame_descriptor_range_head_ = free_node_list_;
			free_node_list_ = free_node_list_->next;
			frame_descriptor_range_head_->RemoveFromList();

			// 設定
			frame_descriptor_range_head_->frame_index = k_invalid_frame_index;
			frame_descriptor_range_head_->start_index = 0;
			frame_descriptor_range_head_->end_index = frame_descriptor_range_head_->start_index + (desc_.allocate_descriptor_count_ - 1);


			return true;
		}
		void FrameDescriptorManager::Finalize()
		{
		}
		/*
			frame_index で確保された範囲をリセットして再利用可能にする
		*/
		void FrameDescriptorManager::ResetFrameDescriptor(u32 frame_index)
		{
			if (k_invalid_frame_index == frame_index)
				return;

			// ロック
			std::lock_guard<std::mutex> lock(mutex_);

			auto* range_ptr = frame_descriptor_range_head_;
			for (; range_ptr != nullptr;)
			{
				auto* cur_node = range_ptr;
				range_ptr = range_ptr->next;

				// 対象のフレームインデックスか, 無効インデックスの場合に処理. (無効インデックスの場合は実質マージ処理をするだけ)
				if (frame_index == cur_node->frame_index || k_invalid_frame_index == cur_node->frame_index)
				{
					// フレームインデックスを無効化して再利用可能.
					cur_node->frame_index = k_invalid_frame_index;

					// 一つ前のノードが未使用ノードならマージしてこのノードをフリーリストへ移動.
					auto prev_node = cur_node->prev;
					if (prev_node && cur_node->frame_index == prev_node->frame_index)
					{
						// prevへ範囲をマージ
						prev_node->end_index = cur_node->end_index;


						// cur_nodeを除外
						cur_node->RemoveFromList();

						// cur_nodeをフリーリストへ移動
						{
							free_node_list_->prev = cur_node;
							cur_node->next = free_node_list_;
							cur_node->prev = nullptr;
							free_node_list_ = cur_node;
						}
					}
				}
			}
		}
		bool FrameDescriptorManager::AllocateFrameDescriptorArray(u32 frame_index, u32 count, D3D12_CPU_DESCRIPTOR_HANDLE& alloc_cpu_handle_head, D3D12_GPU_DESCRIPTOR_HANDLE& alloc_gpu_handle_head)
		{
			if (k_invalid_frame_index == frame_index)
				return false;

			// ロック
			std::lock_guard<std::mutex> lock(mutex_);

			// 空きのある未使用ノードを探す
			auto* range_ptr = frame_descriptor_range_head_;
			for (; range_ptr != nullptr; range_ptr = range_ptr->next)
			{
				if (k_invalid_frame_index == range_ptr->frame_index)
				{
					// 十分な空きがあるか
					const auto empty_count = (range_ptr->end_index - range_ptr->start_index + 1);
					if (count <= empty_count)
						break;
				}
			}
			assert(range_ptr);
			// 空きが無かったため失敗
			if (!range_ptr)
			{
				return false;
			}

			// 割当開始インデックス設定
			u32 alloc_start_index = range_ptr->start_index;

			// アロケーション情報管理の更新.
			// ターゲットのノードを分割して割当. ただし一つ前のノードが同じframe_indexの場合はフリーリストから取得せずにマージする.
			if (range_ptr->prev && range_ptr->prev->frame_index == frame_index)
			{
				// 一つ前のノードにマージする
				auto* new_node = range_ptr->prev;

				// prevノードの終端インデックスを更新
				new_node->end_index = range_ptr->start_index + (count - 1);

				// 分割ノードの開始インデックスを更新
				range_ptr->start_index = new_node->end_index + 1;
				// 範囲を使い切ったノードは除去してフリーリストへ
				if (range_ptr->start_index >= range_ptr->end_index)
				{
					range_ptr->RemoveFromList();

					// range_ptrをフリーリストへ移動
					{
						free_node_list_->prev = range_ptr;
						range_ptr->next = free_node_list_;
						range_ptr->prev = nullptr;
						free_node_list_ = range_ptr;
					}
				}
			}
			else
			{
				// フリーリストからノードを確保して接続する
				assert(free_node_list_);
				if (!free_node_list_)
				{
					// フリーリストが空
					return false;
				}
				// フリーリスト先頭取得
				auto* new_node = free_node_list_;
				// フリーリスト先頭更新
				free_node_list_ = free_node_list_->next;
				// フリーリストから取り外し
				new_node->RemoveFromList();


				*new_node = {};
				new_node->frame_index = frame_index;
				// 開始インデックスからcount分を割り当て
				new_node->start_index = range_ptr->start_index;
				new_node->end_index = new_node->start_index + (count - 1);
			
				// 分割後の開始インデックスを更新
				range_ptr->start_index = new_node->end_index + 1;

				// 接続更新
				new_node->prev = range_ptr->prev;
				if (new_node->prev)
					new_node->prev->next = new_node;
				new_node->next = range_ptr;
				if (new_node->next)
					new_node->next->prev = new_node;

				// 先頭アドレス更新
				if (frame_descriptor_range_head_ == range_ptr)
					frame_descriptor_range_head_ = range_ptr->prev;
			}

			// アロケーション位置の先頭を取得して返す.
			alloc_cpu_handle_head = cpu_handle_start_;
			alloc_cpu_handle_head.ptr += (static_cast<UINT64>(heap_increment_size_) * static_cast<UINT64>(alloc_start_index));
			alloc_gpu_handle_head = gpu_handle_start_;
			alloc_gpu_handle_head.ptr += (static_cast<UINT64>(heap_increment_size_) * static_cast<UINT64>(alloc_start_index));

			return true;
		}
		// -------------------------------------------------------------------------------------------------------------------------------------------------


		// -------------------------------------------------------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------------------------------------------------------
		FrameDescriptorInterface::FrameDescriptorInterface()
		{
		}
		FrameDescriptorInterface::~FrameDescriptorInterface()
		{
			Finalize();
		}

		bool FrameDescriptorInterface::Initialize(FrameDescriptorManager* p_manager, const Desc& desc)
		{
			assert(p_manager);
			if (!p_manager)
				return false;
			assert(0 < desc.stack_size);
			if (0 >= desc.stack_size)
				return false;

			desc_ = desc;

			p_manager_ = p_manager;

			// 初回は無効なフレーム
			frame_index_ = ~u32(0);

			cur_stack_use_count_ = desc_.stack_size;

			cur_stack_cpu_handle_start_ = {};
			cur_stack_gpu_handle_start_ = {};

			return true;
		}
		void FrameDescriptorInterface::Finalize()
		{
			// 念の為フレームインデックスのDescriptorを解放.
			p_manager_->ResetFrameDescriptor(frame_index_);
			// 初期はFull
			cur_stack_use_count_ = desc_.stack_size;

			p_manager_ = nullptr;

			// ヒストリクリア
			stack_history_.clear();
		}
		// 新しいフレームの準備
		void FrameDescriptorInterface::ReadyToNewFrame(u32 frame_index)
		{
			assert(p_manager_);

			// 初期はFull
			cur_stack_use_count_ = desc_.stack_size;

			// 前回処理した際のフレームインデックスを解放.
			p_manager_->ResetFrameDescriptor(frame_index_);
			// フレームインデックス更新
			frame_index_ = frame_index;

			// ヒストリクリア
			stack_history_.clear();
		}
		bool FrameDescriptorInterface::Allocate(u32 count, D3D12_CPU_DESCRIPTOR_HANDLE& alloc_cpu_handle_head, D3D12_GPU_DESCRIPTOR_HANDLE& alloc_gpu_handle_head)
		{
			assert(p_manager_);

			// そもそものスタックサイズよりも大きいサイズを要求されることは想定外.
			assert(desc_.stack_size >= count);
			if (desc_.stack_size < count)
				return false;

			// 現在のスタックが足りない場合はManagerから新規に取得. 連続要素が必要なので.
			if (desc_.stack_size < cur_stack_use_count_ + count)
			{
				// Empty
				cur_stack_use_count_ = 0;
				// 新規スタック取得
				auto alloc_result = p_manager_->AllocateFrameDescriptorArray(frame_index_, desc_.stack_size, cur_stack_cpu_handle_start_, cur_stack_gpu_handle_start_);
				if (alloc_result)
				{
					stack_history_.push_back(std::tuple<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE>(cur_stack_cpu_handle_start_, cur_stack_gpu_handle_start_));
				}
				else
				{
					assert(false);
					return false;
				}
			}

			// 改めてチェック
			assert(desc_.stack_size >= cur_stack_use_count_ + count);
			if (desc_.stack_size < cur_stack_use_count_ + count)
				return false;
			
			const auto increment_size = p_manager_->GetHeapIncrementSize();
			alloc_cpu_handle_head = cur_stack_cpu_handle_start_;
			alloc_cpu_handle_head.ptr += (increment_size * cur_stack_use_count_);
			alloc_gpu_handle_head = cur_stack_gpu_handle_start_;
			alloc_gpu_handle_head.ptr += (increment_size * cur_stack_use_count_);

			// スタック消費量更新.
			cur_stack_use_count_ += count;
		}
		// -------------------------------------------------------------------------------------------------------------------------------------------------
	}
}