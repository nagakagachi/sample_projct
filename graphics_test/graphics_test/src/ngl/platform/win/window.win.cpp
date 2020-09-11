
#include "window.win.h"


namespace ngl
{
	namespace platform
	{
		// 生成部を実装
		bool CoreWindow::createImplement()
		{
			return createImplementT<CoreWindowImplDep>();
		}


		CoreWindowImplDep::CoreWindowImplDep()
		{
		}
		CoreWindowImplDep::~CoreWindowImplDep()
		{
			destroy();
		}

		// ウィンドウ生成
		bool CoreWindowImplDep::initialize(const TCHAR* title, int w, int h)
		{
			// 初期化済みなら帰る
			if (isValidWindow())
				return false;

			WNDCLASSEX wcex = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, CoreWindowImplDep::CallProc, 0, 0, 0, NULL, NULL,
				(HBRUSH)(COLOR_WINDOW + 1), NULL, (_TCHAR*)title, NULL };
			if (!RegisterClassEx(&wcex)) {
				return false;
			}

			// 自身のポインタを指定してウィンドウ生成
			if (!(hwnd_ = CreateWindow(title, title, WS_OVERLAPPEDWINDOW & ~(WS_MAXIMIZEBOX | WS_SIZEBOX),
				CW_USEDEFAULT, 0, w, h, 0, 0, GetModuleHandle(NULL), (LPVOID)this))) {
				return false;
			}

			setWindowSize(w, h);
			ShowWindow(hwnd_, SW_SHOW);
			return true;
		}
		void CoreWindowImplDep::destroy()
		{
			if (isValidWindow())
			{
				// 破棄時にはセットしておいた自分自身をクリア
				SetPointer(hwnd_, NULL);
				DestroyWindow(hwnd_);
			}
		}

		// 有効か？
		bool CoreWindowImplDep::isValid()
		{
			return isValidWindow();
		}

		bool CoreWindowImplDep::isValidWindow()
		{
			return TRUE == IsWindowEnabled(hwnd_);
		}
		void CoreWindowImplDep::setWindowSize(unsigned int w, unsigned int h)
		{
			screenWidth_ = w;
			screenHeight_ = h;
			unsigned int ww, wh;
			getWindowSizeFromClientSize(screenWidth_, screenHeight_, ww, wh);
			::SetWindowPos(hwnd_, NULL, 0, 0, ww, wh, SWP_NOMOVE | SWP_NOZORDER);
		}

		void CoreWindowImplDep::getWindowSizeFromClientSize(unsigned int cw, unsigned int ch, unsigned int& ww, unsigned int& wh)
		{
			RECT rw, rc;
			GetWindowRect(hwnd_, &rw);
			GetClientRect(hwnd_, &rc);

			ww = (rw.right - rw.left) - (rc.right - rc.left) + cw;
			wh = (rw.bottom - rw.top) - (rc.bottom - rc.top) + ch;
		}


		// ウィンドウに実体を登録
		void CoreWindowImplDep::SetPointer(HWND hWnd, CoreWindowImplDep* ptr)
		{
#ifdef _WIN64
			//64bit
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)ptr);
#else
			// 32bit
			SetWindowLong(hWnd, GWL_USERDATA, (LONG)ptr);
#endif
		}

		//===== ウィンドウプロシージャの呼び出し =====//
		LRESULT CALLBACK CoreWindowImplDep::CallProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
		{
			// プロパティリストからthisポインタを取得
#ifdef _WIN64
			//64bit
			CoreWindowImplDep* thisPtr = (CoreWindowImplDep*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
#else
			// 32bit
			CoreWindowImplDep* thisPtr = (CoreWindowImplDep*)GetWindowLong(hWnd, GWL_USERDATA);
#endif
			// まだポインタが設定されてない場合
			if (!thisPtr)
			{
				// 生成時の場合は生成時パラメータから取得
				if (message == WM_CREATE)
					thisPtr = (CoreWindowImplDep*)((LPCREATESTRUCT)lParam)->lpCreateParams;

				//　実体にウィンドウハンドルをセット
				if (thisPtr)
				{
					//＿プロパティリストにオブジェクトハンドル(thisポインタ)を設定する
					thisPtr->SetPointer(hWnd, thisPtr);
				}
			}
			// 個別のWindowプロシージャのコール
			if (thisPtr)
			{
				LRESULT lResult = thisPtr->MainProc(hWnd, message, wParam, lParam);
				return lResult;
			}
			// 実体がなければデフォルト
			return DefWindowProc(hWnd, message, wParam, lParam);
		}


		//===== ウィンドウプロシージャの実装(継承可能) =====//
		// ここでの記述はデフォルトの処理
		LRESULT CoreWindowImplDep::MainProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
		{
			switch (message)
			{
				// ウィンドウが破棄された場合
			case WM_DESTROY:
			{
				PostQuitMessage(0);
			}
			return 0;

			// デフォルトの場合
			default:
				return DefWindowProc(hWnd, message, wParam, lParam);
			}
		}

	}
}