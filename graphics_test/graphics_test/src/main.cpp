#include <iostream>

#include "ngl/boot/boot_application.h"
#include "ngl/util/unique_ptr.h"
#include "ngl/platform/window.h"


#include "ngl/util/time/timer.h"

#include "ngl/memory/tlsf_memory_pool.h"
#include "ngl/memory/tlsf_allocator.h"

#include "ngl/file/file.h"

#include "ngl/util/shared_ptr.h"


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
	ngl::time::Timer::Instance().StartTimer("AppGameTime");

	{
		ngl::UniquePtr<ngl::boot::BootApplication> boot = ngl::boot::BootApplication::Create();
		AppGame app;
		boot->Run(&app);
	}

	std::cout << "App Time: " << ngl::time::Timer::Instance().GetElapsedSec("AppGameTime") << std::endl;
	return 0;
}



AppGame::AppGame()
{
	// ウィンドウ作成
	window_.Initialize(_T("Test Window"), 1280, 720);


#if 0
	// tlslメモリプールテスト
	{
#define TLSF_POOL_HEAP_MEM___

		ngl::memory::TlsfMemoryPool memPool;

#ifdef TLSF_POOL_HEAP_MEM
		// 外部メモリ確保
		const ngl::u32 bSize = 32 * 1024 * 1024;
		struct MemHolder
		{
			ngl::u8 data_[bSize];
		};
		ngl::SharedPtr<MemHolder> heapMem(new MemHolder());

		memPool.Initialize(heapMem.get(), bSize);
#else
		// 内部メモリ確保
		memPool.Initialize(32 * 1024 * 1024);
#endif
		const ngl::u32 ALLOCATE_LOOP = 1000;
		ngl::time::Timer::Instance().StartTimer("memory_pool_1byte");
		for (ngl::u32 i = 0; i < ALLOCATE_LOOP; ++i)
		{
			ngl::memory::TlsfMemoryPtr<ngl::u8> ptrPool1 = memPool.Allocate<ngl::u8>(1);
		}
		std::cout << "memory_pool_1byte Sec   = " << ngl::time::Timer::Instance().GetElapsedSec("memory_pool_1byte") << std::endl;

		ngl::time::Timer::Instance().StartTimer("memory_pool_64byte");
		for (ngl::u32 i = 0; i < ALLOCATE_LOOP; ++i)
		{
			auto ptrPool0 = memPool.Allocate<float>(1);
		}
		std::cout << "memory_pool_64byte Sec   = " << ngl::time::Timer::Instance().GetElapsedSec("memory_pool_64byte") << std::endl;

		ngl::time::Timer::Instance().StartTimer("memory_pool_1024byte");
		for (ngl::u32 i = 0; i < ALLOCATE_LOOP; ++i)
		{
			auto ptrPool0 = memPool.Allocate<float[16]>(1);

			(*ptrPool0.Get())[0] = 1.1f;

			(*ptrPool0.Get())[1] = 2.2f;

		}
		std::cout << "memory_pool_1024byte Sec   = " << ngl::time::Timer::Instance().GetElapsedSec("memory_pool_1024byte") << std::endl;
	}
#endif

#if 0
	{
		// C++アロケータ版TLSFテスト
		ngl::memory::TlsfAllocator<float> tlsfAlloc;
		ngl::memory::TlsfAllocator<float> tlsfAlloc2;
		tlsfAlloc2.Initialize(1024 * 1024);
		tlsfAlloc = std::move(tlsfAlloc2);
		auto mat0 = tlsfAlloc.allocate(2);
		auto mat1 = tlsfAlloc.allocate(128);
		tlsfAlloc.deallocate(mat0, 2);
		auto mat2 = tlsfAlloc.allocate(1);
		tlsfAlloc.deallocate(mat1, 128);
		auto mat3 = tlsfAlloc.allocate(1);
		tlsfAlloc.deallocate(mat2, 1);
		tlsfAlloc.deallocate(mat3, 1);
		// 解放されていないブロックをチェック(標準出力)
		tlsfAlloc.LeakReport();
		tlsfAlloc.Destroy();// デストロイ
	}
#endif

#if 0
	{
		ngl::file::FileObject file_obj0;
		file_obj0.ReadFile("C:\\Users\\nagak\\git\\github\\sample_project\\graphics_test\\graphics_test\\sample_data\\test_text.txt");
		std::cout << "file size :" << file_obj0.GetFileSize() << std::endl;
	}
#endif

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