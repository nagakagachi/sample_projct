#pragma once

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

#include <thread>
#include <mutex>


#include "ngl/math/math.h"
#include "ngl/util/noncopyable.h"
#include "ngl/util/singleton.h"

#include "ngl/rhi/d3d12/rhi.d3d12.h"


// resource base.
#include "resource.h"

// resource derived.
#include "ngl/gfx/mesh_resource.h"
// other.


namespace ngl
{
namespace res
{

	class ResourceHandleCacheMap
	{
	public:
		ResourceHandleCacheMap();
		~ResourceHandleCacheMap();

		// raw handleでmap保持. Appのリクエストに返す場合は ResourceHandle化して返す.
		std::unordered_map<std::string, detail::ResourceHolderHandle> map_;

		std::mutex	mutex_;
	};


	// Resource管理.
	//	読み込み等.
	//	内部でResourceCacheを持ち参照も保持するため, 外部の参照が無くなったResourceもCacheの参照で破棄されない点に注意.
	//	外部から明示的にCacheからの破棄を要求する仕組みもほしい.
	class ResourceManager : public ngl::Singleton<ResourceManager>
	{
		friend class ResourceDeleter;

	public:
		ResourceManager();

		~ResourceManager();

		// Load Mesh Data. Cache対応.
		ResourceHandle<gfx::ResMeshData> LoadResMesh(rhi::DeviceDep* p_device, const char* filename);

		// Cacheされたリソースをすべて解放. ただし外部で参照が残っている場合はその参照が消えるまでは実体は破棄されない.
		void ReleaseCacheAll();

	private:
		void OnDestroyResource(Resource* p_res);

		void Register(detail::ResourceHolderHandle& raw_handle);
		void Unregister(Resource* p_res);

		detail::ResourceHolderHandle FindHandle(const char* res_typename, const char* filename);

	private:
		ResourceHandleCacheMap* GetOrCreateTypedCacheMap(const char* res_typename);

		std::unordered_map<std::string, ResourceHandleCacheMap*>	res_type_map_;

		// タイプ別Map自体へのアクセスMutex. 全体破棄と個別破棄で部分的に再帰するためrecursive_mutexを使用.
		std::recursive_mutex	res_map_mutex_;

	private:
		class ResourceDeleter : public res::IResourceDeleter
		{
		public:
			void operator()(Resource* p_res);
		};
		ResourceDeleter deleter_instance_ = {};
	};

}
}
