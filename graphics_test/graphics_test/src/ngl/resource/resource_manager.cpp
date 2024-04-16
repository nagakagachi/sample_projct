
#include "resource_manager.h"

#include "ngl/gfx/mesh_loader_assimp.h"
#include "ngl/gfx/texture_loader_directxtex.h"

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
		{
			// Lock.
			auto lock = std::lock_guard<std::mutex>(res_render_update_mutex_);

			frame_render_update_list_with_handle_.clear();
		}

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



	// システムによりRenderThreadで呼び出される. CommandListに必要な描画コマンドを発行する.
	void ResourceManager::UpdateResourceOnRender(rhi::DeviceDep* p_device, rhi::GraphicsCommandListDep* p_commandlist)
	{
		// Lock.
		auto lock = std::lock_guard<std::mutex>(res_render_update_mutex_);
		{
			for (auto& e : frame_render_update_list_with_handle_)
			{
				if (e)
				{
					// RenderCommand.
					e->p_res_->OnResourceRenderUpdate(p_device, p_commandlist);

					e.reset();// 参照リセット.
				}
			}
			// 次フレーム用に空に.
			frame_render_update_list_with_handle_.clear();
		}
	}

	// Shader Load 実装部.
	bool ResourceManager::LoadResourceImpl(rhi::DeviceDep* p_device, gfx::ResShader* p_res, gfx::ResShader::LoadDesc* p_desc)
	{
		ngl::rhi::ShaderDep::InitFileDesc desc = {};
		desc.entry_point_name = p_desc->entry_point_name;
		desc.shader_file_path = p_res->GetFileName();
		desc.shader_model_version = p_desc->shader_model_version;
		desc.stage = p_desc->stage;
		if (!p_res->data_.Initialize(p_device, desc))
		{
			assert(false);
			return false;
		}

		return true;
	}
	// Mesh Load 実装部.
	bool ResourceManager::LoadResourceImpl(rhi::DeviceDep* p_device, gfx::ResMeshData* p_res, gfx::ResMeshData::LoadDesc* p_desc)
	{
		// glTFなどに含まれるマテリアル情報も読み取り. テクスチャの読み込みは別途にするか.
		std::vector<assimp::MaterialTextureSet> material_array = {};
		std::vector<int> shape_material_index_array = {};
		const bool result_load_mesh = assimp::LoadMeshData(p_res->data_, material_array, shape_material_index_array, p_device, p_res->GetFileName());
		if(!result_load_mesh)
			return false;

		return true;
	}

	// Texture Load 実装部.
	bool ResourceManager::LoadResourceImpl(rhi::DeviceDep* p_device, gfx::ResTextureData* p_res, gfx::ResTextureData::LoadDesc* p_desc)
	{
		DirectX::ScratchImage image_data;// ピクセルデータ.
		DirectX::TexMetadata meta_data;
		const bool result_load_image = directxtex::LoadImageData(image_data, meta_data, p_device, p_res->GetFileName());
		if(!result_load_image)
			return false;

		// TODO.
		// シンプルにピクセルデータとフォーマット情報などをp_resにもたせて ResTextureData::OnResourceRenderUpdate() でGPUメモリへCopyさせる.
		
		return true;
	}

	// Thread Safe.
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


	// Thread Safe.
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
	// Thread Safe.
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