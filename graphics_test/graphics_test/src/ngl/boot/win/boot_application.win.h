#pragma once
#ifndef _NGL_BOOT_APPLICATION_WIN_H_
#define _NGL_BOOT_APPLICATION_WIN_H_

#include "ngl/boot/boot_application.h"

namespace ngl
{
	namespace boot
	{
		class BootApplicationDep : public BootApplication
		{
		public:
			BootApplicationDep()
			{
			}
			void run(ApplicationBase* app);
		};

	}
}


#endif // _NGL_BOOT_APPLICATION_WIN_H_