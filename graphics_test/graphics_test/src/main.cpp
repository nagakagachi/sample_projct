#include <iostream>

#include "ngl/boot/boot_application.h"
#include "ngl/util/unique_ptr.h"
#include "ngl/platform/window.h"


// アプリ本体.
class AppGame : public ngl::boot::ApplicationBase
{
public:
	AppGame();
	~AppGame();

	// メイン
	bool execute();

private:
	ngl::platform::CoreWindow window_;
};




int main()
{
	std::cout << "Hello World!" << std::endl;

	ngl::UniquePtr<ngl::boot::BootApplication> boot = ngl::boot::BootApplication::create();
	AppGame app;
	boot->run(&app);

	return 0;
}



AppGame::AppGame()
{
	// ウィンドウ作成
	window_.initialize(_T("Test Window"), 1280, 720);
}
AppGame::~AppGame()
{
}

// メインループから呼ばれる
bool AppGame::execute()
{
	// ウィンドウが無効になったら終了
	if (!window_.isValid())
	{
		return false;
	}

	return true;
}