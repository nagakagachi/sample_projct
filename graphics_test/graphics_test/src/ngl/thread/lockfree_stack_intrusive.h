#pragma once

#include <atomic>

namespace ngl
{
namespace thread
{
	// Node継承クラスをStackする.
	template<typename T>
	class LockFreeStackIntrusive
	{
	public:
		struct Node
		{
			Node()
			{
				next.store(nullptr);
			}

			virtual ~Node() {};

			T& Get() 
			{ 
				return *static_cast<T*>(this); 
			}
			const T& Get() const
			{
				return *static_cast<T*>(this);
			}

		private:
			friend class LockFreeStackIntrusive;

			std::atomic<T*> next;
		};


	public:
		LockFreeStackIntrusive()
		{
		}

		void Push(T* node)
		{
			while (true)
			{
				T* old = top_;
				node->next = old;
				if (top_.compare_exchange_weak(old, node))
				{
					break;
				}
			}
		}
		// ABA問題への対策はしていないため, PopしたポインタをそのままPushし直すようなタスクでは問題が発生する可能性がある.
		T* Pop()
		{
			T* old = nullptr;
			while (true)
			{
				old = top_;
				if (nullptr == old)
					break;

				T* next = old->next;
				if (top_.compare_exchange_weak(old, next))
				{
					break;
				}
			}

			return old;
		}

	private:
		std::atomic<T*> top_ = nullptr;
	};
}
}

