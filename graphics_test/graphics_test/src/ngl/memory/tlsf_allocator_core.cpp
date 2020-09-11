
#include "ngl/memory/tlsf_allocator_core.h"
#include "ngl/util/bit_operation.h"

#include <iostream>
#ifdef _DEBUG
#include <assert.h>
#endif

namespace ngl
{
	namespace memory
	{
		TlsfAllocatorCore::TlsfAllocatorCore()
			: head_(nullptr)
			, size_(0)
			, secondLevelExponentiation_(0)
			, freeListBitFli_(0)
		{
			memset(freeListBitSli_, 0x00, sizeof(freeListBitSli_));
			memset(freeList_, 0x00, sizeof(freeList_));
		}
		TlsfAllocatorCore::~TlsfAllocatorCore()
		{
		}
		bool TlsfAllocatorCore::Initialize(void* mem, u64 size, u32 secondLevelExponentiation)
		{
			// 先頭・末尾・最初のフリーリスト要素　の3つ分の最低限必要なサイズを計算
			u32 manageInfoDataSize = BoundaryTagBlock::GetAppendInfoDataSize() * 3;
			// 第二レベルの分割数に応じた最小のデータ部サイズ

			u32 sliExp = SECOND_LEVEL_INDEX_EXP_MAX < secondLevelExponentiation
				? SECOND_LEVEL_INDEX_EXP_MAX : secondLevelExponentiation;

			u32 minDataSize = 1;
			for (u32 i = 0; i < sliExp; ++i)
				minDataSize *= 2;

			if (nullptr == mem || (manageInfoDataSize + minDataSize) > size || 0 >= sliExp)
				return false;
			head_ = mem;
			size_ = size;
			secondLevelExponentiation_ = sliExp;

			// 最初と最後にダミーのタグを仕込む
			headTag_ = BoundaryTagBlock::Placement(mem, 0);// データサイズ0で先頭に配置
			// データサイズ0で末尾に配置 ヘッドと同じサイズなので流用
			tailTag_ = BoundaryTagBlock::Placement(mem, 0, static_cast<u32>(size - headTag_->GetAllSize()));

			// マージしないためにダミーはフラグをON
			headTag_->SetIsUsed(true);
			tailTag_->SetIsUsed(true);

			// フラグをリセット
			freeListBitFli_ = 0;
			memset(freeListBitSli_, 0x00, sizeof(freeListBitSli_));
			memset(freeList_, 0x00, sizeof(freeList_));


			// 残った部分を最初の唯一のブロックとする
			// 残ったサイズは先頭、末尾とこのブロック自体の管理情報サイズを除いたもの
			u32 firstDataSize = static_cast<u32>(size)-headTag_->GetAllSize() - tailTag_->GetAllSize() - BoundaryTagBlock::GetAppendInfoDataSize();
			BoundaryTagBlock* firstBlock = BoundaryTagBlock::Placement(mem, firstDataSize, headTag_->GetAllSize());

			RegisterFreeList(firstBlock);

			return true;
		}
		// 解放
		//		特に削除処理はしないが管理用情報などをクリア
		void TlsfAllocatorCore::destroy()
		{
			head_ = nullptr;
			size_ = 0;
			secondLevelExponentiation_ = 0;
			freeListBitFli_ = 0;
			memset(freeListBitSli_, 0x00, sizeof(freeListBitSli_));
			memset(freeList_, 0x00, sizeof(freeList_));
		}

		// メモリリークのチェック
		// 標準出力にリーク情報を出力する
		void TlsfAllocatorCore::leakReport()
		{
			// 管理メモリ先頭からひとつづつブロックをたどって行って使用中のものがあれば出力
			u8* cur = reinterpret_cast<u8*>(head_);
			u8* end = cur + size_;
			for (; cur < end ;)
			{
				BoundaryTagBlock* block = reinterpret_cast<BoundaryTagBlock*>(cur);

				// データサイズが0のものは管理用の先頭と終端タグなので無視
				if (0 < block->GetDataSize() && block->IsUsed())
				{
					std::cout << "[TlsfAllocatorCore] Memory Leak !" << std::endl;
					std::cout << "	dataSize : " << block->GetDataSize() << std::endl;
				}

				cur = cur + block->GetAllSize();
			}
		}



		// 割り当て
		void* TlsfAllocatorCore::Allocate(u64 size)
		{
			// FLI-SLIに一致するフリーブロックがあればそれを割り当て
			// 無ければビットフラグチェックをして大き目のブロックを割り当てる
			// サイズが前方タグ一つ後方タグ一つ分＋最小割り当てメモリサイズ　を残して分割できる場合は
			// フリーブロックを分割して残った部分を適切なフリーリストに登録する
			if (0 == size)
				return NULL;

			// 強制的に最小サイズ以上にする
			const u32 minSize = 1 << secondLevelExponentiation_;
			size = size < minSize ? minSize : size;

			s32 fli = GetFirstLevelIndex(size);
			s32 sli = GetSecondLevelIndex(size, fli, secondLevelExponentiation_);

			if (0 != (freeListBitFli_ & ((~u64(0x00)) << fli)))
			{
				// 第一インデックスに空きあり
				if (0 != (freeListBitSli_[fli] & (u64(1) << sli)))
				{
					// 第二インデックスに一致するフリーブロック発見
					BoundaryTagBlock* block = RemoveFreeListTop(fli, sli);

					// フリーリストから取り出して利用状態になっているブロックの管理も必要だろうか？

					// 使用状態にする
					block->SetIsUsed(true);

					// 返す
					return block->GetDataPtr();
				}
				else
				{
					// 第二インデックスに空きがなかったので同一第一カテゴリでの空きを探す
					s32 upLevelFli = GetFreeListFirstLevelIndex(fli);
					if (0 <= upLevelFli)
					{
						s32 uplevelSli = GetFreeListSecondLevelIndex(upLevelFli);
						if (0 <= uplevelSli)
						{
							// 第二インデックスに一致するフリーブロック発見
							// ついでに　RemoveFreeListTop　の中でフリーリストビットの操作もされている
							BoundaryTagBlock* block = RemoveFreeListTop(upLevelFli, uplevelSli);

							// アロケート時間の30％くらいがここの分割？
							// 分割できるなら分割
							BoundaryTagBlock* divBlock = DivideBlock(block, static_cast<u32>(size));
							if (NULL != divBlock)
							{
								RegisterFreeList(divBlock);
							}

							// 使用状態にする
							block->SetIsUsed(true);

							// 返す
							return block->GetDataPtr();
						}
					}
				}
			}

			return NULL;
		}

		// 割り当て解除
		bool TlsfAllocatorCore::Deallocate(void* mem)
		{
			if (NULL == mem)
				return false;
			// 渡されたメモリの前方 sizeof(BoundaryTagBlock) の位置にブロックが配置されているはずなので
			// それの使用フラグをfalseとして割り当て解除する
			// さらにブロックの前後をチェックして未割当ブロックがある場合はそれらとマージしてフリーリストに登録する

			u8* dataHead = reinterpret_cast<u8*>(mem);

			// 面倒なことしているがこれはアライメントのためにブロックの先頭のすぐ後にデータ部がない場合があるため

			// 直前の4バイトにデータ部サイズがある
			u32 dataSize = *reinterpret_cast<u32*>(dataHead - sizeof(u32));

			// データ部の先頭からデータ部サイズ分オフセットした位置にブロックサイズがある
			u32 blockSize = *reinterpret_cast<u32*>(dataHead + dataSize);

			// データサイズ + データサイズの後ろのブロックサイズ部u32 - ブロックサイズ でブロックの先頭が得られる
			s64 offsetToBlock = s64(dataSize) + BoundaryTagBlock::GetTailTagSize() - s64(blockSize);
			BoundaryTagBlock* block = reinterpret_cast<BoundaryTagBlock*>(dataHead + offsetToBlock);

			u8* blockHead = reinterpret_cast<u8*>(block);

			//前方ブロック
			BoundaryTagBlock* prevBlock = reinterpret_cast<BoundaryTagBlock*>(
				blockHead - *reinterpret_cast<u32*>(blockHead - sizeof(u32))
				);
			// 後方ブロック
			BoundaryTagBlock* nextBlock = reinterpret_cast<BoundaryTagBlock*>(
				blockHead + blockSize
				);

			BoundaryTagBlock* mergeHead = block;
			u32 mergeBlockSize = blockSize;
			if (!prevBlock->IsUsed())
			{
				// 前方ブロックをフリーリストから除去
				RemoveFreeList(prevBlock);
				// 
				mergeBlockSize += prevBlock->GetAllSize();

				// 前方ブロックがマージ対象ならマージの先頭を前方ブロックにする
				mergeHead = prevBlock;
			}
			if (!nextBlock->IsUsed())
			{
				// 後方ブロックをフリーリストから除去
				RemoveFreeList(nextBlock);
				// 
				mergeBlockSize += nextBlock->GetAllSize();
			}

			// マージ
			block = BoundaryTagBlock::Placement(mergeHead, mergeBlockSize - BoundaryTagBlock::GetAppendInfoDataSize());

			// マージが必要なら完了したはずなのでフリーリストに登録
			block->SetIsUsed(false);
			RegisterFreeList(block);

			return true;
		}

		bool TlsfAllocatorCore::RegisterFreeList(BoundaryTagBlock* block)
		{
			s32 fli = GetFirstLevelIndex(block->GetDataSize());
			s32 sli = GetSecondLevelIndex(block->GetDataSize(), fli, secondLevelExponentiation_);

			//リストに挿入
			if (nullptr == freeList_[fli][sli])
			{
				freeList_[fli][sli] = block;
			}
			else
			{
				freeList_[fli][sli]->InsertNext(block);
			}

			// フリーリストビット操作
			freeListBitFli_ |= (u64(1) << fli);

			freeListBitSli_[fli] |= (1 << sli);

			return true;
		}
		BoundaryTagBlock* TlsfAllocatorCore::RemoveFreeList(BoundaryTagBlock* block)
		{
			//	if (NULL == block)
			//		return NULL;

			s32 fli = GetFirstLevelIndex(block->GetDataSize());
			s32 sli = GetSecondLevelIndex(block->GetDataSize(), fli, secondLevelExponentiation_);

			if (0 != (freeListBitFli_&(u64(1) << fli)) && 0 != (freeListBitSli_[fli] & (u64(1) << sli)))
			{
				// フリーリストに直接保持されている場合はNextを登録して自分を除去
				if (freeList_[fli][sli] == block)
				{
					freeList_[fli][sli] = block->GetNext();
				}
				block->Remove();

				// ビットマスクを操作
				// NULLの場合にマスク
			//	freeListBitSli_[fli] &= ~(u32(NULL == freeList_[fli][sli]) << sli);
			//	freeListBitFli_ &= ~(u32(0 == freeListBitSli_[fli]) << fli);
				freeListBitSli_[fli] &= ~(u64(NULL == freeList_[fli][sli]) << sli);
				freeListBitFli_ &= ~(u64(0 == freeListBitSli_[fli]) << fli);

				return block;
			}
			return NULL;
		}
		BoundaryTagBlock* TlsfAllocatorCore::RemoveFreeListTop(u32 fli, u32 sli)
		{
			return RemoveFreeList(freeList_[fli][sli]);
		}
		// block分割可能なら分割して残りの部分を　divBlock　に
		BoundaryTagBlock* TlsfAllocatorCore::DivideBlock(BoundaryTagBlock* block, u32 size)
		{
			u32 startDataSize = block->GetDataSize();

			u32 needSize = size + BoundaryTagBlock::GetHeadTagSize() + BoundaryTagBlock::GetTailTagSize();

			//　そもそもデータ部に別のブロックが入るのか
			if (startDataSize < needSize)
			{
				return NULL;
			}
			// startDataSize > 分割に必要な最小限サイズが分かったところで分割で残る部分のサイズをチェック
			u32 divDataSize = startDataSize - needSize;

			// 残る部分のサイズが最小分割サイズある?(第二レベルインデックスの最小分割)
			if (SECOND_LEVEL_INDEX_MAX > divDataSize)
			{
				return NULL;
			}

			// 先頭をリプレース
			block = BoundaryTagBlock::Placement(block, size);

			BoundaryTagBlock* divBlock = BoundaryTagBlock::Placement(block, divDataSize, block->GetAllSize());
			return divBlock;
		}

		s32 TlsfAllocatorCore::GetFirstLevelIndex(u64 requireSize)
		{
			return MostSignificantBit64(requireSize);
		}
		// 第二レベル
		s32 TlsfAllocatorCore::GetSecondLevelIndex(u64 requireSize, u32 fli, u32 secondLevelExp)
		{
			//	return (requireSize & ((1 << fli) - 1)) >> (fli - secondLevelExp);

			// 最上位ビット未満のビット列だけを有効にするマスク
			const unsigned mask = (1 << fli) - 1;   // 1000 0000 -> 0111 1111

			// 右へのシフト数を算出
			const unsigned rs = fli - secondLevelExp;    // 7 - 3 = 4 （8分割ならN=3です）

			// 引数sizeにマスクをかけて、右へシフトすればインデックスに
			return (requireSize & mask) >> rs;
		}

		// フリーリストで指定レベルより大きい最小のFLIを返す
		s32 TlsfAllocatorCore::GetFreeListFirstLevelIndex(u32 fli)
		{
			u32 all1 = ~u32(0x00);
			u32 mask = all1 << fli;
			u32 find = mask & freeListBitFli_;
			return LeastSignificantBit64(find);
		}
		// フリーリストで指定したFLI内で最小のSLIを返す
		s32 TlsfAllocatorCore::GetFreeListSecondLevelIndex(u32 fli)
		{
			s32 sli = LeastSignificantBit64(freeListBitSli_[fli]);
			return sli;
		}
	}
}