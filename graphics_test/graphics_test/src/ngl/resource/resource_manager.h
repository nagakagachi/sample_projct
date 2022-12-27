#pragma once

#include <memory>
#include <vector>

#include "ngl/math/math.h"
#include "ngl/util/noncopyable.h"
#include "ngl/util/singleton.h"


#include "ngl/rhi/d3d12/rhi.d3d12.h"


#include "resource.h"

#include "ngl/gfx/mesh_resource.h"



namespace ngl
{
namespace res
{

	// Resource管理.
	//	読み込み等.
	class ResourceManager : public ngl::Singleton<ResourceManager>
	{
	public:
		ResourceManager()
		{
		}

		~ResourceManager()
		{
		}

		// Load Mesh Data.
		ResourceHandle<gfx::ResMeshData> LoadResMesh(rhi::DeviceDep* p_device, const char* filename);

	private:
	};





}
}
