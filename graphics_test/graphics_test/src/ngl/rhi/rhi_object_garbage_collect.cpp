
#include "rhi_object_garbage_collect.h"

namespace ngl
{
namespace rhi
{
	GabageCollector::GabageCollector()
	{
	}
	GabageCollector::~GabageCollector()
	{
	}

	bool GabageCollector::Initialize()
	{
		return true;
	}
	void GabageCollector::Finalize()
	{
#if NGL_RHI_GabageCollector_Enable
		const int max_frame = (int)frame_stack_.size();
		for (int i = 0; i < max_frame; ++i)
		{
			ReadyToNewFrame();
			Execute();
		}
#endif
	}

	// フレーム開始同期処理.
	void GabageCollector::ReadyToNewFrame()
	{
		const int max_frame = (int)frame_stack_.size();
		const int index = (flip_index_.load() + 1) % max_frame;
		flip_index_.store(index);
	}

	// 破棄の実行.
	void GabageCollector::Execute()
	{
#if NGL_RHI_GabageCollector_Enable
		const int max_frame = (int)frame_stack_.size();

		const int oldest_index = (flip_index_.load() + 1) % max_frame;

		while (auto* e = frame_stack_[oldest_index].Pop())
		{
			delete e;
		}
#endif
	}

	// 新規破棄オブジェクトのPush.
	void GabageCollector::Enqueue(RhiObjectBase* p_obj)
	{
#if NGL_RHI_GabageCollector_Enable
		frame_stack_[flip_index_.load()].Push(p_obj);
#endif
	}
}
}
