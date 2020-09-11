/*
	ウィンドウクラス環境非依存部

	// 実装部を内包し、実装部の派生クラスで環境依存実装をする
*/

#pragma once

#ifndef _NGL_GRAPHICS_WINDOW_
#define _NGL_GRAPHICS_WINDOW_
#include <tchar.h>

#if 1
namespace ngl
{
	namespace platform
	{
		class CoreWindowImplDep;

		class CoreWindowImpl
		{
		public:
			CoreWindowImpl() {};
			virtual ~CoreWindowImpl() {};
			virtual bool initialize(const TCHAR* title, int w, int h) = 0;
			virtual void destroy() = 0;

			virtual bool isValid() = 0;
		protected:
		};

		class CoreWindow
		{
		public:
			CoreWindow()
				: windowImpl_(nullptr)
			{
			}
			virtual ~CoreWindow()
			{
				destroy();
			}

			bool initialize(const TCHAR* title, int w, int h)
			{
				// 生成済みなら失敗
				if (nullptr != windowImpl_)
					return false;
				// 生成
				createImplement();
				// 初期化
				windowImpl_->initialize(title, w, h);
				return true;
			}
			void destroy()
			{
				if (nullptr != windowImpl_)
				{
					// 破棄
					windowImpl_->destroy();
					delete windowImpl_;
				}
			}
			bool isValid()
			{
				if (nullptr != windowImpl_ && windowImpl_->isValid())
				{
					return true;
				}
				return false;
			}

			// 実装部取得
			CoreWindowImpl* impl()
			{
				return windowImpl_;
			}
			// 実装依存部取得
			CoreWindowImplDep& dep()
			{
				return *(CoreWindowImplDep*)(windowImpl_);
			}

		protected:

			bool createImplement();

			template<typename T>
			bool createImplementT()
			{
				windowImpl_ = new T();
				return true;
			}

			CoreWindowImpl*  windowImpl_;
		};
	}
}
#endif

#endif // _NGL_GRAPHICS_WINDOW_