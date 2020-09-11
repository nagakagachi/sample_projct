
#include <Windows.h>

#include "boot_application.win.h"

namespace ngl
{
	namespace boot
	{

		BootApplication* BootApplication::create()
		{
			return new BootApplicationDep();
		}


		void BootApplicationDep::run(ApplicationBase* app)
		{
			// Windowsメッセージループ
			MSG msg;
			do {
				if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
				{
					DispatchMessage(&msg);
				}
				else
				{
					// アプリケーション実行
					if (!app->execute())
					{
						break;
					}
				}
			}
			while ( true );
		}
	}
}