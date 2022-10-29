#include "rt_structure_manager.h"

namespace ngl
{
	namespace gfx
	{

		RaytraceStructureBottom::RaytraceStructureBottom()
		{
		}
		RaytraceStructureBottom::~RaytraceStructureBottom()
		{
		}
		bool RaytraceStructureBottom::Setup(rhi::BufferDep* vertex_buffer, rhi::BufferDep* index_buffer)
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
		bool RaytraceStructureBottom::Build(rhi::DeviceDep* p_device, rhi::GraphicsCommandListDep* p_command_list)
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
		bool RaytraceStructureBottom::IsSetuped() const
		{
			return SETUP_TYPE::NONE != setup_type_;
		}
		bool RaytraceStructureBottom::IsBuilt() const
		{
			return is_built_;
		}

		rhi::BufferDep* RaytraceStructureBottom::GetBuffer()
		{
			return &main_;
		}
		const rhi::BufferDep* RaytraceStructureBottom::GetBuffer() const
		{
			return &main_;
		}





		RaytraceStructureTop::RaytraceStructureTop()
		{
		}
		RaytraceStructureTop::~RaytraceStructureTop()
		{
		}
		// TLAS setup.
		// index_buffer : optional.
		// bufferの管理責任は外部.
		bool RaytraceStructureTop::Setup(RaytraceStructureBottom* p_blas, const std::vector<Mat34>& instance_transform_array)
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
		bool RaytraceStructureTop::Build(rhi::DeviceDep* p_device, rhi::GraphicsCommandListDep* p_command_list)
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
				main_desc.bind_flag = rhi::ResourceBindFlag::UnorderedAccess | rhi::ResourceBindFlag::ShaderResource; // Srvフラグ必要かも
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

				// Main Srv.
				if (!main_srv_.InitializeAsRaytracingAccelerationStructure(p_device, &main_))
				{
					std::cout << "[ERROR] Initialize Rt TLAS View." << std::endl;
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
		bool RaytraceStructureTop::IsSetuped() const
		{
			return SETUP_TYPE::NONE != setup_type_;
		}
		bool RaytraceStructureTop::IsBuilt() const
		{
			return is_built_;
		}
		rhi::BufferDep* RaytraceStructureTop::GetBuffer()
		{
			return &main_;
		}
		const rhi::BufferDep* RaytraceStructureTop::GetBuffer() const
		{
			return &main_;
		}





		RaytraceStructureManager::RaytraceStructureManager()
		{
		}
		RaytraceStructureManager::~RaytraceStructureManager()
		{
		}
		bool RaytraceStructureManager::Initialize(rhi::DeviceDep* p_device)
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
			if (!test_blas_.Setup(&test_geom_vb_, nullptr))
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

		void RaytraceStructureManager::UpdateOnRender(rhi::DeviceDep* p_device, rhi::GraphicsCommandListDep* p_command_list)
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



	}
}