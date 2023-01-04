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

			std::atomic<Node*> next;
		};


	public:
		LockFreeStackIntrusive()
		{
		}

		void Push(Node* node)
		{
			while (true)
			{
				Node* old = top_;
				node->next = old;
				if (top_.compare_exchange_weak(old, node))
				{
					break;
				}
			}
		}
		// ABA問題への対策はしていないため, PopしたポインタをそのままPushし直すようなタスクでは問題が発生する可能性がある.
		Node* Pop()
		{
			Node* old = nullptr;
			while (true)
			{
				old = top_;
				if (nullptr == old)
					break;

				Node* next = old->next;
				if (top_.compare_exchange_weak(old, next))
				{
					break;
				}
			}

			return old;
		}

	private:
		std::atomic<Node*> top_ = nullptr;
	};
}
}

