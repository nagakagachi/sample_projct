#pragma once

#ifndef _NGL_UTIL_SINGLETON_
#define _NGL_UTIL_SINGLETON_

#include "noncopyable.h"


namespace ngl
{

	// シングルトン
	template< typename Derived >
	class Singleton
		: NonCopyableTp< Singleton<Derived> >
	{
	public:
		// インスタンス取得
		static Derived& instance();

	protected:
		virtual ~Singleton() {}

	private:
	};

	template< typename Derived >
	inline
		Derived& Singleton<Derived>::instance()
	{
		// C++11以降では静的変数初期化がスレッドセーフらしいので
		static Derived instance;
		return instance;
	}
}
#endif // _NGL_UTIL_SINGLETON_