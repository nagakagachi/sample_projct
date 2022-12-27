
#include "mesh_loader_assimp.h"

#include <numeric>

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

	namespace
	{
		constexpr int CalcAlignedSize(int size, int align)
		{
			return ((size + (align - 1)) / align) * align;
		}

		// 生成用Lambda.
		auto CreateShapeDataRhiBuffer(
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
		}
	}

	// Assimpを利用してファイルから1Meshを構成するShape群を生成.
	void LoadMeshData(rhi::DeviceDep* p_device, const char* filename, gfx::MeshData& out_mesh)
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


		struct ShapeDataOffsetInfo
		{
			ShapeDataOffsetInfo()
			{
				total_size_in_byte = 0;

				num_prim = 0;
				num_vertex = 0;

				num_color_ch = 0;
				num_uv_ch = 0;

				offset_position = -1;
				offset_normal = -1;
				offset_tangent = -1;
				offset_binormal = -1;

				offset_color.fill(-1);
				offset_uv.fill(-1);

				offset_index = -1;
			}

			int total_size_in_byte;

			int num_prim;
			int num_vertex;

			int offset_position;
			int offset_normal;
			int offset_tangent;
			int offset_binormal;

			int					num_color_ch;
			std::array<int, 8>	offset_color;
			int					num_uv_ch;
			std::array<int, 8>	offset_uv;

			int offset_index;
		};


		std::vector<ShapeDataOffsetInfo> offset_info;

		// 総数計算.
		constexpr int vtx_align = 16;
		int total_size_in_byte = 0;
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

			offset_info.push_back({});
			auto& info = offset_info.back();
			info.num_prim = num_prim;
			info.num_vertex = num_vertex;

			info.num_color_ch = num_color_ch;
			info.num_uv_ch = num_uv_ch;


			// 総サイズとオフセット計算.
			info.total_size_in_byte = 0;
			// Position.
			info.offset_position = info.total_size_in_byte + total_size_in_byte;
			info.total_size_in_byte = CalcAlignedSize(info.total_size_in_byte, vtx_align) + num_position * sizeof(ngl::math::Vec3);
			// Normal.
			if (0 < num_normal)
			{
				info.offset_normal = info.total_size_in_byte + total_size_in_byte;
				info.total_size_in_byte = CalcAlignedSize(info.total_size_in_byte, vtx_align) + num_normal * sizeof(ngl::math::Vec3);
			}
			// Tangent.
			if (0 < num_tangent)
			{
				info.offset_tangent = info.total_size_in_byte + total_size_in_byte;
				info.total_size_in_byte = CalcAlignedSize(info.total_size_in_byte, vtx_align) + num_tangent * sizeof(ngl::math::Vec3);
			}
			// Binormal.
			if (0 < num_binormal)
			{
				info.offset_binormal = info.total_size_in_byte + total_size_in_byte;
				info.total_size_in_byte = CalcAlignedSize(info.total_size_in_byte, vtx_align) + num_binormal * sizeof(ngl::math::Vec3);
			}
			// Color.
			for (auto ci = 0; ci < num_color_ch; ++ci)
			{
				info.offset_color[ci] = info.total_size_in_byte + total_size_in_byte;
				info.total_size_in_byte = CalcAlignedSize(info.total_size_in_byte, vtx_align) + num_vertex * sizeof(ngl::gfx::VertexColor);
			}
			// UV.
			for (auto ci = 0; ci < num_uv_ch; ++ci)
			{
				info.offset_uv[ci] = info.total_size_in_byte + total_size_in_byte;
				info.total_size_in_byte = CalcAlignedSize(info.total_size_in_byte, vtx_align) + num_vertex * sizeof(ngl::math::Vec2);
			}
			// Index.
			info.offset_index = info.total_size_in_byte + total_size_in_byte;
			info.total_size_in_byte = CalcAlignedSize(info.total_size_in_byte, vtx_align) + num_prim * sizeof(uint32_t) * 3;

			total_size_in_byte += info.total_size_in_byte;
		}

		// このメッシュの全情報を格納するメモリを確保.
		out_mesh.raw_data_mem_.resize(total_size_in_byte);
		// 必要分のShapeを確保.
		out_mesh.shape_array_.resize(offset_info.size());

		// Raw Dataメモリのマッピング.
		for (int i = 0; i < offset_info.size(); ++i)
		{
			const auto& info = offset_info[i];
			auto& mesh = out_mesh.shape_array_[i];

			mesh.num_primitive_ = info.num_prim;
			mesh.num_vertex_ = info.num_vertex;

			uint8_t* ptr = out_mesh.raw_data_mem_.data();

			// 確保メモリへ頂点属性オフセットマッピング.
			{
				mesh.position_.raw_ptr_ = (ngl::math::Vec3*)&ptr[info.offset_position];
				if (0 <= info.offset_normal)
					mesh.normal_.raw_ptr_ = (ngl::math::Vec3*)&ptr[info.offset_normal];
				if (0 <= info.offset_tangent)
					mesh.tangent_.raw_ptr_ = (ngl::math::Vec3*)&ptr[info.offset_tangent];
				if (0 <= info.offset_binormal)
					mesh.binormal_.raw_ptr_ = (ngl::math::Vec3*)&ptr[info.offset_binormal];

				for (int ci = 0; ci < info.num_color_ch; ++ci)
				{
					mesh.color_.push_back({});
					mesh.color_.back().raw_ptr_ = (ngl::gfx::VertexColor*)&ptr[info.offset_color[ci]];
				}
				for (int ci = 0; ci < info.num_uv_ch; ++ci)
				{
					mesh.texcoord_.push_back({});
					mesh.texcoord_.back().raw_ptr_ = (ngl::math::Vec2*)&ptr[info.offset_uv[ci]];
				}
				mesh.index_.raw_ptr_ = (uint32_t*)&ptr[info.offset_index];
			}
		}

		// Copy Meshdata.
		for (int i = 0; i < offset_info.size(); ++i)
		{
			const auto& info = offset_info[i];
			auto& mesh = out_mesh.shape_array_[i];

			if (mesh.position_.raw_ptr_)
			{
				memcpy(mesh.position_.raw_ptr_, ai_scene->mMeshes[i]->mVertices, sizeof(float) * 3 * info.num_vertex);
			}
			if (mesh.normal_.raw_ptr_)
			{
				memcpy(mesh.normal_.raw_ptr_, ai_scene->mMeshes[i]->mNormals, sizeof(float) * 3 * info.num_vertex);
			}
			if (mesh.tangent_.raw_ptr_)
			{
				memcpy(mesh.tangent_.raw_ptr_, ai_scene->mMeshes[i]->mTangents, sizeof(float) * 3 * info.num_vertex);
			}
			if (mesh.binormal_.raw_ptr_)
			{
				memcpy(mesh.binormal_.raw_ptr_, ai_scene->mMeshes[i]->mBitangents, sizeof(float) * 3 * info.num_vertex);
			}

			// AssimpのVertexColorは32bit float のため変換.
			for (int ci = 0; ci < info.num_color_ch; ++ci)
			{
				auto* p_src = ai_scene->mMeshes[i]->mColors[ci];
				for (int vi = 0; vi < info.num_vertex; ++vi)
				{
					mesh.color_[ci].raw_ptr_[vi].r = uint8_t(p_src[vi].r * 255);
					mesh.color_[ci].raw_ptr_[vi].g = uint8_t(p_src[vi].g * 255);
					mesh.color_[ci].raw_ptr_[vi].b = uint8_t(p_src[vi].b * 255);
					mesh.color_[ci].raw_ptr_[vi].a = uint8_t(p_src[vi].a * 255);
				}
			}

			for (int ci = 0; ci < info.num_uv_ch; ++ci)
			{
				auto* p_src = ai_scene->mMeshes[i]->mTextureCoords[ci];
				for (int vi = 0; vi < info.num_vertex; ++vi)
				{
					mesh.texcoord_[ci].raw_ptr_[vi] = { p_src[vi].x, p_src[vi].y };
				}
			}

			for (uint32_t face_i = 0; face_i < ai_scene->mMeshes[i]->mNumFaces; ++face_i)
			{
				// 三角化前提.
				const auto p_face_index = ai_scene->mMeshes[i]->mFaces[face_i].mIndices;

				mesh.index_.raw_ptr_[face_i * 3 + 0] = p_face_index[0];
				mesh.index_.raw_ptr_[face_i * 3 + 1] = p_face_index[1];
				mesh.index_.raw_ptr_[face_i * 3 + 2] = p_face_index[2];
			}
		}

		// Create Rhi.
		for (int i = 0; i < offset_info.size(); ++i)
		{
			const auto& info = offset_info[i];
			auto& mesh = out_mesh.shape_array_[i];

			// Vertex Attribute.
			{
				CreateShapeDataRhiBuffer(
					&mesh.position_.rhi_buffer_,
					&mesh.position_.rhi_srv,
					&mesh.position_.rhi_vbv_,
					nullptr,
					p_device, ngl::rhi::ResourceBindFlag::VertexBuffer, rhi::ResourceFormat::Format_R32G32B32_FLOAT, sizeof(ngl::math::Vec3), mesh.num_vertex_,
					mesh.position_.raw_ptr_);
			}

			if (mesh.normal_.raw_ptr_)
			{
				CreateShapeDataRhiBuffer(
					&mesh.normal_.rhi_buffer_,
					&mesh.normal_.rhi_srv,
					&mesh.normal_.rhi_vbv_,
					nullptr,
					p_device, ngl::rhi::ResourceBindFlag::VertexBuffer, rhi::ResourceFormat::Format_R32G32B32_FLOAT, sizeof(ngl::math::Vec3), mesh.num_vertex_,
					mesh.normal_.raw_ptr_);

				rhi::VertexBufferViewDep::Desc vbv_desc = {};
				mesh.normal_.rhi_vbv_.Initialize(&mesh.normal_.rhi_buffer_, vbv_desc);
			}
			if (mesh.tangent_.raw_ptr_)
			{
				CreateShapeDataRhiBuffer(
					&mesh.tangent_.rhi_buffer_,
					&mesh.tangent_.rhi_srv,
					&mesh.tangent_.rhi_vbv_,
					nullptr,
					p_device, ngl::rhi::ResourceBindFlag::VertexBuffer, rhi::ResourceFormat::Format_R32G32B32_FLOAT, sizeof(ngl::math::Vec3), mesh.num_vertex_,
					mesh.tangent_.raw_ptr_);

				rhi::VertexBufferViewDep::Desc vbv_desc = {};
				mesh.tangent_.rhi_vbv_.Initialize(&mesh.tangent_.rhi_buffer_, vbv_desc);
			}
			if (mesh.binormal_.raw_ptr_)
			{
				CreateShapeDataRhiBuffer(
					&mesh.binormal_.rhi_buffer_,
					&mesh.binormal_.rhi_srv,
					&mesh.binormal_.rhi_vbv_,
					nullptr,
					p_device, ngl::rhi::ResourceBindFlag::VertexBuffer, rhi::ResourceFormat::Format_R32G32B32_FLOAT, sizeof(ngl::math::Vec3), mesh.num_vertex_,
					mesh.binormal_.raw_ptr_);

				rhi::VertexBufferViewDep::Desc vbv_desc = {};
				mesh.binormal_.rhi_vbv_.Initialize(&mesh.binormal_.rhi_buffer_, vbv_desc);
			}
			// SRGBかLinearで問題になるかもしれない. 現状はとりあえずLinear扱い.
			for (int ci = 0; ci < mesh.color_.size(); ++ci)
			{
				CreateShapeDataRhiBuffer(
					&mesh.color_[ci].rhi_buffer_,
					&mesh.color_[ci].rhi_srv,
					&mesh.color_[ci].rhi_vbv_,
					nullptr,
					p_device, ngl::rhi::ResourceBindFlag::VertexBuffer, rhi::ResourceFormat::Format_R8G8B8A8_UNORM, sizeof(ngl::gfx::VertexColor), mesh.num_vertex_,
					mesh.color_[ci].raw_ptr_);

				rhi::VertexBufferViewDep::Desc vbv_desc = {};
				mesh.color_[ci].rhi_vbv_.Initialize(&mesh.color_[ci].rhi_buffer_, vbv_desc);
			}
			for (int ci = 0; ci < mesh.texcoord_.size(); ++ci)
			{
				CreateShapeDataRhiBuffer(
					&mesh.texcoord_[ci].rhi_buffer_,
					&mesh.texcoord_[ci].rhi_srv,
					&mesh.texcoord_[ci].rhi_vbv_,
					nullptr,
					p_device, ngl::rhi::ResourceBindFlag::VertexBuffer, rhi::ResourceFormat::Format_R32G32_FLOAT, sizeof(ngl::math::Vec2), mesh.num_vertex_,
					mesh.texcoord_[ci].raw_ptr_);

				rhi::VertexBufferViewDep::Desc vbv_desc = {};
				mesh.texcoord_[ci].rhi_vbv_.Initialize(&mesh.texcoord_[ci].rhi_buffer_, vbv_desc);
			}

			// Index.
			{
				CreateShapeDataRhiBuffer(
					&mesh.index_.rhi_buffer_,
					&mesh.index_.rhi_srv,
					nullptr,
					&mesh.index_.rhi_vbv_,
					p_device, ngl::rhi::ResourceBindFlag::IndexBuffer, rhi::ResourceFormat::Format_R32_UINT, sizeof(uint32_t), mesh.num_primitive_ * 3,
					mesh.index_.raw_ptr_);
			}
		}
	}

	bool LoadMeshData(gfx::ResMeshData& out, rhi::DeviceDep* p_device, const char* filename)
	{
		LoadMeshData(p_device, filename, out.data_);

		return (0 < out.data_.raw_data_mem_.size());
	}
}
}
