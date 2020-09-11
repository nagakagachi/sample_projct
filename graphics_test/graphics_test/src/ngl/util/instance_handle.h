#pragma once
#ifndef _NGL_UTIL_INSTANCE_HANDLE
#define _NGL_UTIL_INSTANCE_HANDLE

#include <utility>

#include "ngl/util/types.h"

#include "ngl/util/singleton.h"

namespace ngl
{
	template<typename T, u16 NUM_INSTANCE>
	class InstanceHandleFactory;


	//
	//	固定長の共有ポインタ的オブジェクトハンドル
	//	
	//		// 以下のようにタイプと個数を指定して置き
	//		typedef ngl::InstanceHandle<double, 128> TestHandle;
	//
	//		TestHandle handle0;  // 既定では無効なハンドルになっている
	//
	//		handle0.newHandle(); // これで新しく有効なハンドルを生成
	//
	//		TestHandle handle1 = handle0;  // のようにするとhandle1とhandle0が同じ実体を共有する
	//
	//		handle0.release(); で参照カウントを減算する
	//		
	//		参照カウントがゼロになった実態はフリーになり、新しいハンドルに割り当て可能になる
	//
	//		実体のクラスはデフォルトコンストラクタを持っている必要があります
	//
	//
	template<typename T, u16 NUM_INSTANCE>
	class InstanceHandle
	{
		typedef InstanceHandleFactory<T, NUM_INSTANCE> Factory;

	public:
		friend Factory;

		InstanceHandle()
			: handleId_(NUM_INSTANCE)
		{
			// とりあえず既定でハンドル取得しておく
			// 確保しない
			//Factory::instance().newHandle(*this);
		}


		// コピーコンストラクタ
		InstanceHandle(const InstanceHandle& obj)
		{
			*this = obj;
		}
		// 代入
		InstanceHandle& operator=(const InstanceHandle& obj)
		{
			// 今までのハンドルのカウントを減算して
			release();
			
			// 新しいハンドルをコピーして
			handleId_ = obj.handleId_;


			// 新しい方でカウント加算
			Factory::instance().addRef(*this);
			return *this;
		}

		// move系
		InstanceHandle(InstanceHandle&& obj)
		{
			*this = std::move(obj);
		}
		InstanceHandle & operator=(InstanceHandle&& obj)
		{
			// 持っていたハンドルは無効にして
			release();
			// ハンドルIDだけコピーして
			handleId_ = obj.handleId_;
			// ハンドルを委譲するためobj側のハンドルは直接無効化
			obj.handleId_ = NUM_INSTANCE;
			
			return *this;
		}

		~InstanceHandle()
		{
			// デストラクタで参照カウント減算
			release();
		}

		T* operator->()
		{
			return Factory::instance().getObject(*this);
		}

		// インスタンス取得
		T* get()
		{
			return Factory::instance().getObject(*this);
		}

		bool isValidHandle()
		{
			return Factory::instance().isValidHandle(*this);
		}
		// 新規ハンドル
		bool newHandle()
		{
			// 現在のハンドルを解放して
			release();
			// 新規取得
			Factory::instance().newHandle(*this);
			return isValidHandle();
		}
		// ハンドル解放
		void release()
		{
			// 参照カウント減算して無効なハンドルにする
			Factory::instance().release(*this);
			handleId_ = NUM_INSTANCE;
		}

	private:
		u16 handleId_;
	};

	template<typename T, u16 NUM_INSTANCE>
	class InstanceHandleFactory : public Singleton<InstanceHandleFactory<T, NUM_INSTANCE>>
	{
		typedef InstanceHandle<T, NUM_INSTANCE> HandleType;

	public:
		InstanceHandleFactory()
		{
			for (u32 i = 0; i < NUM_INSTANCE; ++i)
				refCount_[i] = 0;
		}

		T* getObject(const HandleType& handle)
		{
			if (isValidHandle(handle))
				return &instance_[handle.handleId_];
			return NULL;
		}


		void newHandle(HandleType& handle)
		{
			handle.handleId_ = NUM_INSTANCE;
			for (u32 i = 0; i < NUM_INSTANCE; ++i)
			{
				if (0 == refCount_[i])
				{
					// 参照1で開始
					refCount_[i] = 1;
					handle.handleId_ = i;
					break;
				}
			}
		}
		bool isValidHandle(const HandleType& handle)
		{
			if (NUM_INSTANCE > handle.handleId_ && 1 <= refCount_[handle.handleId_])
				return true;
			return false;
		}
		// 参照カウント加算
		void addRef(const HandleType& handle)
		{
			if (isValidHandle(handle))
			{
				++refCount_[handle.handleId_];
			}
		}
		// 参照カウント減算
		void release(const HandleType& handle)
		{
			if (isValidHandle(handle))
			{
				if (0 == --refCount_[handle.handleId_])
				{
					// 参照がゼロになった
				}
			}
		}

	private:
		T	instance_[NUM_INSTANCE];
		u16	refCount_[NUM_INSTANCE];
	};
}

#endif // _NGL_UTIL_INSTANCE_HANDLE