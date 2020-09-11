#pragma once
#ifndef _NGL_TLSF_MEMORY_POOL_
#define _NGL_TLSF_MEMORY_POOL_

/*
	TLSFにより動作するメモリプール
	確保メモリはDeleterを指定したスマートポインタとして返される


	// tlslメモリプールテスト
	{
	#define TLSF_POOL_HEAP_MEM___

	ngl::memory::TlsfMemoryPool memPool;

	#ifdef TLSF_POOL_HEAP_MEM
	// 外部メモリ確保
	const ngl::u32 bSize = 32 * 1024 * 1024;
	struct MemHolder
	{
	ngl::u8 data_[bSize];
	};
	ngl::SharedPtr<MemHolder> heapMem(new MemHolder());

	memPool.initialize(heapMem.get(), bSize);
	#else
	// 内部メモリ確保
	memPool.initialize( 32 * 1024 * 1024 );
	#endif
	const ngl::u32 ALLOCATE_LOOP = 1000000;
	ngl::time::Timer::instance().startTimer("memory_pool_1byte");
	for (ngl::u32 i = 0; i < ALLOCATE_LOOP; ++i)
	{
	ngl::memory::TlsfMemoryPtr<ngl::u8> ptrPool1 = memPool.allocate<ngl::u8>(1);
	}
	std::cout << "memory_pool_1byte Sec   = " << ngl::time::Timer::instance().getElapsedSec("memory_pool_1byte") << std::endl;

	ngl::time::Timer::instance().startTimer("memory_pool_64byte");
	for (ngl::u32 i = 0; i < ALLOCATE_LOOP; ++i)
	{
	ngl::memory::TlsfMemoryPtr<ngl::math::Mtx44> ptrPool0 = memPool.allocate<ngl::math::Mtx44>(1);
	}
	std::cout << "memory_pool_64byte Sec   = " << ngl::time::Timer::instance().getElapsedSec("memory_pool_64byte") << std::endl;

	ngl::time::Timer::instance().startTimer("memory_pool_1024byte");
	for (ngl::u32 i = 0; i < ALLOCATE_LOOP; ++i)
	{
	ngl::memory::TlsfMemoryPtr<ngl::math::Mtx44[16]> ptrPool0 = memPool.allocate<ngl::math::Mtx44[16]>(1);
	}
	std::cout << "memory_pool_1024byte Sec   = " << ngl::time::Timer::instance().getElapsedSec("memory_pool_1024byte") << std::endl;

	ngl::memory::TlsfMemoryPtr<NoDefalutConstructorClass> ptrPool00;

	ngl::memory::TlsfMemoryPtr<NoDefalutConstructorClass> intPoolPtr0 = memPool.allocate<NoDefalutConstructorClass>();
	intPoolPtr0->x_ = 11;

	ngl::memory::TlsfMemoryPtr<NoDefalutConstructorClass> intPoolPtr1 = memPool.allocate<NoDefalutConstructorClass>();
	intPoolPtr1->x_ = 22;

	ptrPool00 = intPoolPtr0;

	// intPoolPtr0 が格納していたメモリは解放されて新しく確保されたメモリを管理する
	intPoolPtr0 = memPool.allocate<NoDefalutConstructorClass>();
	intPoolPtr0->x_ = 33;
	}

	
*/

#include "ngl/util/types.h"
#include "ngl/memory/tlsf_allocator_core.h"

namespace ngl
{

	namespace memory
	{
		class TlsfMemoryPool;

		//--------------------------------------------------------------------------
		// 参照カウンタ基底
		class TlsfMemoryPtrRefCountCoreBase
		{
		public:
			TlsfMemoryPtrRefCountCoreBase()
				: count_(1)
			{
			}
			// 継承先でオーバーライド
			virtual ~TlsfMemoryPtrRefCountCoreBase()
			{
			}
			// 参照カウント加算
			void addRef()
			{
				++count_;
			}
			bool release()
			{
				if (--count_ == 0)
				{
					// 管理ポインタの実削除処理
					dispose();

					// 参照カウンタの削除
					//	delete this;
					// 真が返るとカウンタを削除する
					return true;
				}
				return false;
			}
			// 実削除処理
			virtual void dispose() = 0;

			virtual void* get() const = 0;
			virtual u32 size() const = 0;

		private:
			s32 count_;
		};

		//--------------------------------------------------------------------------
		// 参照カウンタ実装その二 Deleter指定
		template<typename T, typename DeleterT>
		class TlsfMemoryPtrRefCountImpl
			: public TlsfMemoryPtrRefCountCoreBase
		{
		public:
			TlsfMemoryPtrRefCountImpl(T* ptr, u32 size, DeleterT deleter)
				: ptr_(ptr), size_(size), deleter_(deleter)
			{
			}
			void dispose()
			{
				// Deleterで削除
				deleter_(ptr_);
			}
			void* get() const
			{
				return ptr_;
			}
			u32 size() const
			{
				return size_;
			}
		private:
			T* ptr_;
			u32 size_;
			DeleterT deleter_;
		};

		//--------------------------------------------------------------------------
		// 参照カウンタ
		class TlsfMemoryPtrRefCount
		{
		public:
			
			TlsfMemoryPtrRefCount()
				: count_(NULL)
				, allocator_(NULL)
			{
			}
			
			// デリータ有り
			template<typename T, typename DeleterT>
			TlsfMemoryPtrRefCount(T* ptr, u32 size, DeleterT deleter, TlsfAllocatorCore* allocator)
			{
				//count_ = new TlsfMemoryPtrRefCountImpl<T, DeleterT>(ptr, size, deleter);

				// アロケート
				allocator_ = allocator;
				void* mem = allocator_->Allocate(sizeof(TlsfMemoryPtrRefCountImpl<T, DeleterT>));
				count_ = new(mem) TlsfMemoryPtrRefCountImpl<T, DeleterT>(ptr, size, deleter);
			}

			~TlsfMemoryPtrRefCount()
			{
				// カウンタ減算して0なら解放
				releaseCount();
			}

			// 参照加算
			TlsfMemoryPtrRefCount(TlsfMemoryPtrRefCount const& sc)
				: count_(sc.count_)
				, allocator_(sc.allocator_)
			{
				if (count_)
					count_->addRef();
			}

			void releaseCount()
			{
				if (count_)
				{
					// カウンタデクリメント
					// デクリメントした結果が 0 なら真が返るので解放する
					if (count_->release())
					{
						// デアロケート
						allocator_->Deallocate(count_);
						count_ = NULL;
						allocator_ = NULL;
					}
				}
			}
			// 既存をデクリメント、ソース側をインクリメント
			TlsfMemoryPtrRefCount& operator=(TlsfMemoryPtrRefCount const& sc)
			{
				if (sc.count_ != count_)
				{
					if (sc.count_)
						sc.count_->addRef();

					// カウンタ減算して0なら解放
					releaseCount();

					count_ = sc.count_;
					allocator_ = sc.allocator_;
				}
				return *this;
			}
			// 参照取得
			template<typename T>
			T* get() const
			{
				if (count_)
					return static_cast<T*>(count_->get());
				else
					return NULL;
			}
			u32 size() const
			{
				if (count_)
					return count_->size();
				else
					return 0;
			}
		private:
			// カウンタ実部
			TlsfMemoryPtrRefCountCoreBase* count_;

			// カウンタのメモリ確保解放用
			TlsfAllocatorCore*				allocator_;
		};

		//--------------------------------------------------------------------------
		// デリータ
		// プールで確保したメモリ解放用
		class TlsfMemoryDeleter
		{
		public:
			TlsfMemoryDeleter(TlsfMemoryPool* pool)
				: pool_(pool)
			{
			}
			// プールに解放要求
			void operator()(void* ptr);
		private:
			// メモリを確保したプールへのポインタ
			TlsfMemoryPool* pool_;
		};


		//--------------------------------------------------------------------------
		//インスタンスの参照を返すための特性
		template < typename T >
		struct TlsfMemoryPtrTraits
		{
			typedef T& reference;
		};

		//--------------------------------------------------------------------------
		//TlsfMemoryPtr< void >の特殊化
		template<>
		struct TlsfMemoryPtrTraits< void >
		{
			typedef void reference;
		};

		//--------------------------------------------------------------------------
		// プールメモリ共有ポインタ
		// そのうち sharedPtr をアロケータ対応して置き換えたい
		template<typename T>
		class TlsfMemoryPtr
		{
		public:
			// 戻り値の型を定義
			typedef typename TlsfMemoryPtrTraits<T>::reference reference;

			// 任意の型のTlsfMemoryPtrにメンバ公開
			template<typename U>
			friend class TlsfMemoryPtr;

			friend class TlsfMemoryPool;

		public:
			TlsfMemoryPtr()
				: count_()
				, ptr_(NULL)
			{}

			// コピーコンストラクタ
			template<typename U>
			TlsfMemoryPtr(TlsfMemoryPtr<U> const& sp)
				: count_(sp.count_)
				, ptr_(sp.ptr_)
			{
			}

			// アクセサ
			u32 size() const
			{
				return count_.size();
			}
			reference operator*() const
			{
				return *ptr_;
			}
			T* operator->() const
			{
				return ptr_;
			}
			T* get() const
			{
				return ptr_;
			}
			T& operator[](u32 i) const
			{
				return ptr_[i];
			}
		private:

			// 構築
			// プールのみがアクセス
			// 参照カウンタ実体の確保と解放のためにアロケータを受け取る
			template<typename U>
			TlsfMemoryPtr(U* ptr, u32 size, const TlsfMemoryDeleter& deleter, TlsfAllocatorCore* allocator)
				: count_(ptr, size, deleter, allocator)
				, ptr_(ptr)
			{
			}

		private:
			TlsfMemoryPtrRefCount	count_;
			T*						ptr_;	// 外部からのアクセス用　count_も同様に保持しており、そちらが参照0で解放に使われる
		};


		//--------------------------------------------------------------------------
		//
		//	TLSFアロケータによるメモリプール
		//		確保メモリは共有ポインタクラス TlsfMemoryPtr<TYPE> で返される
		//		メモリの解放は TlsfMemoryPtr<TYPE> の生成時に設定したDeleterが実行する
		//		Deleterは内部にメモリを確保したプールへのポインタを持ち、そのプールに対して解放要求をする
		//
		//		確保したメモリを解放するためのプールを管理するのが面倒だったので共有ポインタのDeleterに仕込んだというだけ
		//
		class TlsfMemoryPool
		{
		public:
			friend class TlsfMemoryDeleter;

			TlsfMemoryPool();
			~TlsfMemoryPool();

			// 外部から管理メモリを渡す
			// 解放責任は外
			bool initialize(void* manageMemory, u32 size);
			// 内部で管理用メモリ確保
			// 解放責任は内部
			bool initialize(u32 size);

			//
			void destroy();

			template<typename T>
			TlsfMemoryPtr<T> allocate(u32 size = 1);

		private:
			void deallocate(void* ptr);

		private:
			TlsfAllocatorCore	allocator_;
			// アロケータ管理メモリが外部確保か?
			// 外部確保の場合はdestroy()内部で解放しない
			bool				isOuterManageMemory_;
			void*				manageMemory_;
		};

		// 実装
		template<typename T>
		// プールからアロケーション
		// size : 要素サイズ
		//
		//	ngl::memory::TlsfMemoryPtr<ngl::math::Mtx44> ptrPool0 = memPool.allocate<ngl::math::Mtx44>(1);
		//
		inline TlsfMemoryPtr<T> TlsfMemoryPool::allocate(u32 size)
		{
			void* mem = allocator_.Allocate(sizeof(T) * size);
			TlsfMemoryPtr<T> ptr(reinterpret_cast<T*>( mem ), size, TlsfMemoryDeleter(this), &allocator_);
			
			return ptr;
		}


		//--------------------------------------------------------------------------
		// デリータ実装
		// 確保したアロケータで解放する
		inline void TlsfMemoryDeleter::operator()(void* ptr)
		{
			pool_->deallocate(ptr);
		}
	}
}

#endif // _NGL_TLSF_MEMORY_POOL_