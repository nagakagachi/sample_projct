﻿#pragma once

#include <memory>

#include "ngl/rhi/d3d12/rhi.d3d12.h"

#include "ngl/gfx/mesh_resource.h"

namespace ngl
{
namespace assimp
{
	bool LoadMeshData(gfx::ResMeshData& out, rhi::DeviceDep* p_device, const char* filename);
}
}