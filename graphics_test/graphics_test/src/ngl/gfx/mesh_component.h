#pragma once

#include <vector>

#include "ngl/math/math.h"

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
		class MeshAssetVertexData
		{
		public:
			MeshAssetVertexData()
			{
			}
			~MeshAssetVertexData()
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
		class MeshAssetIndexData
		{
		public:
			MeshAssetIndexData()
			{
			}
			~MeshAssetIndexData()
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

		// Mesh Asset.
		class MeshAssetData
		{
		public:
			MeshAssetData()
			{
			}
			~MeshAssetData()
			{
			}

			// MeshDataメモリ.
			std::vector<uint8_t> raw_data_mem_;

			int num_vertex_ = 0;
			int num_primitive_ = 0;

			MeshAssetIndexData<uint32_t> index_ = {};

			MeshAssetVertexData<math::Vec3> position_ = {};
			MeshAssetVertexData<math::Vec3> normal_ = {};
			MeshAssetVertexData<math::Vec3> tangent_ = {};
			MeshAssetVertexData<math::Vec3> binormal_ = {};
			std::vector<MeshAssetVertexData<VertexColor>>	color_;
			std::vector<MeshAssetVertexData<math::Vec2>>	texcoord_;
		};


		class MeshComponent
		{
		public:
			MeshComponent();
			~MeshComponent();

			
			void SetTransform(const math::Mat34& m)
			{
				mat_ = m;
			}
			math::Mat34 GetTransform() const
			{
				return mat_;
			}

		private:
			math::Mat34	mat_;



		};

	}
}
