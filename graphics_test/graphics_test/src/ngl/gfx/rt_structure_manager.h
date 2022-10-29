#pragma once

#include <cstring>

#include "ngl/rhi/d3d12/rhi.d3d12.h"
#include "ngl/rhi/d3d12/rhi_command_list.d3d12.h"
#include "ngl/rhi/d3d12/rhi_resource.d3d12.h"
#include "ngl/rhi/d3d12/rhi_resource_view.d3d12.h"

namespace ngl
{
	namespace gfx
	{
		struct Mat34
		{
			float m[3][4];
			
			static constexpr Mat34 Identity()
			{
				constexpr Mat34 m = {
					1.0f, 0.0f, 0.0f, 0.0f,
					0.0f, 1.0f, 0.0f, 0.0f,
					0.0f, 0.0f, 1.0f, 0.0f,
				};
				return m;
			}
		};



		// BLAS.
		class RaytraceStructureBottom
		{
		public:
			enum SETUP_TYPE : int
			{
				NONE,
				BLAS_TRIANGLE,
			};

			RaytraceStructureBottom();
			~RaytraceStructureBottom();

			// BLAS setup.
			// index_buffer : optional.
			// bufferの管理責任は外部.
			bool Setup(rhi::BufferDep* vertex_buffer, rhi::BufferDep* index_buffer = nullptr);

			// SetupAs... の情報を元に構造構築コマンドを発行する.
			// Buildタイミングをコントロールするために分離している.
			// TODO RenderDocでのLaunchはクラッシュするのでNsight推奨.
			bool Build(rhi::DeviceDep* p_device, rhi::GraphicsCommandListDep* p_command_list);

			bool IsSetuped() const;
			bool IsBuilt() const;

			rhi::BufferDep* GetBuffer();
			const rhi::BufferDep* GetBuffer() const;

		private:
			bool is_built_ = false;

			// setup data.
			SETUP_TYPE		setup_type_ = SETUP_TYPE::NONE;
			rhi::BufferDep* p_vertex_buffer_ = nullptr;
			rhi::BufferDep* p_index_buffer_ = nullptr;

			// built data.
			rhi::BufferDep scratch_;
			rhi::BufferDep main_;

			// for TLAS only.
			rhi::BufferDep instance_desc_;
		};

		// TLAS.
		class RaytraceStructureTop
		{
		public:
			enum SETUP_TYPE : int
			{
				NONE,
				TLAS,
			};

			RaytraceStructureTop();
			~RaytraceStructureTop();

			// TLAS setup.
			// index_buffer : optional.
			// bufferの管理責任は外部.
			bool Setup(RaytraceStructureBottom* p_blas, const std::vector<Mat34>& instance_transform_array);

			// SetupAs... の情報を元に構造構築コマンドを発行する.
			// Buildタイミングをコントロールするために分離している.
			// TODO RenderDocでのLaunchはクラッシュするのでNsight推奨.
			bool Build(rhi::DeviceDep* p_device, rhi::GraphicsCommandListDep* p_command_list);

			bool IsSetuped() const;
			bool IsBuilt() const;
			rhi::BufferDep* GetBuffer();
			const rhi::BufferDep* GetBuffer() const;

		private:
			bool is_built_ = false;

			// setup data.
			SETUP_TYPE		setup_type_ = SETUP_TYPE::NONE;
			RaytraceStructureBottom* p_blas_ = nullptr;
			std::vector<Mat34> transform_array_;


			// built data.
			rhi::BufferDep instance_buffer_;
			rhi::BufferDep scratch_;
			rhi::BufferDep main_;
			rhi::ShaderResourceViewDep main_srv_;
			int tlas_byte_size_ = 0;
		};


		class RaytraceStructureManager
		{
		public:
			RaytraceStructureManager();
			~RaytraceStructureManager();

			bool Initialize(rhi::DeviceDep* p_device);
			void UpdateOnRender(rhi::DeviceDep* p_device, rhi::GraphicsCommandListDep* p_command_list);

		private:
			rhi::BufferDep test_geom_vb_;

			RaytraceStructureBottom test_blas_;

			RaytraceStructureTop test_tlas_;

		};

	}
}