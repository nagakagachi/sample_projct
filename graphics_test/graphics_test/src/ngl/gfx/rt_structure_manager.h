#pragma once

#include "ngl/rhi/d3d12/rhi.d3d12.h"
#include "ngl/rhi/d3d12/rhi_resource.d3d12.h"

namespace ngl
{
	namespace gfx
	{

		class RaytraceStructureElement
		{
		public:
			enum SETUP_TYPE : int
			{
				NONE,
				BLAS_TRIANGLE,
				TLAS
			};


			bool IsBuilt() const
			{
				return is_built_;
			}

		public:
			// BLAS setup.
			// index_buffer : optional.
			// bufferの管理責任は外部.
			bool SetupAsBLAS(rhi::BufferDep* vertex_buffer, rhi::BufferDep* index_buffer = nullptr)
			{
				if (is_built_)
					return false;

				if (!vertex_buffer)
				{
					assert(false);
					return false;
				}

				setup_type_ = SETUP_TYPE::BLAS_TRIANGLE;
				p_vertex_buffer_ = vertex_buffer;
				p_index_buffer_ = index_buffer;
				
				return true;
			}

			// SetupAs... の情報を元に構造構築コマンドを発行する.
			// Buildタイミングをコントロールするために分離している.
			bool Build(rhi::DeviceDep* p_device, rhi::GraphicsCommandListDep* p_command_list)
			{
				assert(p_device);
				assert(p_command_list);

				if (is_built_)
					return false;

				if (SETUP_TYPE::NONE == setup_type_)
				{
					// セットアップされていない.
					assert(false);
					return false;
				}

				// BLAS Build (Triangle Geometry).
				if (SETUP_TYPE::BLAS_TRIANGLE == setup_type_)
				{
					assert(p_vertex_buffer_);

					D3D12_RAYTRACING_GEOMETRY_DESC geom_desc = {};
					geom_desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;	// Triangle Geom.
					geom_desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;	// Opaque.
					geom_desc.Triangles.VertexBuffer.StartAddress = p_vertex_buffer_->GetD3D12Resource()->GetGPUVirtualAddress();
					geom_desc.Triangles.VertexBuffer.StrideInBytes = p_vertex_buffer_->GetElementByteSize();// 12 byte (vec3).
					geom_desc.Triangles.VertexCount = p_vertex_buffer_->getElementCount();
					geom_desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;// vec3.


					D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS build_inputs = {};
					build_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
					build_inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
					build_inputs.NumDescs = 1;
					build_inputs.pGeometryDescs = &geom_desc;
					build_inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL; // BLAS.

					// TODO ここでRenderDocからLaunchするとクラッシュする模様.
					// Prebuildで必要なサイズ取得.
					D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO build_info = {};
					p_device->GetD3D12DeviceForDxr()->GetRaytracingAccelerationStructurePrebuildInfo(&build_inputs, &build_info);


					// PreBuild情報からバッファ生成
					// Scratch Buffer.
					rhi::BufferDep::Desc scratch_desc = {};
					scratch_desc.bind_flag = rhi::ResourceBindFlag::UnorderedAccess;
					scratch_desc.initial_state = rhi::ResourceState::UnorderedAccess;
					scratch_desc.heap_type = rhi::ResourceHeapType::Default;
					scratch_desc.element_count = 1;
					scratch_desc.element_byte_size = (u32)build_info.ScratchDataSizeInBytes;
					if (!scratch_.Initialize(p_device, scratch_desc))
					{
						std::cout << "[ERROR] Initialize Rt Scratch Buffer." << std::endl;
						assert(false);
						return false;
					}
					// Main Buffer.
					rhi::BufferDep::Desc main_desc = {};
					main_desc.bind_flag = rhi::ResourceBindFlag::UnorderedAccess;
					main_desc.initial_state = rhi::ResourceState::RaytracingAccelerationStructure;
					main_desc.heap_type = rhi::ResourceHeapType::Default;
					main_desc.element_count = 1;
					main_desc.element_byte_size = (u32)build_info.ResultDataMaxSizeInBytes;
					if (!main_.Initialize(p_device, main_desc))
					{
						std::cout << "[ERROR] Initialize Rt Main Buffer." << std::endl;
						assert(false);
						return false;
					}

					// Builld.
					D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build_desc = {};
					build_desc.Inputs = build_inputs;
					build_desc.DestAccelerationStructureData = main_.GetD3D12Resource()->GetGPUVirtualAddress();
					build_desc.ScratchAccelerationStructureData = scratch_.GetD3D12Resource()->GetGPUVirtualAddress();
					p_command_list->GetD3D12GraphicsCommandListForDxr()->BuildRaytracingAccelerationStructure(&build_desc, 0, nullptr);

					// UAV Barrier変更.
					p_command_list->ResourceUavBarrier(&main_);
				}

				is_built_ = true;

				return true;
			}

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



		class RaytraceStructureManager
		{
		public:
			RaytraceStructureManager()
			{
			}
			~RaytraceStructureManager()
			{
			}

			bool Initialize(rhi::DeviceDep* p_device)
			{
				struct Vec3
				{
					float x, y, z;
				};

				const Vec3 vtx_array[] =
				{
					{0.0f, 1.0f, 0.0f},
					{0.866f, -0.5f, 0.0f},
					{-0.866f, -0.5f, 0.0f},
				};

				// テスト用GeomBuffer.
				rhi::BufferDep::Desc vb_desc = {};
				vb_desc.heap_type = rhi::ResourceHeapType::Upload;
				vb_desc.initial_state = rhi::ResourceState::General;
				vb_desc.element_count = 3;
				vb_desc.element_byte_size = sizeof(vtx_array[0]);
				if (!test_geom_vb_.Initialize(p_device, vb_desc))
				{
					assert(false);
					return false;
				}
				if (auto* mapped = test_geom_vb_.Map())
				{
					memcpy(mapped, vtx_array, sizeof(vtx_array));
					test_geom_vb_.Unmap();
				}


				if(!test_blas_.SetupAsBLAS(&test_geom_vb_, nullptr))
				{
					assert(false);
					return false;
				}

				return true;
			}

			void UpdateOnRender(rhi::DeviceDep* p_device, rhi::GraphicsCommandListDep* p_command_list)
			{
				if (!test_blas_.IsBuilt())
				{
					if (!test_blas_.Build(p_device, p_command_list))
					{
						assert(false);
					}
				}
			}
		private:
			rhi::BufferDep test_geom_vb_;

			RaytraceStructureElement test_blas_;

		};

	}
}