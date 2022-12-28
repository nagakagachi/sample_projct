
#include "resource_manager.h"

#include "ngl/gfx/mesh_loader_assimp.h"

namespace ngl
{
namespace res
{

	ResourceHandleCacheMap::ResourceHandleCacheMap()
	{
	}
	ResourceHandleCacheMap::~ResourceHandleCacheMap()
	{
		std::cout << "~ResourceMap()" << std::endl;
	}



	void ResourceManager::ResourceDeleter::operator()(Resource* p_res)
	{
		ResourceManager::Instance().OnDestroyResource(p_res);
	}


	ResourceManager::ResourceManager()
	{
	}

	ResourceManager::~ResourceManager()
	{
		ReleaseCacheAll();
	}

	// 全て破棄.
	void ResourceManager::ReleaseCacheAll()
	{
		auto lock = std::lock_guard(res_map_mutex_);

		for (; res_type_map_.begin() != res_type_map_.end();)
		{
			auto* m = &*res_type_map_.begin();

			for (auto ri = m->second->map_.begin(); ri != m->second->map_.end();)
			{
				auto tmpi = ri;
				++ri;

				const auto ref_count = tmpi->second.use_count();
				auto* p_res = tmpi->second.get();
				if (1 < ref_count)
				{
					// 参照カウントがManager管理分の1より大きい場合は警告.
					std::cout << "[~ResourceManager] Undestroyed resources: " << p_res->p_res_->GetFileName() << " (" << ref_count << ") " << std::endl;
				}

				// NOTE.  map_.clear() だと内部で参照が消えた要素が自信をMapから除去するためclear内でのイテレータが無効になってしまうため一つずつ消す.
				m->second->map_.erase(tmpi);
			}

			// Type別Map自体破棄.
			delete m->second;

			// 除去.
			res_type_map_.erase(res_type_map_.begin());
		}
	}

	// リソース破棄. 参照カウンタが完全にゼロになった場合.
	void ResourceManager::OnDestroyResource(Resource* p_res)
	{
		// データベースから除去.
		Unregister(p_res);

		// 完全に破棄. 保持している描画リソースの安全性のために破棄キューに積んでNフレーム後に完全破棄するなども可能.
		delete p_res;
	}

	// Load Mesh.
	ResourceHandle<gfx::ResMeshData> ResourceManager::LoadResMesh(rhi::DeviceDep* p_device, const char* filename)
	{
		// 登録済みか検索.
		auto exist_handle = FindHandle(gfx::ResMeshData::k_resource_type_name, filename);
		if (exist_handle.get())
		{
			// あれば返却.
			return ResourceHandle<gfx::ResMeshData>(exist_handle);
		}
		// 存在しない場合は読み込み.

		// 新規生成.
		auto p_res = new gfx::ResMeshData();
		// 情報設定.
		res::ResoucePrivateAccess::SetResourceInfo(p_res, filename);
		// Handle生成.
		auto handle = ResourceHandle<gfx::ResMeshData>(p_res, &deleter_instance_);
		// 内部管理用RawHandle取得. handleの内部参照カウンタ共有.
		auto raw_handle = ResoucePrivateAccess::GetRawHandle(handle);
		// データベースに登録.
		Register(raw_handle);


		// 実際にリソースロード.
		assimp::LoadMeshData(*p_res, p_device, filename);


		// 新規ハンドルを生成して返す.
		return handle;
	}


	detail::ResourceHolderHandle ResourceManager::FindHandle(const char* res_typename, const char* filename)
	{
		ResourceHandleCacheMap* res_map = GetOrCreateTypedCacheMap(res_typename);

		// 対象タイプのみMutexLockで操作.
		auto lock = std::lock_guard(res_map->mutex_);
		if (res_map->map_.end() == res_map->map_.find(filename))
		{
			return {};
		}

		return res_map->map_[filename];
	}


	void ResourceManager::Register(detail::ResourceHolderHandle& raw_handle)
	{
		ResourceHandleCacheMap* res_map = GetOrCreateTypedCacheMap(raw_handle->p_res_->GetResourceTypeName());

#if 1
		// 対象タイプのみMutexLockで操作.
		auto lock = std::lock_guard(res_map->mutex_);
		{
			// 登録処理.
			// すでに同名登録されている場合は参照カウント減少し, ゼロになったらOnDestroyResourceのフローに入る.
			res_map->map_[raw_handle->p_res_->GetFileName()] = raw_handle;
		}
#endif	
	}
	void ResourceManager::Unregister(Resource* p_res)
	{
		ResourceHandleCacheMap* res_map = GetOrCreateTypedCacheMap(p_res->GetResourceTypeName());

#if 1
		// 対象タイプのみMutexLockで操作.
		auto lock = std::lock_guard(res_map->mutex_);
		{
			const auto* filename = p_res->GetFileName();
			if (res_map->map_.end() == res_map->map_.find(filename))
			{
				// 存在しないキー.
				return;
			}

			// 登録解除はポインタが一致するかチェック. 同名リソースに別実体が登録されて参照カウントがゼロになるパターンがあるため.
			if (res_map->map_[filename]->p_res_ == p_res)
			{
				// ハンドルをデータベースから解放.
				// ここで参照カウントがゼロになった場合は ResourceDeleter経由でOnDestroyResourceが呼ばれ, 再度Unregisterが呼ばれるがすでにMapにはないので素通りする.
				res_map->map_.erase(filename);
			}
		}
#endif
	}

	// Thread Safe.
	ResourceHandleCacheMap* ResourceManager::GetOrCreateTypedCacheMap(const char* res_typename)
	{
		ResourceHandleCacheMap* res_map = nullptr;
		{
			// Lock.
			auto lock = std::lock_guard(res_map_mutex_);

			if (res_type_map_.end() == res_type_map_.find(res_typename))
			{
				// タイプ用の要素を追加.
				auto new_map = new ResourceHandleCacheMap();
				// 必要な要素追加.
				res_type_map_[res_typename] = new_map;
			}
			assert(res_type_map_.end() != res_type_map_.find(res_typename));

			res_map = res_type_map_[res_typename];
		}
		return res_map;
	}
}
}