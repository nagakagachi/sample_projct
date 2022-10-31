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


			// RtPso等.
			{
				// Global Root Signature.
				{
					std::vector<D3D12_DESCRIPTOR_RANGE> range_array;
					range_array.resize(2);
					range_array[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
					range_array[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
					range_array[0].NumDescriptors = 1;
					range_array[0].BaseShaderRegister = 0;
					range_array[0].RegisterSpace = 0;

					range_array[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
					range_array[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
					range_array[1].NumDescriptors = 1;
					range_array[1].BaseShaderRegister = 0;
					range_array[1].RegisterSpace = 0;


					std::vector<D3D12_ROOT_PARAMETER> root_param;
					{
						root_param.push_back({});
						auto& parame_elem = root_param.back();
						parame_elem.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
						parame_elem.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
						parame_elem.DescriptorTable.NumDescriptorRanges = 2;
						parame_elem.DescriptorTable.pDescriptorRanges = &range_array[0];
					}

					// GlobalでDescriptorTable一つでUAVとSRVを設定するSignature.
					D3D12_ROOT_SIGNATURE_DESC root_signature_desc = {};
					root_signature_desc.NumParameters = (uint32_t)root_param.size();
					root_signature_desc.pParameters = root_param.data();
					root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

					if (!rhi::helper::SerializeAndCreateRootSignature(p_device, root_signature_desc, rt_global_root_signature_))
					{
						assert(false);
						return false;
					}
				}
				// Subobject Global Root Signature.
				subobject::SubobjectGlobalRootSignature so_grs = {};
				so_grs.Setup(rt_global_root_signature_);


				// Shader Config.
				subobject::SubobjectRaytracingShaderConfig so_shader_config = {};
				so_shader_config.Setup(sizeof(uint32_t));

				// Pipeline Config.
				subobject::SubobjectRaytracingPipelineConfig so_pipeline_config = {};
				so_pipeline_config.Setup(1);


				// ShaderLibrary.
				ngl::rhi::ShaderDep::Desc shader_desc = {};
				shader_desc.stage = ngl::rhi::ShaderStage::ShaderLibrary;
				shader_desc.shader_model_version = "6_3";
				shader_desc.shader_file_path = "./src/ngl/resource/shader/dxr_sample_lib.hlsl";
				if (!rt_shader_lib0_.Initialize(p_device, shader_desc))
				{
					std::cout << "[ERROR] Create DXR ShaderLib" << std::endl;
					assert(false);
				}

				// DxilLibrary.
				subobject::SubobjectDxilLibrary so_shader_lib = {};
				const char* rt_entries[] =
				{
					"rayGen",
					"miss",
					"closestHit",
				};
				so_shader_lib.Setup(&rt_shader_lib0_, rt_entries, (int)std::size(rt_entries));

				// HitGroup
				subobject::SubobjectHitGroup so_hitgroup = {};
				so_hitgroup.Setup(nullptr, "closestHit", nullptr, "hitGroup");


				// Local Root Signature.
				{
					// 空.
					D3D12_ROOT_SIGNATURE_DESC root_signature_desc = {};
					root_signature_desc.NumParameters = 0;
					root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

					if (!rhi::helper::SerializeAndCreateRootSignature(p_device, root_signature_desc, rt_local_root_signature0_))
					{
						assert(false);
						return false;
					}
				}
				subobject::SubobjectLocalRootSignature so_lrs0 = {};
				so_lrs0.Setup(rt_local_root_signature0_);


				// subobject array.
				std::vector<D3D12_STATE_SUBOBJECT> state_subobject_array;
				state_subobject_array.resize(10);
				int cnt_subobject = 0;

				state_subobject_array[cnt_subobject++] = (so_shader_lib.subobject_);
				state_subobject_array[cnt_subobject++] = (so_hitgroup.subobject_);
				state_subobject_array[cnt_subobject++] = (so_shader_config.subobject_);

				state_subobject_array[cnt_subobject++] = (so_pipeline_config.subobject_);
				state_subobject_array[cnt_subobject++] = (so_grs.subobject_);
				state_subobject_array[cnt_subobject++] = (so_lrs0.subobject_);

				D3D12_STATE_OBJECT_DESC state_object_desc = {};
				state_object_desc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
				state_object_desc.NumSubobjects = (uint32_t)cnt_subobject;
				state_object_desc.pSubobjects = state_subobject_array.data();

				// 生成.
				if (FAILED(p_device->GetD3D12DeviceForDxr()->CreateStateObject(&state_object_desc, IID_PPV_ARGS(&rt_state_oject_))))
				{
					assert(false);
					return false;
				}
			}

			// Shader Table.
			{
				// https://github.com/Monsho/D3D12Samples/blob/95d1c3703cdcab816bab0b5dcf1a1e42377ab803/Sample013/src/main.cpp
				// https://github.com/microsoft/DirectX-Specs/blob/master/d3d/Raytracing.md#shader-tables

				// 現状の最大はRayGenのTable一つなので * 1.
				// Table一つで TLAS(srv)とOutput(uav)を設定.
				const uint32_t rt_scene_max_shader_record_discriptor_table_count = 1;
				// raygen, miss, hitgroup の3つ
				const uint32_t rt_scene_shader_count = 3;

				constexpr uint32_t k_shader_identifier_byte_size = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
				// Table一つにつきベースのGPU Descriptor Handleを書き込むためのサイズ計算.
				const uint32_t shader_record_resource_byte_size = sizeof(D3D12_GPU_DESCRIPTOR_HANDLE) * rt_scene_max_shader_record_discriptor_table_count;

				const uint32_t shader_record_byte_size = rhi::align_to(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, k_shader_identifier_byte_size + shader_record_resource_byte_size);

				const uint32_t shader_table_byte_size = shader_record_byte_size * rt_scene_shader_count;

				// バッファ確保.
				rhi::BufferDep::Desc rt_shader_table_desc = {};
				rt_shader_table_desc.element_count = 1;
				rt_shader_table_desc.element_byte_size = shader_table_byte_size;
				rt_shader_table_desc.heap_type = rhi::ResourceHeapType::Upload;// CPUから直接書き込むため.
				if (!rt_shader_table_.Initialize(p_device, rt_shader_table_desc))
				{
					assert(false);
					return false;
				}


				// リソース用DescriptorHeapを生成.
				{
					D3D12_DESCRIPTOR_HEAP_DESC resource_descriptor_desc = {};
					resource_descriptor_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
					resource_descriptor_desc.NumDescriptors = (rt_scene_max_shader_record_discriptor_table_count * rt_scene_shader_count);
					// Shaderから可視のHeap. CPUから直接書き込めないのでrhi::PersistentDescriptorAllocator等のDescriptorをコピーすること前提.
					resource_descriptor_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
					if (FAILED(p_device->GetD3D12Device()->CreateDescriptorHeap(&resource_descriptor_desc, IID_PPV_ARGS(&rt_resource_descriptor_heap_))))
					{
						assert(false);
						return false;
					}
				}


				// レコード書き込み.
				if (auto* mapped = static_cast<uint8_t*>(rt_shader_table_.Map()))
				{
					CComPtr<ID3D12StateObjectProperties> p_rt_so_prop;
					if (FAILED(rt_state_oject_->QueryInterface(IID_PPV_ARGS(&p_rt_so_prop))))
					{
						assert(false);
					}

					// raygen
					{
						memcpy(mapped + (shader_record_byte_size * 0), p_rt_so_prop->GetShaderIdentifier(L"rayGen"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

						// TODO. Local Root Signature で設定するリソースがある場合はここでGPU Descriptor Handleを書き込む.
					}

					// miss
					{
						memcpy(mapped + (shader_record_byte_size * 1), p_rt_so_prop->GetShaderIdentifier(L"miss"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

						// TODO. Local Root Signature で設定するリソースがある場合はここでGPU Descriptor Handleを書き込む.
					}

					// hitGroup
					{
						memcpy(mapped + (shader_record_byte_size * 2), p_rt_so_prop->GetShaderIdentifier(L"hitGroup"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

						// TODO. Local Root Signature で設定するリソースがある場合はここでGPU Descriptor Handleを書き込む.
					}


					rt_shader_table_.Unmap();
				}
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