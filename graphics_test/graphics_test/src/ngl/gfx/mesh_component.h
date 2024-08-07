﻿#pragma once

#include <memory>

#include "ngl/util/noncopyable.h"
#include "ngl/math/math.h"
#include "render/standard_render_model.h"

#include "resource/resource_mesh.h"

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
		StaticMeshComponent();
		~StaticMeshComponent();

		bool Initialize(rhi::DeviceDep* p_device, const res::ResourceHandle<ResMeshData>& res_mesh);
		const ResMeshData* GetMeshData() const;

		void UpdateRenderData();
		rhi::RhiRef<rhi::ConstantBufferViewDep> GetInstanceBufferView() const;

		math::Mat34	transform_ = math::Mat34::Identity();
		StandardRenderModel	model_ = {};
		
	private:
		std::array<rhi::RhiRef<rhi::BufferDep>, 2>	cb_instance_;
		std::array<rhi::RhiRef<rhi::ConstantBufferViewDep>, 2>	cbv_instance_;

		math::Mat34	transform_prev_ = math::Mat34::Identity();

		s8 flip_index_ = 0;
	};


	// 簡易シーン.
	class SceneRepresentation
	{
	public:
		SceneRepresentation() {}
		~SceneRepresentation() {}

		std::vector<gfx::StaticMeshComponent*> mesh_instance_array_ = {};
	};

}
}
