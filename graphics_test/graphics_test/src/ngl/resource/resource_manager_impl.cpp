
#include "resource_manager.h"

#include "ngl/gfx/mesh_loader_assimp.h"
#include "ngl/gfx/texture_loader_directxtex.h"

namespace ngl
{
namespace res
{
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

		bool is_dds = true;
		{
			const auto* file_name = p_res->GetFileName();
			const auto file_name_lenght = strlen(file_name);
			if(4 < file_name_lenght)
			{
				is_dds = (0==strncmp((file_name + file_name_lenght-4), ".dds", 4));
			}
		}

		if(is_dds)
		{
			// DDS ロード.
			if(!directxtex::LoadImageData_DDS(image_data, meta_data, p_device, p_res->GetFileName()))
				return false;
		}
		else
		{
			// WIC ロード.
			if(!directxtex::LoadImageData_WIC(image_data, meta_data, p_device, p_res->GetFileName()))
				return false;
		}
		
		const rhi::EResourceFormat image_format = rhi::ConvertResourceFormat(meta_data.format);

		// Upload用CPU側データ.
		{
			// メモリ確保.
			p_res->upload_pixel_memory_.resize(image_data.GetPixelsSize());
			// ピクセルデータを作業用にコピー.
			memcpy(p_res->upload_pixel_memory_.data(), image_data.GetPixels(), image_data.GetPixelsSize());

			
			// Sliceのデータ.
			p_res->upload_subresource_info_array.resize(image_data.GetImageCount());
			// 適切なSubresource情報収集.
			// Cubemapの場合は面1つがarray要素1つに対応するため, array sizeは6の倍数.
			for(int array_index = 0; array_index < meta_data.arraySize; ++array_index)
			{
				for(int mip_index = 0; mip_index < meta_data.mipLevels; ++mip_index)
				{
					for(int depth_index = 0; depth_index < meta_data.depth; ++depth_index)
					{
						const auto& image_plane = *image_data.GetImage(mip_index, array_index, depth_index);
						
						const auto subresource_index = meta_data.CalculateSubresource(mip_index, array_index, depth_index);

						auto& image_plane_data = p_res->upload_subresource_info_array[subresource_index];
						{
							image_plane_data.array_index = array_index;
							image_plane_data.mip_index = mip_index;
							image_plane_data.slice_index = depth_index;
						
							image_plane_data.format = image_format;
							image_plane_data.width = static_cast<s32>(image_plane.width);
							image_plane_data.height = static_cast<s32>(image_plane.height);
							image_plane_data.rowPitch = static_cast<s32>(image_plane.rowPitch);
							image_plane_data.slicePitch = static_cast<s32>(image_plane.slicePitch);
							// 作業メモリ上の位置をセット.
							image_plane_data.pixels = p_res->upload_pixel_memory_.data() + std::distance(image_data.GetPixels(), image_plane.pixels);
						}
					}
				}
			}
		}

		
		// resにオブジェクト生成.
		p_res->ref_texture_ = new rhi::TextureDep();
		p_res->ref_view_ = new rhi::ShaderResourceViewDep();
		{
			rhi::TextureDep::Desc tex_desc = {};
			{
				tex_desc.heap_type = rhi::EResourceHeapType::Default;// GPU読み取り用.
				tex_desc.bind_flag = rhi::ResourceBindFlag::ShaderResource;// シェーダリソース用途.
			
				tex_desc.format = image_format;
				tex_desc.width = static_cast<u32>(meta_data.width);
				tex_desc.height = static_cast<u32>(meta_data.height);
				tex_desc.depth = static_cast<u32>(meta_data.depth);
				{
					if(meta_data.IsCubemap())
						tex_desc.type = rhi::ETextureType::TextureCube;
					else if(DirectX::TEX_DIMENSION_TEXTURE1D == meta_data.dimension)
						tex_desc.type = rhi::ETextureType::Texture1D;
					else if(DirectX::TEX_DIMENSION_TEXTURE2D == meta_data.dimension)
						tex_desc.type = rhi::ETextureType::Texture2D;
					else if(DirectX::TEX_DIMENSION_TEXTURE3D == meta_data.dimension)
						tex_desc.type = rhi::ETextureType::Texture3D;
				}
				tex_desc.array_size = static_cast<s32>(meta_data.arraySize);
				tex_desc.mip_count = static_cast<s32>(meta_data.mipLevels);
			}
			p_res->ref_texture_->Initialize(p_device, tex_desc);
			p_res->ref_view_->InitializeAsTexture(p_device, p_res->ref_texture_.Get(), 0, tex_desc.mip_count, 0, tex_desc.array_size);
		}
		
		return true;
	}
	
}
}