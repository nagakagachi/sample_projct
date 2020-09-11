#pragma once
#ifndef _NGL_MEMORY_TLSF_ALLOCATOR_
#define _NGL_MEMORY_TLSF_ALLOCATOR_

/*
	TLSF実装によるアロケータ
		コピーは不許可
		moveは許可


	// C++アロケータ版TLSFテスト
	ngl::memory::TlsfAllocator<ngl::math::Mtx44> tlsfAlloc;
	ngl::memory::TlsfAllocator<ngl::math::Mtx44> tlsfAlloc2;
	tlsfAlloc2.initialize(1024 * 1024);
	tlsfAlloc = std::move(tlsfAlloc2);
	ngl::math::Mtx44* mat0 = tlsfAlloc.allocate(2);
	ngl::math::Mtx44* mat1 = tlsfAlloc.allocate(128);
	tlsfAlloc.deallocate(mat0, 2);
	ngl::math::Mtx44* mat2 = tlsfAlloc.allocate(1);
	tlsfAlloc.deallocate(mat1, 128);
	ngl::math::Mtx44* mat3 = tlsfAlloc.allocate(1);
	tlsfAlloc.deallocate(mat2, 1);
	tlsfAlloc.deallocate(mat3, 1);
	// 解放されていないブロックをチェック(標準出力)
	tlsfAlloc.leakReport();
	tlsfAlloc.destroy();// デストロイ
		
*/

#if 1
#include <memory>
#include "ngl/memory/tlsf_allocator_core.h"

#include "ngl/util/instance_handle.h"

namespace ngl
{

	namespace memory
	{
		typedef ngl::InstanceHandle<ngl::memory::TlsfAllocatorCore, 32> TlsfAllocatorCoreHandle;

		template<typename T>
		struct TlsfAllocator
		{
			typedef T value_type;

			// 初期化
			// 面倒なので現状では内部でヒープを確保して初期化
			bool initialize( u32 byteSize )
			{
				if (isInitialized_)
				{
					// 前回のinitialize()からdestroy()される前に再度initializeしようとした
					return false;
				}

				// ハンドルの生成
				if (!tlsfCore_.newHandle())
				{
					// アロケータのハンドル生成に失敗
					return false;
				}

				poolMemory_ = new u8[byteSize];
				if (nullptr == poolMemory_)
					return false;
				poolSize_ = byteSize;

				if (!tlsfCore_->Initialize(poolMemory_, poolSize_))
				{
					destroy();
					return false;
				}
				isInitialized_ = true;
				return true;
			}

			// 破棄
			void destroy()
			{
				tlsfCore_->destroy();
				tlsfCore_.release();// ハンドル解放
				if (nullptr != poolMemory_)
				{
					delete poolMemory_;
					poolMemory_ = nullptr;
				}
				poolSize_ = 0;
				isInitialized_ = false;
			}

			// メモリリークレポート
			void leakReport()
			{
				tlsfCore_->leakReport();
			}

			TlsfAllocator()
				: poolMemory_(NULL)
				, poolSize_(0)
				, isInitialized_(false)
			{
			}
		
			// コピーは内部で何もしないようにしたい
			template<typename U>
			TlsfAllocator(const TlsfAllocator<U> &) {}
			TlsfAllocator(const TlsfAllocator &) {}
			TlsfAllocator & operator=(const TlsfAllocator &) { return *this; }

			// moveは許可
			TlsfAllocator(TlsfAllocator&& obj)
			{
				*this = std::move(obj);
			}
			// moveオペレータ
			TlsfAllocator & operator=(TlsfAllocator&& obj)
			{
				// 内容をコピーしつつ移動元は無効にしていく
				poolSize_ = obj.poolSize_;
				obj.poolSize_ = 0;

				poolMemory_ = obj.poolMemory_;
				obj.poolMemory_ = nullptr;

				isInitialized_ = obj.isInitialized_;

				tlsfCore_ = std::move( obj.tlsfCore_ );

				return *this;
			}


			typedef std::true_type propagate_on_container_copy_assignment;
			typedef std::true_type propagate_on_container_move_assignment;
			typedef std::true_type propagate_on_container_swap;

			bool operator==(const TlsfAllocator & other) const
			{
				return this == &other;
			}
			bool operator!=(const TlsfAllocator & other) const
			{
				return !(*this == other);
			}

			T * allocate(size_t num_to_allocate)
			{
				return static_cast<T*>(tlsfCore_->Allocate(sizeof(T)* num_to_allocate));
			}
			void deallocate(T * ptr, size_t num_to_free)
			{
				tlsfCore_->Deallocate(ptr);
			}

			// boilerplate that shouldn't be needed, except
			// libstdc++ doesn't use allocator_traits yet
			template<typename U>
			struct rebind
			{
				typedef TlsfAllocator<U> other;
			};
			typedef T * pointer;
			typedef const T * const_pointer;
			typedef T & reference;
			typedef const T & const_reference;
			template<typename U, typename... Args>
			void construct(U * object, Args &&... args)
			{
				new (object)U(std::forward<Args>(args)...);
			}
			template<typename U, typename... Args>
			void construct(const U * object, Args &&... args) = delete;
			template<typename U>
			void destroy(U * object)
			{
				object->~U();
			}

		private:


			u8*					poolMemory_;
			u32					poolSize_;
			bool				isInitialized_;
			TlsfAllocatorCoreHandle tlsfCore_;
		};
	}
}

#endif

#endif //_NGL_MEMORY_TLSF_ALLOCATOR_