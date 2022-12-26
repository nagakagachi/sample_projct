#pragma once

#include <memory>

#include "ngl/rhi/d3d12/rhi.d3d12.h"

#include "ngl/gfx/mesh_resource.h"

namespace ngl
{
namespace assimp
{
	std::shared_ptr<gfx::ResMeshData> LoadMeshData(rhi::DeviceDep* p_device, const char* filename);
}
}