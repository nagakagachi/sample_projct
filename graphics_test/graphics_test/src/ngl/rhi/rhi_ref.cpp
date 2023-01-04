
#include "rhi_ref.h"


// とりあえずD3D12用
#include "ngl/rhi/d3d12/rhi.d3d12.h"


namespace ngl
{
namespace rhi
{
	namespace detail
	{
		RhiObjectHolder::RhiObjectHolder()
		{
		}
		RhiObjectHolder::RhiObjectHolder(RhiObjectBase* p)
		{
			p_obj_ = p;
		}
		RhiObjectHolder::~RhiObjectHolder()
		{
#if 1
			// TODO.
			// 安全なタイミングでのRHIオブジェクト破棄対応.
			// Deviceの持つFrameDestroyListに積み込む等.
			if (p_obj_ && p_obj_->GetParentDeviceInreface())
			{
				// 実際の破棄処理をDeviceに依頼.
				p_obj_->GetParentDeviceInreface()->DestroyRhiObject(p_obj_);
				p_obj_ = nullptr;
			}
#else
			if (p_res_)
			{
				assert(false); // 本来はRhiオブジェクト破棄を安全なタイミングで遅延実行する.

				delete p_res_;
			}
#endif
		}
	}
}
}