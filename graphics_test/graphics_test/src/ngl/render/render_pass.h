#pragma once


#include "ngl/math/math.h"

#include "ngl/rhi/d3d12/device.d3d12.h"
#include "ngl/rhi/d3d12/shader.d3d12.h"
#include "ngl/rhi/d3d12/command_list.d3d12.h"
#include "ngl/rhi/d3d12/resource_view.d3d12.h"

#include "ngl/resource/resource_manager.h"
#include "ngl/gfx/resource/resource_shader.h"


#include "graph_builder.h"

namespace ngl::render
{
	/*
	class IPass
	{
	public:
		IPass() = default;
		virtual ~IPass() = 0;

		virtual void Setup();

		virtual void Execute(rhi::GraphicsCommandListDep* p_command_list) = 0;

	protected:
	};
	*/
}