#pragma once

#ifndef _NGL_BOUNDARY_TAG_BLOCK_
#define _NGL_BOUNDARY_TAG_BLOCK_


#include "ngl/util/types.h"
#include <new>

namespace ngl
{

	class BoundaryTagBlock
	{
	public:
		BoundaryTagBlock();
		BoundaryTagBlock(u8* data, u32 size);
		~BoundaryTagBlock();

		// 初期化
		void Initialize(u8* data, u32 size);

		// データ部
		u8* GetDataPtr() const;

		// ブロック占有サイズの格納されたポインタ
		u32* GetTailTagPtr() const;

		// オブジェクトが占有する全サイズを取得
		u32 GetAllSize() const;

		// データ部のサイズ
		u32 GetDataSize() const;


		/*
		// メモリ管理用に付加するデータのサイズ　データ部のサイズ以外に必要な部分のサイズ
		// 先頭と末尾のタグサイズの合計
		static u32 GetAppendInfoDataSize();
		// メモリ管理用の先頭タグサイズ
		static u32 GetHeadTagSize();
		// メモリ管理用の末端タグサイズ
		static u32 GetTailTagSize();
		*/
		// メモリ管理用に付加するデータのサイズ　データ部のサイズ以外に必要な部分のサイズ
		// 先頭と末尾のタグサイズの合計
		static inline u32 GetAppendInfoDataSize()
		{
			return GetHeadTagSize() + GetTailTagSize();
		}

		// メモリ管理用の先頭タグサイズ
		static inline u32 GetHeadTagSize()
		{
			return sizeof(BoundaryTagBlock)+sizeof(u32); // サイズ部もずらした
		}
		// メモリ管理用の末端タグサイズ
		static inline u32 GetTailTagSize()
		{
			return sizeof(u32);
		}




		void SetIsUsed( bool f );
		bool IsUsed() const;

		// 渡されたオブジェクトを自身の後ろに追加する
		BoundaryTagBlock* InsertNext(BoundaryTagBlock* newObject);
		// 渡されたオブジェクトを自身の前に追加する
		BoundaryTagBlock* InsertPrev(BoundaryTagBlock* newObject);
		// リストから離脱
		BoundaryTagBlock* Remove();

		BoundaryTagBlock* GetNext() const;
		BoundaryTagBlock* GetPrev() const;

	//	template<typename T>
	//	// 指定されたメモリにBoundaryTagオブジェクトを配置して初期化しそのポインタとして返す
	//	static BoundaryTagBlock* Placement(T* placementMemory, u32 dataSize, u32 offset = 0);

		// 指定されたメモリにBoundaryTagオブジェクトを配置して初期化しそのポインタとして返す
		static BoundaryTagBlock* Placement(void* placementMemory, u32 dataSize, u32 offset = 0);

	protected:
		u8*		dataHeadPtr_;// データ部の先頭へのポインタ

		BoundaryTagBlock* prev_;// 双方向リスト用
		BoundaryTagBlock* next_;

		// データサイズ部へのポインタ
		u32*	dataSizeTagPtr_;
		u8		isUsed_;// 使用フラグ
	};

	/*
	template<typename T>
	// 指定されたメモリにBoundaryTagオブジェクトを配置して初期化しそのポインタとして返す
	BoundaryTagBlock* BoundaryTagBlock::Placement(T* placementMemory, u32 dataSize, u32 offset)
	{
		void* m = reinterpret_cast<void*>( placementMemory );

		BoundaryTagBlock* obj = new(m)
			BoundaryTagBlock(reinterpret_cast<u8*>(m) + offset + sizeof(BoundaryTagBlock), dataSize);
		return obj;
	}
	*/

}

#endif // _NGL_BOUNDARY_TAG_BLOCK_