﻿#pragma once

// rhi_object_garbage_collect.h

#include <atomic>
#include <array>

#include "ngl/rhi/rhi_ref.h"



namespace ngl
{
namespace rhi
{
	// RHIオブジェクトを安全なタイミングまで遅延してから破棄をするためのクラス.
	// 参照管理オブジェクトの破棄からDevice経由でPushされ, フレーム同期を挟んで2フレーム後に実際の破棄をする
	//	(2フレームは GameThread->RenderThread->GPU という構成での最大を考慮)
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
		void Enqueue(IRhiObject* p_obj);

	private:
		std::atomic_int	flip_index_ = 0;

		std::array<RhiObjectGabageCollectStack, 3>	frame_stack_;
	};
}
}
