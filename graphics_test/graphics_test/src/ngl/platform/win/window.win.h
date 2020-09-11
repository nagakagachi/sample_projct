/*
	ウィンドウクラス依存部実装
*/

#pragma once

#ifndef _NGL_GRAPHICS_WINDOW_WIN_
#define _NGL_GRAPHICS_WINDOW_WIN_

#include <Windows.h>

#include "ngl/platform/window.h"

#if 1
namespace ngl
{
	namespace platform
	{
		class CoreWindowImplDep : public CoreWindowImpl
		{
		public:
			CoreWindowImplDep();
			virtual ~CoreWindowImplDep();
			virtual bool initialize(const TCHAR* title, int w, int h);
			virtual void destroy();

			virtual bool isValid();

			HWND getWindowHandle()
			{
				return hwnd_;
			}

			// クライアントサイズからウィンドウのサイズを計算
			void getWindowSizeFromClientSize(unsigned int cw, unsigned int ch, unsigned int& ww, unsigned int& wh);
			// ウィンドウサイズ(実際はクライアントサイズ)を設定
			void setWindowSize(unsigned int w, unsigned int h);
			// 有効か?
			bool isValidWindow();


			// ウィンドウプロシージャの呼び出し
			static LRESULT CALLBACK CallProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
			// ウィンドウプロシージャの実装
			virtual LRESULT MainProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
		protected:
			// ポインタの設定
			void SetPointer(HWND hWnd, CoreWindowImplDep* ptr);

			HWND hwnd_ = {};
			unsigned int screenWidth_ = {};
			unsigned int screenHeight_ = {};
		};

	}
}
#endif

#endif // _NGL_GRAPHICS_WINDOW_WIN_