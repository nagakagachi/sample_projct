#pragma once

#include <cstring>

#include "ngl/rhi/d3d12/rhi.d3d12.h"
#include "ngl/rhi/d3d12/rhi_command_list.d3d12.h"
#include "ngl/rhi/d3d12/rhi_resource.d3d12.h"

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

			bool IsSetuped() const
			{
				return SETUP_TYPE::NONE != setup_type_;
			}
			bool IsBuilt() const
			{
				return is_built_;
			}

		public:
			// BLAS setup.
			// index_buffer : optional.
			// bufferの管理責任は外部.
			bool Setup(rhi::BufferDep* vertex_buffer, rhi::BufferDep* index_buffer = nullptr)
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
			// TODO RenderDocでのLaunchはクラッシュするのでNsight推奨.
			bool Build(rhi::DeviceDep* p_device, rhi::GraphicsCommandListDep* p_command_list)
			{
				assert(p_device);
				assert(p_command_list);

				if (is_built_)
					return false;

				if (!IsSetuped())
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

			rhi::BufferDep* GetBuffer()
			{
				return &main_;
			}
			const rhi::BufferDep* GetBuffer() const
			{
				return &main_;
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

		// TLAS.
		class RaytraceStructureTop
		{
		public:
			enum SETUP_TYPE : int
			{
				NONE,
				TLAS,
			};

			bool IsSetuped() const
			{
				return SETUP_TYPE::NONE != setup_type_;
			}
			bool IsBuilt() const
			{
				return is_built_;
			}

		public:
			// BLAS setup.
			// index_buffer : optional.
			// bufferの管理責任は外部.
			bool Setup(RaytraceStructureBottom* p_blas, const std::vector<Mat34>& instance_transform_array)
			{
				if (is_built_)
					return false;

				if (!p_blas || !p_blas->IsSetuped())
				{
					assert(false);
					return false;
				}
				if (0 >= instance_transform_array.size())
				{
					assert(false);
					return false;
				}

				setup_type_ = SETUP_TYPE::TLAS;
				p_blas_ = p_blas;
				transform_array_ = instance_transform_array;// copy.

				return true;
			}

			// SetupAs... の情報を元に構造構築コマンドを発行する.
			// Buildタイミングをコントロールするために分離している.
			// TODO RenderDocでのLaunchはクラッシュするのでNsight推奨.
			bool Build(rhi::DeviceDep* p_device, rhi::GraphicsCommandListDep* p_command_list)
			{
				assert(p_device);
				assert(p_command_list);

				if (is_built_)
					return false;

				if (!IsSetuped())
				{
					// セットアップされていない.
					assert(false);
					return false;
				}

				// TLAS Build .
				if (SETUP_TYPE::TLAS == setup_type_)
				{
					assert(p_blas_);
					assert(0 < transform_array_.size());

					D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS build_inputs = {};
					build_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
					build_inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
					build_inputs.NumDescs = 1;
					build_inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL; // TLAS.

					// Prebuildで必要なサイズ取得.
					D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO build_info = {};
					p_device->GetD3D12DeviceForDxr()->GetRaytracingAccelerationStructurePrebuildInfo(&build_inputs, &build_info);


					// PreBuild情報からバッファ生成
					tlas_byte_size_ = (int)build_info.ResultDataMaxSizeInBytes;
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
					// Instance Desc Buffer.
					rhi::BufferDep::Desc instance_buffer_desc = {};
					instance_buffer_desc.heap_type = rhi::ResourceHeapType::Upload;// CPUからアップロードするInstanceDataのため.
					instance_buffer_desc.element_count = 1;
					instance_buffer_desc.element_byte_size = sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
					if (!instance_buffer_.Initialize(p_device, instance_buffer_desc))
					{
						std::cout << "[ERROR] Initialize Rt Instance Buffer." << std::endl;
						assert(false);
						return false;
					}

					// Instance情報を書き込み.
					if (D3D12_RAYTRACING_INSTANCE_DESC* mapped = (D3D12_RAYTRACING_INSTANCE_DESC*)instance_buffer_.Map())
					{

						mapped->InstanceID = 0;
						mapped->InstanceContributionToHitGroupIndex = 0;
						mapped->Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
						mapped->InstanceMask = ~0u;// 0xff;
						// BLAS.
						mapped->AccelerationStructure = p_blas_->GetBuffer()->GetD3D12Resource()->GetGPUVirtualAddress();
						// Transform.
						memcpy(mapped->Transform, &transform_array_[0], sizeof(mapped->Transform));

						instance_buffer_.Unmap();
					}

					// input情報にInstanceBufferセット.
					build_inputs.InstanceDescs = instance_buffer_.GetD3D12Resource()->GetGPUVirtualAddress();

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
			rhi::BufferDep* GetBuffer()
			{
				return &main_;
			}
			const rhi::BufferDep* GetBuffer() const
			{
				return &main_;
			}

		private:
			bool is_built_ = false;

			// setup data.
			SETUP_TYPE		setup_type_ = SETUP_TYPE::NONE;
			RaytraceStructureBottom* p_blas_ = nullptr;
			std::vector<Mat34> transform_array_;


			// built data.
			rhi::BufferDep scratch_;
			rhi::BufferDep main_;
			rhi::BufferDep instance_buffer_;
			int tlas_byte_size_ = 0;
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

				// BLAS.
				if(!test_blas_.Setup(&test_geom_vb_, nullptr))
				{
					assert(false);
					return false;
				}

				// TLAS. SetupにはBuild前のBLASを設定可能ということにする.
				std::vector<Mat34> test_inst_transforms;
				test_inst_transforms.push_back(Mat34::Identity());
				if (!test_tlas_.Setup(&test_blas_, test_inst_transforms))
				{
					assert(false);
					return false;
				}

				return true;
			}

			void UpdateOnRender(rhi::DeviceDep* p_device, rhi::GraphicsCommandListDep* p_command_list)
			{
				// BLAS.
				if (!test_blas_.IsBuilt())
				{
					if (!test_blas_.Build(p_device, p_command_list))
					{
						assert(false);
					}
				}

				// TLAS.
				if (!test_tlas_.IsBuilt())
				{
					if (!test_tlas_.Build(p_device, p_command_list))
					{
						assert(false);
					}
				}
			}
		private:
			rhi::BufferDep test_geom_vb_;

			RaytraceStructureBottom test_blas_;

			RaytraceStructureTop test_tlas_;

		};

	}
}