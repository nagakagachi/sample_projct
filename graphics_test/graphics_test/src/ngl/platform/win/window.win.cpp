
#include "window.win.h"

//#include <windowsx.h>


namespace ngl
{
	namespace platform
	{
		// 生成部を実装
		bool CoreWindow::CreateImplement()
		{
			return CreateImplementT<CoreWindowImplDep>();
		}


		CoreWindowImplDep::CoreWindowImplDep()
		{
		}
		CoreWindowImplDep::~CoreWindowImplDep()
		{
			Destroy();
		}

		// ウィンドウ生成
		bool CoreWindowImplDep::Initialize(const TCHAR* title, int w, int h)
		{
			// 初期化済みなら帰る
			if (IsValidWindow())
				return false;

			HINSTANCE hinstance = GetModuleHandle(nullptr);

			wcex_ = {};
			wcex_.cbSize		= sizeof(WNDCLASSEX);
			wcex_.style			= CS_HREDRAW | CS_VREDRAW;
			wcex_.lpfnWndProc	= CoreWindowImplDep::CallProc;
			wcex_.cbClsExtra	= 0;
			wcex_.cbWndExtra	= 0;
			wcex_.hInstance		= hinstance;
			wcex_.hIcon			= nullptr;
			wcex_.hCursor		= nullptr;
			wcex_.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
			wcex_.lpszMenuName	= nullptr;
			wcex_.lpszClassName = (_TCHAR*)title;
			wcex_.hIconSm		= nullptr;


			if (!RegisterClassEx(&wcex_)) {
				return false;
			}

			// 自身のポインタを指定してウィンドウ生成
			if (!(hwnd_ = CreateWindow(title, title, WS_OVERLAPPEDWINDOW & ~(WS_MAXIMIZEBOX | WS_SIZEBOX),
				CW_USEDEFAULT, 0, w, h, 0, 0, GetModuleHandle(NULL), (LPVOID)this))) {
				return false;
			}

			// 有効化
			is_valid_window_ = true;

			SetWindowSize(w, h);
			ShowWindow(hwnd_, SW_SHOW);
			return true;
		}
		void CoreWindowImplDep::Destroy()
		{
			if (IsValidWindow())
			{
				is_valid_window_ = false;
				// 破棄時にはセットしておいた自分自身をクリア
				SetPointer(hwnd_, NULL);
				DestroyWindow(hwnd_);
				UnregisterClassW(wcex_.lpszClassName, wcex_.hInstance);
			}

			wcex_ = {};
			hwnd_ = {};
		}

		// 有効か？
		bool CoreWindowImplDep::IsValid() const
		{
			return IsValidWindow();
		}

		bool CoreWindowImplDep::IsValidWindow() const
		{
			//return TRUE == IsWindowEnabled(hwnd_);
			return is_valid_window_;
		}

		void CoreWindowImplDep::GetScreenSize(unsigned int& w, unsigned int& h) const
		{
			w = screen_w_;
			h = screen_h_;
		}


		void CoreWindowImplDep::SetWindowSize(unsigned int w, unsigned int h)
		{
			screen_w_ = w;
			screen_h_ = h;
			unsigned int ww, wh;
			GetWindowSizeFromClientSize(screen_w_, screen_h_, ww, wh);
			::SetWindowPos(hwnd_, NULL, 0, 0, ww, wh, SWP_NOMOVE | SWP_NOZORDER);
		}

		void CoreWindowImplDep::GetWindowSizeFromClientSize(unsigned int cw, unsigned int ch, unsigned int& ww, unsigned int& wh)
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

				// フラグのリセットなどのために破棄処理呼び出し.
				Destroy();
				break;
			}

			default:
				break;
			}

			InputProc(message, wParam, lParam);

			return DefWindowProc(hWnd, message, wParam, lParam);
		}


		void CoreWindowImplDep::InputProc(UINT message, WPARAM wParam, LPARAM lParam)
		{
			if (message == WM_KEYDOWN || message == WM_KEYUP)
			{
				// keyboard virtual key state. VK_LEFT, VK_0, VK_A, VK_Z, VK_OEM_PLUS.
				int keyboard_key = -1;
				bool keyboard_down = (message == WM_KEYDOWN);
				if (0 < wParam && 0xff > wParam)
				{
					keyboard_key = (int)wParam;

					virtual_key_state_[keyboard_key] = (keyboard_down) ? 1 : 0;
				}
			}
			else if (message == WM_MOUSEMOVE)
			{
				// position.
				mouse_pos_x_ = LOWORD(lParam);
				mouse_pos_y_ = HIWORD(lParam);

				mouse_pos_x_rate_ = static_cast<float>(mouse_pos_x_) / static_cast<float>(screen_w_);
				mouse_pos_y_rate_ = static_cast<float>(mouse_pos_y_) / static_cast<float>(screen_h_);
			}
			else if (message == WM_LBUTTONDOWN || message == WM_LBUTTONUP)
			{
				// mouse left button.
				mouse_left_state_ = (message == WM_LBUTTONDOWN);
			}
			else if (message == WM_RBUTTONDOWN || message == WM_RBUTTONUP)
			{
				// mouse right button.
				mouse_right_state_ = (message == WM_RBUTTONDOWN);
			}
			else if (message == WM_MBUTTONDOWN || message == WM_MBUTTONUP)
			{
				// mouse middl button.
				mouse_middle_state_ = (message == WM_MBUTTONDOWN);
			}

		}

	}
}