#pragma once

#ifndef _NGL_UTIL_UNIQUE_PTR_
#define _NGL_UTIL_UNIQUE_PTR_


//	単一のポインタを管理する
//	コピーは許可されず、moveによる移譲のみが許可される


#include <utility>
#include <iostream>

namespace ngl
{
	// デフォルトデリータ
	template<typename T>
	class UniquePtrDeleter
	{
	public:
		void operator()(T* ptr) const
		{
			if (nullptr != ptr)
				delete ptr;
		}
	};
	template<typename T>
	class UniquePtrDeleter<T[]>
	{
	public:
		void operator()(T* ptr) const
		{
			if (nullptr != ptr)
				delete[] ptr;
		}
	};

	// 型定義用
	template<typename T>
	class UniquePtrTypeDef
	{
	public:
		typedef T* TypePtr;
	};
	template<typename T>
	class UniquePtrTypeDef<T[]>
	{
	public:
		typedef T* TypePtr;
	};

	template<typename T, typename DELETER_T = UniquePtrDeleter<T>>
	class UniquePtr
	{
	public:
		typename typedef DELETER_T Deleter;
		typename typedef UniquePtrTypeDef<T>::TypePtr TypePtr;

		// コンストラクタ
		UniquePtr()
		{
		}
		UniquePtr(TypePtr ptr)
			: ptr_(ptr)
		{
		}
		// moveコンストラクタ
		UniquePtr(UniquePtr&& obj)
		{
			*this = std::move(obj);
		}
		// デストラクタ
		~UniquePtr()
		{
			Reset();
		}
		// moveオペレータ
		UniquePtr& operator=(UniquePtr&& obj)
		{
			Reset(obj.get());
			obj.ptr_ = nullptr;
			return *this;
		}
		TypePtr operator->()
		{
			return ptr_;
		}
		TypePtr operator->() const
		{
			return ptr_;
		}
		T& operator*()
		{
			return *ptr_;
		}
		T& operator*() const
		{
			return *ptr_;
		}
	private:
		// コピー禁止
		UniquePtr(const UniquePtr& obj)
		{
		}
		UniquePtr& operator=(const UniquePtr& obj)
		{
			return *this;
		}

	public:
		// メソッド
		void Reset(TypePtr ptr = nullptr)
		{
			if (nullptr != ptr_)
			{
				//delete ptr_;
				deleter_(ptr_);
			}
			ptr_ = ptr;
		}
		TypePtr Get()
		{
			return ptr_;
		}
		TypePtr Get() const
		{
			return ptr_;
		}
		bool IsValid() const
		{
			return nullptr != ptr_;
		}
	private:
		//T* ptr_;
		TypePtr ptr_ = nullptr;
		Deleter deleter_;
	};
}

#endif // _NGL_UTIL_UNIQUE_PTR_