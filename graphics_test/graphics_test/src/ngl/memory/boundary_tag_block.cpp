
#include "boundary_tag_block.h"

namespace ngl
{

	// 指定されたメモリにBoundaryTagオブジェクトを配置して初期化しそのポインタとして返す
	BoundaryTagBlock* BoundaryTagBlock::Placement(void* placementMemory, u32 dataSize, u32 offset)
	{
		u8* m = reinterpret_cast<u8*>(placementMemory);

		u8* headPtr = m + offset;

		// アライメント対応はここでやる 
		BoundaryTagBlock* obj = new(headPtr)BoundaryTagBlock(headPtr + GetHeadTagSize(), dataSize);
		return obj;
	}

	BoundaryTagBlock::BoundaryTagBlock()
		: 
		dataHeadPtr_(nullptr)

		, isUsed_(false)
		, prev_(nullptr)
		, next_(nullptr)
	{
	}

	BoundaryTagBlock::BoundaryTagBlock(u8* data, u32 size)
	{
		Initialize(data, size);
	}

	BoundaryTagBlock::~BoundaryTagBlock()
	{
	}

	void BoundaryTagBlock::Initialize(u8* data, u32 size)
	{
		dataHeadPtr_ = data;
	
		// データ部の前方4バイトにサイズが書き込まれる
		dataSizeTagPtr_ = reinterpret_cast<u32*>( data - sizeof(u32) );
		(*dataSizeTagPtr_) = size;
	
		u32* endTag = GetTailTagPtr();
		*endTag = GetAppendInfoDataSize() + GetDataSize();

		isUsed_ = false;
		prev_ = next_ = nullptr;
	}

	u8* BoundaryTagBlock::GetDataPtr() const
	{
		return dataHeadPtr_;
	}

	// ブロック占有サイズの格納されたポインタ
	u32* BoundaryTagBlock::GetTailTagPtr() const
	{
		return reinterpret_cast<u32*>(dataHeadPtr_ + GetDataSize());
	}

	/*
	// メモリ管理用に付加するデータのサイズ　データ部のサイズ以外に必要な部分のサイズ
	// 先頭と末尾のタグサイズの合計
	u32 BoundaryTagBlock::GetAppendInfoDataSize()
	{
		return GetHeadTagSize() + GetTailTagSize();
	}

	// メモリ管理用の先頭タグサイズ
	u32 BoundaryTagBlock::GetHeadTagSize()
	{
		return sizeof(BoundaryTagBlock) + sizeof(u32); // サイズ部もずらした
	}
	// メモリ管理用の末端タグサイズ
	u32 BoundaryTagBlock::GetTailTagSize()
	{
		return sizeof(u32);
	}
	*/


	// データ部のサイズ
	u32 BoundaryTagBlock::GetDataSize() const
	{
		return *dataSizeTagPtr_;
	}
	// オブジェクトが占有する全サイズを取得
	u32 BoundaryTagBlock::GetAllSize() const
	{
		return *GetTailTagPtr();
	}

	void BoundaryTagBlock::SetIsUsed(bool f)
	{
		isUsed_ = f;
	}
	bool BoundaryTagBlock::IsUsed() const
	{
		return 0!=isUsed_;
	}

	// 渡されたオブジェクトを自身の後ろに追加する
	BoundaryTagBlock* BoundaryTagBlock::InsertNext(BoundaryTagBlock* newObject)
	{
		newObject->next_ = next_;
		newObject->prev_ = this;
		if (nullptr != next_)
			next_->prev_ = newObject;
		next_ = newObject;
		return this;
	}
	// 渡されたオブジェクトを自身の前に追加する
	BoundaryTagBlock* BoundaryTagBlock::InsertPrev(BoundaryTagBlock* newObject)
	{
		newObject->next_ = this;
		newObject->prev_ = prev_;
		if (nullptr != prev_)
			prev_->next_ = newObject;
		prev_ = newObject;
		return this;
	}
	// リストから離脱
	BoundaryTagBlock* BoundaryTagBlock::Remove()
	{
		if (nullptr != next_)
			next_->prev_ = prev_;
		if (nullptr != prev_)
			prev_->next_ = next_;
		prev_ = next_ = nullptr;
		return this;
	}

	BoundaryTagBlock* BoundaryTagBlock::GetNext() const
	{
		return next_;
	}
	BoundaryTagBlock* BoundaryTagBlock::GetPrev() const
	{
		return prev_;
	}
}