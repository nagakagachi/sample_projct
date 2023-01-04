#pragma once

// rhi_object_garbage_collect.h

#include <atomic>
#include <array>

#include "ngl/rhi/rhi_ref.h"


#define NGL_RHI_GabageCollector_Enable 1

namespace ngl
{
namespace rhi
{
	class GabageCollector
	{
	public:
		GabageCollector();
		~GabageCollector();

		bool Initialize();
		void Finalize();

		// フレーム開始同期処理.
		void ReadyToNewFrame();

		// 破棄の実行.
		void Execute();

		// 新規破棄オブジェクトのPush.
		void Enqueue(RhiObjectBase* p_obj);

	private:
		std::atomic_int	flip_index_ = 0;

#if NGL_RHI_GabageCollector_Enable
		std::array<RhiObjectBaseGabageCollectStack, 3>	frame_stack_;
#endif

	};
}
}
