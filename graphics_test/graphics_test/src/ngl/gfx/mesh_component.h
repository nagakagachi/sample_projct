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
		virtual ~IComponent() = 0;
	};



	class MeshComponent : public IComponent
	{
	public:
		MeshComponent()
		{
		}
		~MeshComponent()
		{
		}

		void SetMeshData(std::shared_ptr<ResMeshData> data)
		{
			mesh_data_ = data;
		}
		ResMeshData* GetMeshData()
		{
			return mesh_data_.get();
		}
		const ResMeshData* GetMeshData() const
		{
			return mesh_data_.get();
		}

		math::Mat34	transform_ = {};

	private:
		std::shared_ptr<ResMeshData> mesh_data_ = {};
	};
}
}
