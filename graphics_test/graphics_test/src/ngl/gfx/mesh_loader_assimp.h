#pragma once

#include <memory>

#include "ngl/rhi/d3d12/device.d3d12.h"

#include "ngl/gfx/resource_mesh.h"

namespace ngl
{
namespace assimp
{
	bool LoadMeshData(gfx::ResMeshData& out, rhi::DeviceDep* p_device, const char* filename);
}
}