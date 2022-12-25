#pragma once


#include "ngl/rhi/d3d12/rhi.d3d12.h"

#include "ngl/gfx/mesh_component.h"

namespace ngl
{
namespace assimp
{
	void LoadMeshData(rhi::DeviceDep* p_device, const char* filename, gfx::MeshData& out_mesh);
}
}