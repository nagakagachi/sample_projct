#pragma once

#include <vector>

#include "ngl/math/math.h"
#include "ngl/util/noncopyable.h"
#include "ngl/util/singleton.h"


#include "ngl/rhi/d3d12/rhi.d3d12.h"
#include "ngl/rhi/d3d12/rhi_resource.d3d12.h"
#include "ngl/rhi/d3d12/rhi_resource_view.d3d12.h"
#include "ngl/rhi/d3d12/rhi_command_list.d3d12.h"

#include "ngl/resource/resource.h"

namespace ngl
{
	namespace res
	{
		class IResourceRenderUpdater;
	}


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
		class MeshShapeGeomBufferBase
		{
		public:
			MeshShapeGeomBufferBase()
			{
			}
			virtual ~MeshShapeGeomBufferBase()
			{
			}

			void InitRender(rhi::DeviceDep* p_device, rhi::GraphicsCommandListDep* p_commandlist)
			{
				// バッファ自体が生成されていなければ終了.
				if (!ref_upload_rhibuffer_.IsValid() || !rhi_buffer_.GetD3D12Resource())
					return;

				auto* p_d3d_commandlist = p_commandlist->GetD3D12GraphicsCommandList();
				// Init Upload Buffer から DefaultHeapのBufferへコピー. StateはGeneral想定.
				p_commandlist->ResourceBarrier(&rhi_buffer_, rhi::ResourceState::General, rhi::ResourceState::CopyDst);
				p_d3d_commandlist->CopyResource(rhi_buffer_.GetD3D12Resource(), ref_upload_rhibuffer_->GetD3D12Resource());
				p_commandlist->ResourceBarrier(&rhi_buffer_, rhi::ResourceState::CopyDst, rhi::ResourceState::General);

				// upload bufferを解放.
				ref_upload_rhibuffer_.Reset();
			}

			// rhi buffer for gpu.
			rhi::BufferDep	rhi_buffer_ = {};
			// rhi srv.
			rhi::ShaderResourceViewDep rhi_srv = {};


			// raw data ptr.
			T* raw_ptr_ = nullptr;
			// 初期データバッファ. InitRenderでrhi_buffer_へコピー命令を発行した後に破棄される.
			rhi::RhiRef<rhi::BufferDep>	ref_upload_rhibuffer_ = {};

		};

		template<typename T>
		class MeshShapeVertexData : public MeshShapeGeomBufferBase<T>
		{
		public:
			MeshShapeVertexData()
			{
			}
			~MeshShapeVertexData()
			{
			}

			// rhi vertex buffer view.
			rhi::VertexBufferViewDep rhi_vbv_ = {};
		};

		template<typename T>
		class MeshShapeIndexData : public MeshShapeGeomBufferBase<T>
		{
		public:
			MeshShapeIndexData()
			{
			}
			~MeshShapeIndexData()
			{
			}

			// rhi index buffer view.
			rhi::IndexBufferViewDep rhi_vbv_ = {};
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





		// Mesh Resource 実装.
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

			void operator()(rhi::DeviceDep* p_device, rhi::GraphicsCommandListDep* p_commandlist) override;

			MeshData data_ = {};
		};
	}
}
