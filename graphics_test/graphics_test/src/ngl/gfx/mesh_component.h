#pragma once

#include <memory>

#include "ngl/util/noncopyable.h"
#include "ngl/math/math.h"

#include "mesh_resource.h"

namespace ngl
{
namespace gfx
{


	class IComponent : public NonCopyableTp<IComponent>
	{
	public:
		IComponent()
		{
		}
		virtual ~IComponent() {}
	};



	class StaticMeshComponent : public IComponent
	{
	public:
		StaticMeshComponent()
		{
		}
		~StaticMeshComponent()
		{
		}

		void SetMeshData(const res::ResourceHandle<ResMeshData>& res_mesh)
		{
			res_mesh_ = res_mesh;
		}
		const ResMeshData* GetMeshData() const
		{
			return res_mesh_.Get();
		}

		math::Mat34	transform_ = math::Mat34::Identity();
	private:
		res::ResourceHandle<ResMeshData> res_mesh_ = {};
	};
}
}
