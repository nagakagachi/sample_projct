
#include "mesh_loader_assimp.h"


#include "ngl/math/math.h"

// rhi
#include "ngl/rhi/d3d12/rhi_resource.d3d12.h"
#include "ngl/rhi/d3d12/rhi_resource_view.d3d12.h"


// assimpテスト.
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags


namespace ngl
{
namespace assimp
{

	void LoadMeshData(rhi::DeviceDep* p_device, const char* filename, std::vector<gfx::MeshAssetData*>& out_mesh)
	{
		// ReadFileで読み込まれたメモリ等はAssimp::Importerインスタンスの寿命でクリーンアップされる.
		Assimp::Importer asimporter;
		const aiScene* ai_scene = asimporter.ReadFile(filename,

			aiProcess_CalcTangentSpace |
			aiProcess_Triangulate |
			aiProcess_JoinIdenticalVertices |
			aiProcess_SortByPType
		);
		if (!ai_scene)
			return;

		// MeshAssetDataテスト.
		for (auto mesh_i = 0u; mesh_i < ai_scene->mNumMeshes; ++mesh_i)
		{
			// assume triangle.
			const int num_prim = ai_scene->mMeshes[mesh_i]->mNumFaces;
			const int num_vertex = ai_scene->mMeshes[mesh_i]->mNumVertices;

			const int num_color_ch = ai_scene->mMeshes[mesh_i]->GetNumColorChannels();
			const int num_uv_ch = ai_scene->mMeshes[mesh_i]->GetNumUVChannels();
			const int num_position = ai_scene->mMeshes[mesh_i]->mVertices ? num_vertex : 0;
			const int num_normal = ai_scene->mMeshes[mesh_i]->mNormals ? num_vertex : 0;
			const int num_tangent = ai_scene->mMeshes[mesh_i]->mTangents ? num_vertex : 0;
			const int num_binormal = ai_scene->mMeshes[mesh_i]->mBitangents ? num_vertex : 0;

			// 総サイズとオフセット計算.
			int asset_size_in_byte = 0; ;
			// Position.
			int offset_position = asset_size_in_byte;
			asset_size_in_byte += num_position * sizeof(ngl::math::Vec3);
			// Normal.
			int offset_normal = -1;
			if (0 < num_normal)
			{
				offset_normal = asset_size_in_byte;
				asset_size_in_byte += num_normal * sizeof(ngl::math::Vec3);
			}
			// Tangent.
			int offset_tangent = -1;
			if (0 < num_tangent)
			{
				offset_tangent = asset_size_in_byte;
				asset_size_in_byte += num_tangent * sizeof(ngl::math::Vec3);
			}
			// Binormal.
			int offset_binormal = -1;
			if (0 < num_binormal)
			{
				offset_binormal = asset_size_in_byte;
				asset_size_in_byte += num_binormal * sizeof(ngl::math::Vec3);
			}
			// Color.
			std::vector<int> offset_color;
			for (auto ci = 0; ci < num_color_ch; ++ci)
			{
				offset_color.push_back(asset_size_in_byte);
				asset_size_in_byte += num_vertex * sizeof(ngl::gfx::VertexColor);
			}
			// UV.
			std::vector<int> offset_uv;
			for (auto ci = 0; ci < num_uv_ch; ++ci)
			{
				offset_uv.push_back(asset_size_in_byte);
				asset_size_in_byte += num_vertex * sizeof(ngl::math::Vec2);
			}
			// Index.
			int offset_index = asset_size_in_byte;
			asset_size_in_byte += num_prim * sizeof(uint32_t) * 3;


			auto* p_mesh_data = new ngl::gfx::MeshAssetData();
			// Database登録.
			out_mesh.push_back(p_mesh_data);

			// MeshAssetDataメモリ確保とオフセットマッピング.
			{
				p_mesh_data->num_vertex_ = num_vertex;
				p_mesh_data->num_primitive_ = num_prim;

				// raw data 用確保.
				p_mesh_data->raw_data_mem_.resize(asset_size_in_byte);
				uint8_t* ptr = p_mesh_data->raw_data_mem_.data();

				// 確保メモリへ頂点属性オフセットマッピング.
				{
					p_mesh_data->position_.raw_ptr_ = (ngl::math::Vec3*)&ptr[offset_position];
					if (0 <= offset_normal)
						p_mesh_data->normal_.raw_ptr_ = (ngl::math::Vec3*)&ptr[offset_normal];
					if (0 <= offset_tangent)
						p_mesh_data->tangent_.raw_ptr_ = (ngl::math::Vec3*)&ptr[offset_tangent];
					if (0 <= offset_binormal)
						p_mesh_data->binormal_.raw_ptr_ = (ngl::math::Vec3*)&ptr[offset_binormal];

					for (int ci = 0; ci < offset_color.size(); ++ci)
					{
						p_mesh_data->color_.push_back({});
						p_mesh_data->color_.back().raw_ptr_ = (ngl::gfx::VertexColor*)&ptr[offset_color[ci]];
					}
					for (int ci = 0; ci < offset_uv.size(); ++ci)
					{
						p_mesh_data->texcoord_.push_back({});
						p_mesh_data->texcoord_.back().raw_ptr_ = (ngl::math::Vec2*)&ptr[offset_uv[ci]];
					}
					p_mesh_data->index_.raw_ptr_ = (uint32_t*)&ptr[offset_index];
				}

				// Copy to Meshdata.
				{
					if (p_mesh_data->position_.raw_ptr_)
					{
						memcpy(p_mesh_data->position_.raw_ptr_, ai_scene->mMeshes[mesh_i]->mVertices, sizeof(float) * 3 * num_position);
					}
					if (p_mesh_data->normal_.raw_ptr_)
					{
						memcpy(p_mesh_data->normal_.raw_ptr_, ai_scene->mMeshes[mesh_i]->mNormals, sizeof(float) * 3 * num_normal);
					}
					if (p_mesh_data->tangent_.raw_ptr_)
					{
						memcpy(p_mesh_data->tangent_.raw_ptr_, ai_scene->mMeshes[mesh_i]->mTangents, sizeof(float) * 3 * num_tangent);
					}
					if (p_mesh_data->binormal_.raw_ptr_)
					{
						memcpy(p_mesh_data->binormal_.raw_ptr_, ai_scene->mMeshes[mesh_i]->mBitangents, sizeof(float) * 3 * num_binormal);
					}

					// AssimpのVertexColorは32bit float のため変換.
					for (int ci = 0; ci < p_mesh_data->color_.size(); ++ci)
					{
						auto* p_src = ai_scene->mMeshes[mesh_i]->mColors[ci];
						for (int vi = 0; vi < num_vertex; ++vi)
						{
							p_mesh_data->color_[ci].raw_ptr_[vi].r = uint8_t(p_src[vi].r * 255);
							p_mesh_data->color_[ci].raw_ptr_[vi].g = uint8_t(p_src[vi].g * 255);
							p_mesh_data->color_[ci].raw_ptr_[vi].b = uint8_t(p_src[vi].b * 255);
							p_mesh_data->color_[ci].raw_ptr_[vi].a = uint8_t(p_src[vi].a * 255);
						}
					}

					for (int ci = 0; ci < p_mesh_data->texcoord_.size(); ++ci)
					{
						auto* p_src = ai_scene->mMeshes[mesh_i]->mTextureCoords[ci];
						for (int vi = 0; vi < num_vertex; ++vi)
						{
							p_mesh_data->texcoord_[ci].raw_ptr_[vi] = { p_src[vi].x, p_src[vi].y };
						}
					}

					for (uint32_t face_i = 0; face_i < ai_scene->mMeshes[mesh_i]->mNumFaces; ++face_i)
					{
						// 三角化前提.
						const auto p_face_index = ai_scene->mMeshes[mesh_i]->mFaces[face_i].mIndices;

						p_mesh_data->index_.raw_ptr_[face_i * 3 + 0] = p_face_index[0];
						p_mesh_data->index_.raw_ptr_[face_i * 3 + 1] = p_face_index[1];
						p_mesh_data->index_.raw_ptr_[face_i * 3 + 2] = p_face_index[2];
					}
				}
			}

			// RHIオブジェクト生成.
			{
				// 生成用Lambda.
				auto func_gen_rhi_buffer = 
					[](
						ngl::rhi::BufferDep* p_out_buffer, 
						ngl::rhi::ShaderResourceViewDep* p_out_view, 
						ngl::rhi::VertexBufferViewDep* p_out_vbv, 
						ngl::rhi::IndexBufferViewDep* p_out_ibv,
						ngl::rhi::DeviceDep* p_device,
						uint32_t bind_flag, rhi::ResourceFormat view_format, int element_size_in_byte, int element_count, void* initial_data = nullptr)
					-> bool
				{
					ngl::rhi::BufferDep::Desc buffer_desc = {};
					buffer_desc.heap_type = ngl::rhi::ResourceHeapType::Upload;
					buffer_desc.initial_state = ngl::rhi::ResourceState::General;
					// RT用のShaderResource.
					buffer_desc.bind_flag = bind_flag | ngl::rhi::ResourceBindFlag::ShaderResource;
					buffer_desc.element_count = element_count;
					buffer_desc.element_byte_size = element_size_in_byte;


					if (!p_out_buffer->Initialize(p_device, buffer_desc))
					{
						assert(false);
						return false;
					}
					if (initial_data)
					{
						if (void* mapped = p_out_buffer->Map())
						{
							memcpy(mapped, initial_data, element_size_in_byte * element_count);
							p_out_buffer->Unmap();
						}
					}

					bool result = true;
					if (p_out_view)
					{
						if (!p_out_view->InitializeAsTyped(p_device, p_out_buffer, view_format, 0, p_out_buffer->getElementCount()))
						{
							assert(false);
							result = false;
						}
					}
					if (p_out_vbv)
					{
						rhi::VertexBufferViewDep::Desc vbv_desc = {};
						if (!p_out_vbv->Initialize(p_out_buffer, vbv_desc))
						{
							assert(false);
							result = false;
						}
					}
					if (p_out_ibv)
					{
						rhi::IndexBufferViewDep::Desc ibv_desc = {};
						if (!p_out_ibv->Initialize(p_out_buffer, ibv_desc))
						{
							assert(false);
							result = false;
						}
					}
					return result;
				};

				// Vertex Attribute.

				{
					func_gen_rhi_buffer(
						&p_mesh_data->position_.rhi_buffer_, 
						&p_mesh_data->position_.rhi_srv, 
						&p_mesh_data->position_.rhi_vbv_, 
						nullptr,
						p_device, ngl::rhi::ResourceBindFlag::VertexBuffer, rhi::ResourceFormat::Format_R32G32B32_FLOAT, sizeof(ngl::math::Vec3), p_mesh_data->num_vertex_,
						p_mesh_data->position_.raw_ptr_);
				}

				if (p_mesh_data->normal_.raw_ptr_)
				{
					func_gen_rhi_buffer(
						&p_mesh_data->normal_.rhi_buffer_, 
						&p_mesh_data->normal_.rhi_srv,
						&p_mesh_data->normal_.rhi_vbv_,
						nullptr,
						p_device, ngl::rhi::ResourceBindFlag::VertexBuffer, rhi::ResourceFormat::Format_R32G32B32_FLOAT, sizeof(ngl::math::Vec3), p_mesh_data->num_vertex_,
						p_mesh_data->normal_.raw_ptr_);

					rhi::VertexBufferViewDep::Desc vbv_desc = {};
					p_mesh_data->normal_.rhi_vbv_.Initialize(&p_mesh_data->normal_.rhi_buffer_, vbv_desc);
				}
				if (p_mesh_data->tangent_.raw_ptr_)
				{
					func_gen_rhi_buffer(
						&p_mesh_data->tangent_.rhi_buffer_, 
						&p_mesh_data->tangent_.rhi_srv,
						&p_mesh_data->tangent_.rhi_vbv_,
						nullptr,
						p_device, ngl::rhi::ResourceBindFlag::VertexBuffer, rhi::ResourceFormat::Format_R32G32B32_FLOAT, sizeof(ngl::math::Vec3), p_mesh_data->num_vertex_,
						p_mesh_data->tangent_.raw_ptr_);

					rhi::VertexBufferViewDep::Desc vbv_desc = {};
					p_mesh_data->tangent_.rhi_vbv_.Initialize(&p_mesh_data->tangent_.rhi_buffer_, vbv_desc);
				}
				if (p_mesh_data->binormal_.raw_ptr_)
				{
					func_gen_rhi_buffer(
						&p_mesh_data->binormal_.rhi_buffer_, 
						&p_mesh_data->binormal_.rhi_srv,
						&p_mesh_data->binormal_.rhi_vbv_,
						nullptr,
						p_device, ngl::rhi::ResourceBindFlag::VertexBuffer, rhi::ResourceFormat::Format_R32G32B32_FLOAT, sizeof(ngl::math::Vec3), p_mesh_data->num_vertex_,
						p_mesh_data->binormal_.raw_ptr_);

					rhi::VertexBufferViewDep::Desc vbv_desc = {};
					p_mesh_data->binormal_.rhi_vbv_.Initialize(&p_mesh_data->binormal_.rhi_buffer_, vbv_desc);
				}
				// SRGBかLinearで問題になるかもしれない. 現状はとりあえずLinear扱い.
				for (int ci = 0; ci < p_mesh_data->color_.size(); ++ci)
				{
					func_gen_rhi_buffer(
						&p_mesh_data->color_[ci].rhi_buffer_, 
						&p_mesh_data->color_[ci].rhi_srv,
						&p_mesh_data->color_[ci].rhi_vbv_,
						nullptr,
						p_device, ngl::rhi::ResourceBindFlag::VertexBuffer, rhi::ResourceFormat::Format_R8G8B8A8_UNORM, sizeof(ngl::gfx::VertexColor), p_mesh_data->num_vertex_,
						p_mesh_data->color_[ci].raw_ptr_);

					rhi::VertexBufferViewDep::Desc vbv_desc = {};
					p_mesh_data->color_[ci].rhi_vbv_.Initialize(&p_mesh_data->color_[ci].rhi_buffer_, vbv_desc);
				}
				for (int ci = 0; ci < p_mesh_data->texcoord_.size(); ++ci)
				{
					func_gen_rhi_buffer(
						&p_mesh_data->texcoord_[ci].rhi_buffer_,
						&p_mesh_data->texcoord_[ci].rhi_srv,
						&p_mesh_data->texcoord_[ci].rhi_vbv_,
						nullptr,
						p_device, ngl::rhi::ResourceBindFlag::VertexBuffer, rhi::ResourceFormat::Format_R32G32_FLOAT, sizeof(ngl::math::Vec2), p_mesh_data->num_vertex_,
						p_mesh_data->texcoord_[ci].raw_ptr_);

					rhi::VertexBufferViewDep::Desc vbv_desc = {};
					p_mesh_data->texcoord_[ci].rhi_vbv_.Initialize(&p_mesh_data->texcoord_[ci].rhi_buffer_, vbv_desc);
				}

				// Index.
				{
					func_gen_rhi_buffer(
						&p_mesh_data->index_.rhi_buffer_, 
						&p_mesh_data->index_.rhi_srv,
						nullptr,
						&p_mesh_data->index_.rhi_vbv_,
						p_device, ngl::rhi::ResourceBindFlag::IndexBuffer, rhi::ResourceFormat::Format_R32_UINT, sizeof(uint32_t), p_mesh_data->num_primitive_ * 3,
						p_mesh_data->index_.raw_ptr_);
				}
			}
		}
	}
}
}
