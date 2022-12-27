#pragma once

#include <vector>

#include "ngl/math/math.h"
#include "ngl/util/noncopyable.h"
#include "ngl/util/singleton.h"


#include "ngl/resource/resource.h"

#include "ngl/rhi/d3d12/rhi.d3d12.h"
#include "ngl/rhi/d3d12/rhi_resource.d3d12.h"
#include "ngl/rhi/d3d12/rhi_resource_view.d3d12.h"


namespace ngl
{
	namespace gfx
	{
		struct VertexColor
		{
			uint32_t r : 8;
			uint32_t g : 8;
			uint32_t b : 8;
			uint32_t a : 8;
		};

		template<typename T>
		class MeshShapeVertexData
		{
		public:
			MeshShapeVertexData()
			{
			}
			~MeshShapeVertexData()
			{
			}

			// raw data ptr.
			T* raw_ptr_ = nullptr;
			// rhi buffer.
			rhi::BufferDep	rhi_buffer_ = {};
			// rhi vertex buffer view.
			rhi::VertexBufferViewDep rhi_vbv_ = {};
			// rhi srv.
			rhi::ShaderResourceViewDep rhi_srv = {};
		};
		template<typename T>
		class MeshShapeIndexData
		{
		public:
			MeshShapeIndexData()
			{
			}
			~MeshShapeIndexData()
			{
			}

			// raw data ptr.
			T* raw_ptr_ = nullptr;
			// rhi buffer.
			rhi::BufferDep	rhi_buffer_ = {};
			// rhi index buffer view.
			rhi::IndexBufferViewDep rhi_vbv_ = {};
			// rhi srv.
			rhi::ShaderResourceViewDep rhi_srv = {};
		};

		// Mesh Shape Data.
		// 頂点属性の情報のメモリ実態は親のMeshDataが保持し, ロードや初期化でマッピングされる.
		class MeshShapePart
		{
		public:
			MeshShapePart()
			{
			}
			~MeshShapePart()
			{
			}

			int num_vertex_ = 0;
			int num_primitive_ = 0;

			MeshShapeIndexData<uint32_t> index_ = {};
			MeshShapeVertexData<math::Vec3> position_ = {};
			MeshShapeVertexData<math::Vec3> normal_ = {};
			MeshShapeVertexData<math::Vec3> tangent_ = {};
			MeshShapeVertexData<math::Vec3> binormal_ = {};
			std::vector<MeshShapeVertexData<VertexColor>>	color_;
			std::vector<MeshShapeVertexData<math::Vec2>>	texcoord_;
		};

		// Mesh Shape Data.
		class MeshData
		{
		public:
			MeshData()
			{
			}
			~MeshData()
			{
			}

			// ジオメトリ情報のRawDataメモリ. 個々でメモリ確保してマッピングする場合に利用.
			std::vector<uint8_t> raw_data_mem_;
			// RawData以外にもRHIBufferも一纏めにする場合はここで管理してshapeのviewが参照することもできるかもしれない.

			std::vector<MeshShapePart> shape_array_;
		};





		// Mesh Resource.
		class ResMeshData : public res::Resource
		{
			NGL_RES_MEMBER_DECLARE(ResMeshData)

		public:
			ResMeshData()
			{
			}
			~ResMeshData()
			{
				std::cout << "[ResMeshData] Destruct " << this << std::endl;
			}

			MeshData data_ = {};
		};
	}
}
