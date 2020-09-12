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

			virtual bool Initialize(const TCHAR* title, int w, int h) override;
			virtual void Destroy() override;
			virtual bool IsValid() const override;


			// クライアントサイズからウィンドウのサイズを計算
			void GetWindowSizeFromClientSize(unsigned int cw, unsigned int ch, unsigned int& ww, unsigned int& wh);

			// ウィンドウサイズ(実際はクライアントサイズ)を設定
			void SetWindowSize(unsigned int w, unsigned int h);

			// 有効なWindowか.
			bool IsValidWindow() const;

			HWND GetWindowHandle() const
			{
				return hwnd_;
			}

			// ウィンドウプロシージャの呼び出し
			static LRESULT CALLBACK CallProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
			// ウィンドウプロシージャの実装
			virtual LRESULT MainProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
		protected:
			// ポインタの設定
			void SetPointer(HWND hWnd, CoreWindowImplDep* ptr);

			WNDCLASSEX		wcex_ = {};
			HWND			hwnd_ = {};
			bool			is_valid_window_ = false;
			unsigned int	screen_w_ = {};
			unsigned int	screen_h_ = {};
		};

	}
}
#endif

#endif // _NGL_GRAPHICS_WINDOW_WIN_