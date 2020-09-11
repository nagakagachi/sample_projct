#pragma once
#ifndef _NGL_UTIL_LOCK_FREE_STACK_
#define _NGL_UTIL_LOCK_FREE_STACK_

#include <atomic>

namespace ngl
{
	/*
		侵襲型ロックフリースタック
		奇妙に再帰したテンプレートパターンでNodeを継承したオブジェクトを扱うスタック
		class TestNode : public LockFreeStack<オブジェクト型>::Node
		{
			// ...
		};

		CASでpushとpopを実装しているだけの非常に簡素なもの
	*/
	template<class T>
	class LockFreeStack
	{
	public:
		class Node
		{
			friend LockFreeStack;
		public:
			Node()
			{
				next_ = nullptr;
			}
			virtual ~Node()
			{
			}
		private:
			Node* next_;
		};

	public:
		LockFreeStack()
		{
			head_.store(nullptr);
		}
		~LockFreeStack()
		{
		}

		bool push(Node* node)
		{
			if (nullptr == node)
				return false;

			Node* oldHead = nullptr;
			do
			{
				node->next_ = oldHead = head_.load();
			} while ( !head_.compare_exchange_weak(oldHead, node) );
			
			return true;
		}
		T* pop()
		{
			Node* oldHead = nullptr;
			do
			{
				oldHead = head_.load();
				if (nullptr == oldHead)
					break;
			} while ( !head_.compare_exchange_weak(oldHead, oldHead->next_) );

			return reinterpret_cast<T*>(oldHead);
		}
	protected:
	private:
		std::atomic<Node*> head_;
	};
}

#endif // _NGL_UTIL_LOCK_FREE_STACK_