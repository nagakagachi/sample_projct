#include "mesh_component.h"

#include "common_struct.h"

namespace ngl
{
namespace gfx
{
	StaticMeshComponent::StaticMeshComponent()
	{
	}
	StaticMeshComponent::~StaticMeshComponent()
	{
	}

	bool StaticMeshComponent::Initialize(rhi::DeviceDep* p_device)
	{
		for (int i = 0; i < cb_instance_.size(); ++i)
		{
			cb_instance_[i] = new rhi::BufferDep();
			rhi::BufferDep::Desc desc = {};
			desc.SetupAsConstantBuffer(sizeof(InstanceInfo));
			cb_instance_[i]->Initialize(p_device, desc);


			cbv_instance_[i] = new rhi::ConstantBufferViewDep();
			rhi::ConstantBufferViewDep::Desc cbv_desc = {};
			cbv_instance_[i]->Initialize(cb_instance_[i].Get(), cbv_desc);
		}

		return true;
	}

	void StaticMeshComponent::SetMeshData(const res::ResourceHandle<ResMeshData>& res_mesh)
	{
		res_mesh_ = res_mesh;
	}
	const ResMeshData* StaticMeshComponent::GetMeshData() const
	{
		return res_mesh_.Get();
	}

	void StaticMeshComponent::UpdateRenderData()
	{
		flip_index_ = (flip_index_ + 1) % 2;

		if (auto* mapped = (InstanceInfo*)cb_instance_[flip_index_]->Map())
		{
			mapped->mtx = transform_;

			cb_instance_[flip_index_]->Unmap();
		}

	}
	rhi::RhiRef<rhi::ConstantBufferViewDep> StaticMeshComponent::GetInstanceBufferView() const
	{
		return cbv_instance_[flip_index_];
	}
}
}
