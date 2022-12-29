

#include "rhi_descriptor.d3d12.h"

#include <array>
#include <algorithm>
#include <limits>

#include "ngl/util/bit_operation.h"


namespace ngl
{
	namespace rhi
	{
		DescriptorHeapWrapper::DescriptorHeapWrapper()
		{
		}
		DescriptorHeapWrapper::~DescriptorHeapWrapper()
		{
			Finalize();
		}
		bool DescriptorHeapWrapper::Initialize(DeviceDep* p_device, const Desc& desc)
		{
			assert(p_device);
			assert(0 < desc.allocate_descriptor_count_);
			if (!p_device || 0 >= desc.allocate_descriptor_count_)
			{
				return false;
			}

			desc_ = desc;
			// Heap作成
			{
				heap_desc_ = {};
				heap_desc_.Type = desc_.type;
				heap_desc_.NumDescriptors = desc_.allocate_descriptor_count_;
				heap_desc_.NodeMask = 0;
				// SHaderからの可視性.
				//https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-copydescriptors
				heap_desc_.Flags = (desc.shader_visible)? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

				if (FAILED(p_device->GetD3D12Device()->CreateDescriptorHeap(&heap_desc_, IID_PPV_ARGS(&p_heap_))))
				{
					std::cout << "[ERROR] Create DescriptorHeap" << std::endl;
					return false;
				}

				handle_increment_size_ = p_device->GetD3D12Device()->GetDescriptorHandleIncrementSize(heap_desc_.Type);
				cpu_handle_start_ = p_heap_->GetCPUDescriptorHandleForHeapStart();
				gpu_handle_start_ = p_heap_->GetGPUDescriptorHandleForHeapStart();
			}
			return true;
		}
		void DescriptorHeapWrapper::Finalize()
		{
			// CComPtrで解放
			p_heap_ = nullptr;
		}


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
			assert(0 < desc.allocate_descriptor_count_);

			desc_ = desc;
			last_allocate_index_ = 0;
			num_allocated_ = 0;
			tail_fraction_bit_mask_ = 0;

			// Heap作成
			{
				// このHeap上のDescriptorは直接描画に利用しないためNone指定によってシェーダから不可視(InVisible)とする.
				// CopyDescriptorsのコピー元はシェーダから不可視とする必要があるためD3D12_DESCRIPTOR_HEAP_FLAG_NONE.
				// シェーダから可視なヒープはCPUからのアクセスが低速でパフォーマンスに影響があるため.
				// 
				//https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-copydescriptors
				DescriptorHeapWrapper::Desc heap_desc = {};
				heap_desc.allocate_descriptor_count_ = desc.allocate_descriptor_count_;
				heap_desc.type = desc.type;
				heap_desc.shader_visible = false;

				if (!heap_wrapper_.Initialize(p_device, heap_desc))
				{
					return false;
				}
			}

			{
				// 管理用情報構築
				num_use_flag_elem_ = (desc_.allocate_descriptor_count_ + (k_num_flag_elem_bit_ - 1)) / k_num_flag_elem_bit_;
				use_flag_bit_array_.resize(num_use_flag_elem_);
				
				// リセット

				// NOTE:
				//	使用ビットフラグの総ビット数は実際の確保可能サイズよりも大きいため、末尾チェックを簡略化するために事前に末尾の使用不可ビットは1にしておく.
				//	ランタイムでこの部分を0にすると不正な位置にAllocationできると判断されてしまうため上書きしないように！
				// 一旦すべて0クリアした後に端数部を1で埋める.
				std::fill_n(use_flag_bit_array_.data(), num_use_flag_elem_, 0u);
				// 端数
				if (0 < desc_.allocate_descriptor_count_)
				{
					const u32 fraction = desc_.allocate_descriptor_count_ - k_num_flag_elem_bit_ * (num_use_flag_elem_ - 1);
					if (0 < fraction && fraction < k_num_flag_elem_bit_)
					{
						const u32 lower_mask = (1u << fraction) - 1;
						tail_fraction_bit_mask_ = ~lower_mask;

						// 末尾要素の端数部が使用されないようにマスクを書き込み. ここの部分が操作中にゼロになることは発生し得ない. 発生した場合はバグ.
						use_flag_bit_array_[num_use_flag_elem_ - 1] = tail_fraction_bit_mask_;
					}
				}
			}

			// デフォルト利用のためのDescriptorを一つ作成しておく.
			default_persistent_descriptor_ = Allocate();

			return true;
		}
		void PersistentDescriptorAllocator::Finalize()
		{
			Deallocate(default_persistent_descriptor_);

			heap_wrapper_.Finalize();
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
			if (desc_.allocate_descriptor_count_ <= allocation_index)
			{
				std::cout << "PersistentDescriptorAllocator::Allocate: Failed to Allocate" << std::endl;
				return ret;
			}

			// 最後にAllocateしたインデックスを保存(使用ビットフラグ要素インデックスではないので注意)
			last_allocate_index_ = allocation_index;
			// 使用ビットを建てる
			use_flag_bit_array_[find] |= (1 << empty_bit_pos);

			assert(desc_.allocate_descriptor_count_ >= num_allocated_);
			// アロケーション数加算
			++num_allocated_;
			// ---------------------------------------------------------------------------------------------------------------------------------


			// 戻り値セットアップ
			ret.allocator = this;
			ret.allocation_index = allocation_index;
			// ハンドルセット
			ret.cpu_handle = heap_wrapper_.GetCpuHandleStart();// cpu_handle_start_;
			ret.gpu_handle = heap_wrapper_.GetGpuHandleStart();// gpu_handle_start_;
			// アドレスオフセット
			const auto handle_offset = heap_wrapper_.GetHandleIncrementSize() * ret.allocation_index;
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
			assert(v.allocator == this && v.allocation_index < desc_.allocate_descriptor_count_);
			if ((this != v.allocator) || (desc_.allocate_descriptor_count_ <= v.allocation_index))
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
				heap_desc_.Type = desc_.type;
				heap_desc_.NumDescriptors = desc_.allocate_descriptor_count_;
				heap_desc_.NodeMask = 0;
				// このHeap上のDescriptorは描画に利用するためVISIBLE設定. シェーダから可視.
				heap_desc_.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

				if (FAILED(p_device->GetD3D12Device()->CreateDescriptorHeap(&heap_desc_, IID_PPV_ARGS(&p_heap_))))
				{
					std::cout << "[ERROR] Create DescriptorHeap" << std::endl;
					return false;
				}

				handle_increment_size_ = p_device->GetD3D12Device()->GetDescriptorHandleIncrementSize(heap_desc_.Type);
				cpu_handle_start_ = p_heap_->GetCPUDescriptorHandleForHeapStart();
				gpu_handle_start_ = p_heap_->GetGPUDescriptorHandleForHeapStart();
			}

			// フリーレンジセットアップ
			for (int i = 0; i < range_node_pool_.size(); ++i)
			{
				range_node_pool_[i] = {};
				range_node_pool_[i].alloc_group_id = k_invalid_alloc_group_id;
				range_node_pool_[i].start_index = 0;
				range_node_pool_[i].end_index = 0;
				if (i > 0)
				{
					range_node_pool_[i].prev = &range_node_pool_[static_cast<s64>(i) - 1];
				}
				if (i < range_node_pool_.size() - 1)
				{
					range_node_pool_[i].next = &range_node_pool_[static_cast<s64>(i) + 1];
				}
			}
			// フリーノードリスト登録.
			free_node_list_ = &range_node_pool_[0];


			// 初期レンジ全域未使用のためにフリーリストから一つ取得.
			frame_descriptor_range_head_ = free_node_list_;
			free_node_list_ = free_node_list_->next;
			frame_descriptor_range_head_->RemoveFromList();

			// 設定
			frame_descriptor_range_head_->alloc_group_id = k_invalid_alloc_group_id;
			frame_descriptor_range_head_->start_index = 0;
			frame_descriptor_range_head_->end_index = frame_descriptor_range_head_->start_index + (desc_.allocate_descriptor_count_ - 1);


			return true;
		}
		void FrameDescriptorManager::Finalize()
		{
		}
		/*
			フレームに関連付けされたレンジを解放,マージして再利用可能にする

			alloc_group_id : 0,1,2,0,1,2,... のようなframe_flip_index.
		*/
		void FrameDescriptorManager::ResetFrameDescriptor(u32 alloc_group_id)
		{
			if (k_invalid_alloc_group_id == alloc_group_id)
				return;

			// ロック
			std::lock_guard<std::mutex> lock(mutex_);

			auto* range_ptr = frame_descriptor_range_head_;
			for (; range_ptr != nullptr;)
			{
				auto* cur_node = range_ptr;
				range_ptr = range_ptr->next;

				// 対象のフレームインデックスか, 無効インデックスの場合に処理. (無効インデックスの場合は実質マージ処理をするだけ)
				if (alloc_group_id == cur_node->alloc_group_id || k_invalid_alloc_group_id == cur_node->alloc_group_id)
				{
					// フレームインデックスを無効化して再利用可能.
					cur_node->alloc_group_id = k_invalid_alloc_group_id;

					// 一つ前のノードが未使用ノードならマージしてこのノードをフリーリストへ移動.
					auto prev_node = cur_node->prev;
					if (prev_node && cur_node->alloc_group_id == prev_node->alloc_group_id)
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
		bool FrameDescriptorManager::AllocateDescriptorArray(u32 alloc_group_id, u32 count, D3D12_CPU_DESCRIPTOR_HANDLE& alloc_cpu_handle_head, D3D12_GPU_DESCRIPTOR_HANDLE& alloc_gpu_handle_head)
		{
			if (k_invalid_alloc_group_id == alloc_group_id)
				return false;

			// ロック
			std::lock_guard<std::mutex> lock(mutex_);

			// 空きのある未使用ノードを探す
			auto* range_ptr = frame_descriptor_range_head_;
			for (; range_ptr != nullptr; range_ptr = range_ptr->next)
			{
				if (k_invalid_alloc_group_id == range_ptr->alloc_group_id)
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
			// ターゲットのノードを分割して割当. ただし一つ前のノードが同じalloc_group_idの場合はフリーリストから取得せずにマージする.
			if (range_ptr->prev && range_ptr->prev->alloc_group_id == alloc_group_id)
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
				new_node->alloc_group_id = alloc_group_id;
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
			alloc_cpu_handle_head.ptr += (static_cast<UINT64>(handle_increment_size_) * static_cast<UINT64>(alloc_start_index));
			alloc_gpu_handle_head = gpu_handle_start_;
			alloc_gpu_handle_head.ptr += (static_cast<UINT64>(handle_increment_size_) * static_cast<UINT64>(alloc_start_index));

			return true;
		}
		// -------------------------------------------------------------------------------------------------------------------------------------------------


		// -------------------------------------------------------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------------------------------------------------------
		FrameDescriptorAllocInterface::FrameDescriptorAllocInterface()
		{
		}
		FrameDescriptorAllocInterface::~FrameDescriptorAllocInterface()
		{
			Finalize();
		}

		bool FrameDescriptorAllocInterface::Initialize(FrameDescriptorManager* p_manager, const Desc& desc)
		{
			assert(p_manager);
			if (!p_manager)
				return false;
			assert(0 < desc.stack_size);
			if (0 >= desc.stack_size)
				return false;

			desc_ = desc;

			p_manager_ = p_manager;

			// Full扱いでリセット.
			cur_stack_use_count_ = desc_.stack_size;

			cur_stack_cpu_handle_start_ = {};
			cur_stack_gpu_handle_start_ = {};

			return true;
		}
		void FrameDescriptorAllocInterface::Finalize()
		{
			cur_stack_use_count_ = desc_.stack_size;

			p_manager_ = nullptr;
		}
		bool FrameDescriptorAllocInterface::Allocate(u32 alloc_id, u32 count, D3D12_CPU_DESCRIPTOR_HANDLE& alloc_cpu_handle_head, D3D12_GPU_DESCRIPTOR_HANDLE& alloc_gpu_handle_head)
		{
			assert(p_manager_);

			// 許可されていない場合は frame flip index と重複するインデックス範囲を指定することはできない.
			if (!desc_.allow_frame_flip_index && ((k_fdm_frame_flip_index_bitmask & alloc_id) == alloc_id))
			{
				assert(false);
				return false;
			}

			// そもそものスタックサイズよりも大きいサイズを要求されることは想定外.
			assert(desc_.stack_size >= count);
			if (desc_.stack_size < count)
				return false;

			// 現在のスタックが足りない場合はManagerから新規に取得. 連続要素が必要なので.
			if (desc_.stack_size < cur_stack_use_count_ + count)
			{
				// Empty
				cur_stack_use_count_ = 0;

				// idに関連付けてStack分を新規に確保する.
				// CommandList用のフレーム単位自動解放では 0,1,2 のIDを使い毎フレーム自動で解放されるためフレームを跨ぐDescriptorの場合はそのIDを避けて運用する.
				auto alloc_result = p_manager_->AllocateDescriptorArray(alloc_id, desc_.stack_size, cur_stack_cpu_handle_start_, cur_stack_gpu_handle_start_);
				if (alloc_result)
				{
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

			const auto increment_size = p_manager_->GetHandleIncrementSize();
			const auto increment_offset = increment_size * cur_stack_use_count_;

			alloc_cpu_handle_head = cur_stack_cpu_handle_start_;
			alloc_cpu_handle_head.ptr += (increment_offset);
			alloc_gpu_handle_head = cur_stack_gpu_handle_start_;
			alloc_gpu_handle_head.ptr += (increment_offset);

			// スタック消費量更新.
			cur_stack_use_count_ += count;
			return true;
		}
		void FrameDescriptorAllocInterface::Deallocate(u32 alloc_id)
		{
			assert(p_manager_);

			if (!desc_.allow_frame_flip_index && ((k_fdm_frame_flip_index_bitmask & alloc_id) == alloc_id))
			{
				// 許可されていない場合は frame flip index を指定することはできない.
				assert(false);
				return;
			}

			// IDに関連付けられた領域を全開放.
			p_manager_->ResetFrameDescriptor(alloc_id);

			// Full扱いでリセット.
			cur_stack_use_count_ = desc_.stack_size;
		}
		void FrameDescriptorAllocInterface::ResetStack()
		{
			// スタックをフルにしてリセット.
			cur_stack_use_count_ = desc_.stack_size;
		}
		// -------------------------------------------------------------------------------------------------------------------------------------------------
		// 
		// -------------------------------------------------------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------------------------------------------------------
		FrameCommandListDescriptorInterface::FrameCommandListDescriptorInterface()
		{
		}
		FrameCommandListDescriptorInterface::~FrameCommandListDescriptorInterface()
		{
			Finalize();
		}

		bool FrameCommandListDescriptorInterface::Initialize(FrameDescriptorManager* p_manager, const Desc& desc)
		{
			assert(p_manager);
			if (!p_manager)
				return false;
			assert(0 < desc.stack_size);
			if (0 >= desc.stack_size)
				return false;

			desc_ = desc;


			FrameDescriptorAllocInterface::Desc alloc_interface_desc = {};
			alloc_interface_desc.stack_size = desc.stack_size;
			alloc_interface_desc.allow_frame_flip_index = true;	// Frame単位自動解放の対象とするため true.
			alloc_interface_.Initialize(p_manager, alloc_interface_desc);

			// 初回は無効なフレーム
			frame_flip_index_ = ~u32(0);
			
			return true;
		}
		void FrameCommandListDescriptorInterface::Finalize()
		{
			// 念の為フレームインデックスのDescriptorを解放.
			alloc_interface_.Deallocate(frame_flip_index_);
			
			alloc_interface_.Finalize();
		}
		// 新しいフレームの準備
		void FrameCommandListDescriptorInterface::ReadyToNewFrame(u32 frame_flip_index)
		{
			// Frame ID での確保領域は FrameDescriptorManager のフレーム開始処理で自動的に前回フレームが解放される.
			// そのためここではスタックのリセットだけをする.
			alloc_interface_.ResetStack();

			// フレームインデックス更新
			// Raytraceリソースなどが同じマネージャに対して固有IDでアロケートするため, Frame側は多くても3bitまでしか使わない, 使えないものとする.
			const auto safe_frame_flip_index = (frame_flip_index & k_fdm_frame_flip_index_bitmask);
			assert(safe_frame_flip_index == frame_flip_index);

			frame_flip_index_ = safe_frame_flip_index;
		}
		bool FrameCommandListDescriptorInterface::Allocate(u32 count, D3D12_CPU_DESCRIPTOR_HANDLE& alloc_cpu_handle_head, D3D12_GPU_DESCRIPTOR_HANDLE& alloc_gpu_handle_head)
		{
			// フレームフリップ番号で確保.
			if (alloc_interface_.Allocate(frame_flip_index_, count, alloc_cpu_handle_head, alloc_gpu_handle_head))
			{
			}
			else
			{
				assert(false);
				return false;
			}

			return true;
		}
		// -------------------------------------------------------------------------------------------------------------------------------------------------


		// -------------------------------------------------------------------------------------------------------------------------------------------------
		FrameDescriptorHeapPagePool::FrameDescriptorHeapPagePool()
		{
		}
		FrameDescriptorHeapPagePool::~FrameDescriptorHeapPagePool()
		{
			Finalize();
		}

		bool FrameDescriptorHeapPagePool::Initialize(DeviceDep* p_device)
		{
			assert(p_device);
			if (!p_device)
				return false;

			p_device_ = p_device;

			constexpr auto i_srv = GetHeapTypeIndex(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			constexpr auto i_sampler = GetHeapTypeIndex(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

			// CBV_SRV_UAV.
			page_size_[i_srv] = 4096;
			// SAMPLER.
			page_size_[i_sampler] = k_max_sampler_heap_handle_count;

			handle_increment_size_[i_srv] = p_device_->GetD3D12Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			handle_increment_size_[i_sampler] = p_device_->GetD3D12Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

			return true;
		}
		void FrameDescriptorHeapPagePool::Finalize()
		{
		}

		// ページを割り当て.
		ID3D12DescriptorHeap* FrameDescriptorHeapPagePool::AllocatePage(D3D12_DESCRIPTOR_HEAP_TYPE t)
		{
			assert(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV == t || D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER == t);

			const auto type_index = GetHeapTypeIndex(t);

			// HeapType毎にLock.
			std::lock_guard<std::mutex> lock(mutex_[type_index]);

			// タイミングで返却リストのものを処理する.
			{
				const auto func_check_fence_complete = [](DeviceDep* p_device, u64 retired_frame)
				{
					const auto frame_index = p_device->GetDeviceFrameIndex();
					if (frame_index == retired_frame)
						return false;

					const auto max_index = ~u64(0);
					const auto frame_diff = (frame_index > retired_frame) ? (frame_index - retired_frame) : (max_index - retired_frame + (frame_index + 1));
					return (2 <= frame_diff);
				};

				// 返却Queueの先頭から経過フレームチェックで再利用可能なものをAvailableに移動.
				while (!retired_pool_[type_index].empty() && func_check_fence_complete(p_device_, retired_pool_[type_index].front().first))
				{
					available_pool_[type_index].push(retired_pool_[type_index].front().second);
					retired_pool_[type_index].pop();
				}
			}

			// 割り当てられるものがあるならそれを返す.
			if (!available_pool_[type_index].empty())
			{
				auto lend_object = available_pool_[type_index].front();
				available_pool_[type_index].pop();
				return lend_object;
			}
			else
			{
				// なければ新規に作成
				D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
				heap_desc.Type = t;
				heap_desc.NumDescriptors = page_size_[type_index];
				heap_desc.NodeMask = 0;
				// このHeap上のDescriptorは描画に利用するためVISIBLE設定. シェーダから可視.
				heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

				CComPtr<ID3D12DescriptorHeap> p_heap;
				if (FAILED(p_device_->GetD3D12Device()->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&p_heap))))
				{
					std::cout << "[ERROR] Create DescriptorHeap" << std::endl;
					return nullptr;
				}
				// 生成したHeapをすべてスマートポインタで保持しておく.
				created_pool_[type_index].push_back(p_heap);

				// 確保したものを返す.
				return p_heap.p;
			}
		}

		void FrameDescriptorHeapPagePool::DeallocatePage(D3D12_DESCRIPTOR_HEAP_TYPE t, ID3D12DescriptorHeap* p_heap)
		{
			assert(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV == t || D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER == t);

			const auto type_index = GetHeapTypeIndex(t);
			const auto frame_index = p_device_->GetDeviceFrameIndex();

			// HeapType毎にLock.
			std::lock_guard<std::mutex> lock(mutex_[type_index]);

			retired_pool_[type_index].push(std::make_pair(frame_index, p_heap));
		}


		u32 FrameDescriptorHeapPagePool::GetPageSize(D3D12_DESCRIPTOR_HEAP_TYPE t) const
		{
			assert(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV == t || D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER == t);

			return page_size_[GetHeapTypeIndex(t)];
		}
		u32	FrameDescriptorHeapPagePool::GetHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE t) const
		{
			assert(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV == t || D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER == t);

			return handle_increment_size_[GetHeapTypeIndex(t)];
		}
		// -------------------------------------------------------------------------------------------------------------------------------------------------


		// -------------------------------------------------------------------------------------------------------------------------------------------------
		FrameDescriptorHeapPageInterface::FrameDescriptorHeapPageInterface()
		{
		}
		FrameDescriptorHeapPageInterface::~FrameDescriptorHeapPageInterface()
		{
			Finalize();
		}

		bool FrameDescriptorHeapPageInterface::Initialize(FrameDescriptorHeapPagePool* p_pool, const Desc& desc)
		{
			assert(p_pool);
			if (!p_pool)
				return false;

			assert(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV == desc.type || D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER == desc.type);
			// CBV_SRV_UAVとSAMPLERのみ対応.
			if (!(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV == desc.type || D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER == desc.type))
				return false;

			p_pool_ = p_pool;
			desc_ = desc;
			use_handle_count_ = 0;

			return true;
		}
		void FrameDescriptorHeapPageInterface::Finalize()
		{
			if (p_cur_heap_)
			{
				// 現在のPageを返却.
				p_pool_->DeallocatePage(desc_.type, p_cur_heap_);
				p_cur_heap_ = nullptr;
			}
		}
		u32	FrameDescriptorHeapPageInterface::GetHandleIncrementSize() const
		{
			return p_pool_->GetHandleIncrementSize(desc_.type);
		}
		bool FrameDescriptorHeapPageInterface::Allocate(u32 handle_count, D3D12_CPU_DESCRIPTOR_HANDLE& alloc_cpu_handle_head, D3D12_GPU_DESCRIPTOR_HANDLE& alloc_gpu_handle_head)
		{
			assert(p_pool_);

			// Pageが確保されていないか, 必要個数が確保できない場合はPageを新規取得.
			if (!p_cur_heap_ || (use_handle_count_ + handle_count > p_pool_->GetPageSize(desc_.type)))
			{
				if (p_cur_heap_)
				{
					// 現在のPageを返却.
					p_pool_->DeallocatePage(desc_.type, p_cur_heap_);
					p_cur_heap_ = nullptr;
				}

				// 新規Page.
				p_cur_heap_ = p_pool_->AllocatePage(desc_.type);
				assert(p_cur_heap_);
				if (!p_cur_heap_)
				{
					return false;
				}
				// 各種リセット.
				use_handle_count_ = 0;
				cur_cpu_handle_start_ = p_cur_heap_->GetCPUDescriptorHandleForHeapStart();
				cur_gpu_handle_start_ = p_cur_heap_->GetGPUDescriptorHandleForHeapStart();
			}

			// Handleを確保.
			const auto increment_size = p_pool_->GetHandleIncrementSize(desc_.type);
			// 使用範囲の末端までオフセットして割り当てる.
			const auto increment_offset = increment_size * use_handle_count_;

			// ページ先頭から位置までオフセットして返す.
			alloc_cpu_handle_head = cur_cpu_handle_start_;
			alloc_cpu_handle_head.ptr += (increment_offset);
			alloc_gpu_handle_head = cur_gpu_handle_start_;
			alloc_gpu_handle_head.ptr += (increment_offset);

			// 確保分だけ使用数加算.
			use_handle_count_ += handle_count;

			return true;
		}
	}
}