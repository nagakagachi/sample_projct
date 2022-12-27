
#include "resource_manager.h"

#include "ngl/gfx/mesh_loader_assimp.h"

namespace ngl
{
namespace res
{

	ResourceHandle<gfx::ResMeshData> ResourceManager::LoadResMesh(rhi::DeviceDep* p_device, const char* filename)
	{
		auto p = new gfx::ResMeshData();
		assimp::LoadMeshData(*p, p_device, filename);

		return ResourceHandle<gfx::ResMeshData>(p);
	}


}
}