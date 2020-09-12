#include <iostream>

#include "ngl/boot/boot_application.h"
#include "ngl/util/unique_ptr.h"
#include "ngl/platform/window.h"


#include "ngl/util/time/timer.h"

#include "ngl/memory/tlsf_memory_pool.h"


// アプリ本体.
class AppGame : public ngl::boot::ApplicationBase
{
public:
	AppGame();
	~AppGame();

	// メイン
	bool Execute();

private:
	ngl::platform::CoreWindow window_;
};




int main()
{
	std::cout << "Hello World!" << std::endl;
	ngl::time::Timer::instance().StartTimer("AppGameTime");

	{
		ngl::UniquePtr<ngl::boot::BootApplication> boot = ngl::boot::BootApplication::Create();
		AppGame app;
		boot->Run(&app);
	}

	std::cout << "App Time: " << ngl::time::Timer::instance().GetElapsedSec("AppGameTime") << std::endl;
	return 0;
}



AppGame::AppGame()
{
	// ウィンドウ作成
	window_.Initialize(_T("Test Window"), 1280, 720);
}
AppGame::~AppGame()
{
}

// メインループから呼ばれる
bool AppGame::Execute()
{
	// ウィンドウが無効になったら終了
	if (!window_.IsValid())
	{

		return false;
	}

	return true;
}