#include "rt_structure_manager.h"

#include <unordered_map>

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
		bool RaytraceStructureBottom::Setup(rhi::DeviceDep* p_device, rhi::BufferDep* vertex_buffer, rhi::BufferDep* index_buffer)
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


			// CommandListが不要な生成部だけを実行する.

			geom_desc_ = {};
			geom_desc_.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;	// Triangle Geom.
			geom_desc_.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;	// Opaque.
			geom_desc_.Triangles.VertexBuffer.StartAddress = p_vertex_buffer_->GetD3D12Resource()->GetGPUVirtualAddress();
			geom_desc_.Triangles.VertexBuffer.StrideInBytes = p_vertex_buffer_->GetElementByteSize();// 12 byte (vec3).
			geom_desc_.Triangles.VertexCount = p_vertex_buffer_->getElementCount();
			geom_desc_.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;// vec3.

			// ここで設定した情報はそのままBuildで利用される.
			build_setup_info_ = {};
			build_setup_info_.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
			build_setup_info_.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
			build_setup_info_.NumDescs = 1;
			build_setup_info_.pGeometryDescs = &geom_desc_;
			build_setup_info_.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL; // BLAS.

			// Prebuildで必要なサイズ取得.
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO build_info = {};
			p_device->GetD3D12DeviceForDxr()->GetRaytracingAccelerationStructurePrebuildInfo(&build_setup_info_, &build_info);

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

			// 実際にscratch_descとmain_descでASをビルドするのはCommandListにタスクとして発行するため分離する.

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
				// Setupで準備した情報からASをビルドするコマンドを発行.
				// Build後は入力に利用したVertexBufferやIndexBufferは不要となるとのこと.

				// Builld.
				D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build_desc = {};
				build_desc.Inputs = build_setup_info_;
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
		bool RaytraceStructureTop::Setup(rhi::DeviceDep* p_device, RaytraceStructureBottom* p_blas, const std::vector<Mat34>& instance_transform_array)
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


			// BLASはSetup済みである必要がある(Setupでバッファ確保等までは完了している必要がある. BLASのBuildはこの時点では不要.)
			assert(p_blas_ && p_blas_->IsSetuped());
			assert(0 < transform_array_.size());

			// Instance Desc Buffer.
			const uint32_t num_instance_total = (uint32_t)transform_array_.size();
			rhi::BufferDep::Desc instance_buffer_desc = {};
			instance_buffer_desc.heap_type = rhi::ResourceHeapType::Upload;// CPUからアップロードするInstanceDataのため.
			instance_buffer_desc.element_count = num_instance_total;// Instance数を確保.
			instance_buffer_desc.element_byte_size = sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
			if (!instance_buffer_.Initialize(p_device, instance_buffer_desc))
			{
				std::cout << "[ERROR] Initialize Rt Instance Buffer." << std::endl;
				assert(false);
				return false;
			}
			// Instance情報をBufferに書き込み.
			if (D3D12_RAYTRACING_INSTANCE_DESC* mapped = (D3D12_RAYTRACING_INSTANCE_DESC*)instance_buffer_.Map())
			{
				for (auto inst_i = 0; inst_i < transform_array_.size(); ++inst_i)
				{
					// 一応ID入れておく
					mapped[inst_i].InstanceID = inst_i;
					// このInstanceのHitGroupを示すベースインデックス. Instanceのマテリアル情報に近い.
					// TraceRay()のRayContributionToHitGroupIndexがこの値に加算されて実際のHitGroup参照になる.
					mapped[inst_i].InstanceContributionToHitGroupIndex = inst_i % 2; // テストで2つあるHitGroupへ振り分け.
					mapped[inst_i].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
					mapped[inst_i].InstanceMask = ~0u;// 0xff;
					// InstanceのBLASを設定.
					mapped[inst_i].AccelerationStructure = p_blas_->GetBuffer()->GetD3D12Resource()->GetGPUVirtualAddress();
					// InstanceのTransform.
					memcpy(mapped[inst_i].Transform, &transform_array_[inst_i], sizeof(mapped[inst_i].Transform));
				}
				instance_buffer_.Unmap();
			}



			// ここで設定した情報はそのままBuildで利用される.
			build_setup_info_ = {};
			build_setup_info_.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
			build_setup_info_.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
			build_setup_info_.NumDescs = num_instance_total;// Instance Desc Bufferの要素数を指定.
			build_setup_info_.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL; // TLAS.
			// input情報にInstanceBufferセット.
			build_setup_info_.InstanceDescs = instance_buffer_.GetD3D12Resource()->GetGPUVirtualAddress();

			// Prebuildで必要なサイズ取得.
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO build_info = {};
			p_device->GetD3D12DeviceForDxr()->GetRaytracingAccelerationStructurePrebuildInfo(&build_setup_info_, &build_info);

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
			main_desc.bind_flag = rhi::ResourceBindFlag::UnorderedAccess | rhi::ResourceBindFlag::ShaderResource; // シェーダからはSRVとして見えるためShaderResourceフラグも設定.
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
			// Main Srv.
			if (!main_srv_.InitializeAsRaytracingAccelerationStructure(p_device, &main_))
			{
				std::cout << "[ERROR] Initialize Rt TLAS View." << std::endl;
				assert(false);
				return false;
			}

			// 実際にscratch_descとmain_descでASをビルドするのはCommandListにタスクとして発行するため分離する.

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
				// Setupで準備した情報からASをビルドするコマンドを発行.
				// 
				// Builld.
				D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build_desc = {};
				build_desc.Inputs = build_setup_info_;
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
		rhi::ShaderResourceViewDep* RaytraceStructureTop::GetSrv()
		{
			return &main_srv_;
		}
		const rhi::ShaderResourceViewDep* RaytraceStructureTop::GetSrv() const
		{
			return &main_srv_;
		}
		// -------------------------------------------------------------------------------



		bool RaytraceStateObject::Initialize(rhi::DeviceDep* p_device, 
			const std::vector<RaytraceShaderRegisterInfo>& shader_info_array, 
			uint32_t payload_byte_size, uint32_t attribute_byte_size, uint32_t max_trace_recursion)
		{
			if (initialized_)
				return false;

			if (0 >= payload_byte_size || 0 >= attribute_byte_size)
			{
				assert(false);
				return false;
			}
			initialized_ = true;

			// 設定を保存.
			payload_byte_size_ = payload_byte_size;
			attribute_byte_size_ = attribute_byte_size;
			max_trace_recursion_ = max_trace_recursion;


			std::unordered_map<const rhi::ShaderDep*, int> shader_map;
			// これらの名前はStateObject内で重複禁止のためMapでチェック.
			std::unordered_map<std::string, int> raygen_map;
			std::unordered_map<std::string, int> miss_map;
			std::unordered_map<std::string, int> hitgroup_map;
			for (int i = 0; i < shader_info_array.size(); ++i)
			{
				const auto& info = shader_info_array[i];

				// ターゲットのシェーダ参照は必須.
				if (nullptr == info.p_shader_library)
				{
					assert(false);
					continue;
				}

				// 管理用シェーダ参照登録.
				if (shader_map.end() == shader_map.find(info.p_shader_library))
				{
					shader_map.insert(std::make_pair(info.p_shader_library, (int)shader_database_.size()));
					// 内部シェーダ登録.
					shader_database_.push_back(info.p_shader_library);
				}
				// 内部シェーダインデックス.
				int shader_index = shader_map.find(info.p_shader_library)->second;

				// RayGeneration.
				for (auto& register_elem : info.ray_generation_shader_array)
				{
					if (raygen_map.end() == raygen_map.find(register_elem))
					{
						raygen_map.insert(std::make_pair(register_elem, shader_index));

						RayGenerationInfo new_elem = {};
						new_elem.shader_index = shader_index;
						new_elem.ray_generation_name = register_elem;
						raygen_database_.push_back(new_elem);
					}
					else
					{
						// 同名はエラー.
						assert(false);
					}
				}
				// Miss.
				for (auto& register_elem : info.miss_shader_array)
				{
					if (miss_map.end() == miss_map.find(register_elem))
					{
						miss_map.insert(std::make_pair(register_elem, shader_index));

						MissInfo new_elem = {};
						new_elem.shader_index = shader_index;
						new_elem.miss_name = register_elem;
						miss_database_.push_back(new_elem);
					}
					else
					{
						// 同名登録はエラー.
						assert(false);
					}
				}
				// HitGroup.
				for (auto& register_elem : info.hitgroup_array)
				{
					if (hitgroup_map.end() == hitgroup_map.find(register_elem.hitgorup_name))
					{
						hitgroup_map.insert(std::make_pair(register_elem.hitgorup_name, shader_index));

						HitgroupInfo new_elem = {};
						new_elem.shader_index = shader_index;
						new_elem.hitgorup_name = register_elem.hitgorup_name;
						
						new_elem.any_hit_name = register_elem.any_hit_name;
						new_elem.closest_hit_name = register_elem.closest_hit_name;
						new_elem.intersection_name = register_elem.intersection_name;

						hitgroup_database_.push_back(new_elem);
					}
					else
					{
						// 同名登録はエラー.
						assert(false);
					}
				}
			}


			// Subobject ShaderLib Exportセットアップ.
			std::vector<subobject::SubobjectDxilLibrary> subobject_shaderlib_array;
			subobject_shaderlib_array.resize(shader_database_.size());
			{
				std::vector<std::vector<const char*>> shader_function_export_info = {};
				shader_function_export_info.resize(shader_database_.size());

				auto func_push_func_name_cp = [](auto& name_vec, const std::string& name)
				{
					if (0 < name.length())
						name_vec.push_back(name.c_str());
				};

				for (const auto& e : raygen_database_)
				{
					func_push_func_name_cp(shader_function_export_info[e.shader_index], e.ray_generation_name);
				}
				for (const auto& e : miss_database_)
				{
					func_push_func_name_cp(shader_function_export_info[e.shader_index], e.miss_name);
				}

				for (const auto& e : hitgroup_database_)
				{
					func_push_func_name_cp(shader_function_export_info[e.shader_index], e.any_hit_name);
					func_push_func_name_cp(shader_function_export_info[e.shader_index], e.closest_hit_name);
					func_push_func_name_cp(shader_function_export_info[e.shader_index], e.intersection_name);
				}

				for (int si = 0; si < shader_database_.size(); ++si)
				{
					subobject_shaderlib_array[si].Setup(shader_database_[si], shader_function_export_info[si].data(), (int)shader_function_export_info[si].size());
				}
			}

			// Subobject Hitgroupセットアップ.
			std::vector<subobject::SubobjectHitGroup> subobject_hitgroup_array;
			subobject_hitgroup_array.resize(hitgroup_database_.size());
			{
				for (int hi = 0; hi < hitgroup_database_.size(); ++hi)
				{
					const auto p_hitgroup		= hitgroup_database_[hi].hitgorup_name.c_str();
					const auto p_anyhit			= hitgroup_database_[hi].any_hit_name.c_str();
					const auto p_closesthit		= hitgroup_database_[hi].closest_hit_name.c_str();
					const auto p_intersection	= hitgroup_database_[hi].intersection_name.c_str();

					subobject_hitgroup_array[hi].Setup(p_anyhit, p_closesthit, p_intersection, p_hitgroup);
				}
			}


			// Global Root Signature. 
			// TODO 現状はかなり固定.
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

				if (!rhi::helper::SerializeAndCreateRootSignature(p_device, root_signature_desc, global_root_signature_))
				{
					assert(false);
					return false;
				}
			}
			// Subobject Global Root Signature.
			subobject::SubobjectGlobalRootSignature so_grs = {};
			so_grs.Setup(global_root_signature_);

			/*
			// Local Root Signatureはとりあえず無し.
			*/

			// Shader Config.
			subobject::SubobjectRaytracingShaderConfig so_shader_config = {};
			so_shader_config.Setup(payload_byte_size, attribute_byte_size);
			// Pipeline Config.
			subobject::SubobjectRaytracingPipelineConfig so_pipeline_config = {};
			so_pipeline_config.Setup(max_trace_recursion);




			// Subobject間の参照が必要になる場合があるため改めて連続メモリにSUBOBJECTを構築. Resolveフェーズを入れるのも予定.
			int num_subobject = 0;
			num_subobject += 1; // Global Root Signature.
			num_subobject += 1; // ShaderConfig.
			num_subobject += 1; // PipelineConfig.
			num_subobject += (int)subobject_shaderlib_array.size();
			num_subobject += (int)subobject_hitgroup_array.size();

			// subobject array.
			std::vector<D3D12_STATE_SUBOBJECT> state_subobject_array;
			state_subobject_array.resize(num_subobject);

			// Subobject間の参照はとりあえず無視して積み込み.
			int cnt_subobject = 0;
			state_subobject_array[cnt_subobject++] = (so_grs.subobject_);
			state_subobject_array[cnt_subobject++] = (so_shader_config.subobject_);
			state_subobject_array[cnt_subobject++] = (so_pipeline_config.subobject_);
			for (const auto& e : subobject_shaderlib_array)
			{
				state_subobject_array[cnt_subobject++] = e.subobject_;
			}
			for (const auto& e : subobject_hitgroup_array)
			{
				state_subobject_array[cnt_subobject++] = e.subobject_;
			}

			D3D12_STATE_OBJECT_DESC state_object_desc = {};
			state_object_desc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
			state_object_desc.NumSubobjects = (uint32_t)state_subobject_array.size();
			state_object_desc.pSubobjects = state_subobject_array.data();

			// 生成.
			if (FAILED(p_device->GetD3D12DeviceForDxr()->CreateStateObject(&state_object_desc, IID_PPV_ARGS(&state_oject_))))
			{
				assert(false);
				return false;
			}

			return true;
		}


		// -------------------------------------------------------------------------------

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

			// BLAS Setup. TRASのSetupに先行する必要がある.
			if (!test_blas_.Setup(p_device, &test_geom_vb_, nullptr))
			{
				assert(false);
				return false;
			}

			// TLAS Setup.
			std::vector<Mat34> test_inst_transforms;
			{
				test_inst_transforms.push_back(Mat34::Identity());

				Mat34 tmp_m = Mat34::Identity();
				tmp_m.m[0][3] = 2.0f;
				test_inst_transforms.push_back(tmp_m);

			}
			if (!test_tlas_.Setup(p_device, &test_blas_, test_inst_transforms))
			{
				assert(false);
				return false;
			}




			// ShaderLibrary.
			ngl::rhi::ShaderDep::InitFileDesc shader_desc = {};
			shader_desc.stage = ngl::rhi::ShaderStage::ShaderLibrary;
			shader_desc.shader_model_version = "6_3";
			shader_desc.shader_file_path = "./src/ngl/resource/shader/dxr_sample_lib.hlsl";
			if (!rt_shader_lib0_.Initialize(p_device, shader_desc))
			{
				std::cout << "[ERROR] Create DXR ShaderLib" << std::endl;
				assert(false);
			}
			// StateObject生成.
			{
				std::vector<RaytraceShaderRegisterInfo> shader_reg_info_array = {};
				{
					// Shader登録エントリ新規.
					auto shader_index = shader_reg_info_array.size();
					shader_reg_info_array.push_back({});

					// 関数登録元ShaderLib参照.
					shader_reg_info_array[shader_index].p_shader_library = &rt_shader_lib0_;

					// シェーダから公開するRaygenerationShader名.
					shader_reg_info_array[shader_index].ray_generation_shader_array.push_back("rayGen");

					// シェーダから公開するMissShader名.
					shader_reg_info_array[shader_index].miss_shader_array.push_back("miss");
					shader_reg_info_array[shader_index].miss_shader_array.push_back("miss2");

					// シェーダから公開するHitGroup関連情報.
					{
						auto hg_index = shader_reg_info_array[shader_index].hitgroup_array.size();
						shader_reg_info_array[shader_index].hitgroup_array.push_back({});

						shader_reg_info_array[shader_index].hitgroup_array[hg_index].hitgorup_name = "hitGroup";
						// このHitGroupはClosestHitのみ.
						shader_reg_info_array[shader_index].hitgroup_array[hg_index].closest_hit_name = "closestHit";
					}
					{
						auto hg_index = shader_reg_info_array[shader_index].hitgroup_array.size();
						shader_reg_info_array[shader_index].hitgroup_array.push_back({});

						shader_reg_info_array[shader_index].hitgroup_array[hg_index].hitgorup_name = "hitGroup2";
						// このHitGroupはClosestHitのみ.
						shader_reg_info_array[shader_index].hitgroup_array[hg_index].closest_hit_name = "closestHit2";
					}
				}

				if (!state_object_.Initialize(p_device, shader_reg_info_array, sizeof(float) * 4, sizeof(float) * 2, 1))
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
				// raygen, miss, hitgroup hitgroup2
				const uint32_t rt_scene_shader_count = 4;

				constexpr uint32_t k_shader_identifier_byte_size = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
				// Table一つにつきベースのGPU Descriptor Handleを書き込むためのサイズ計算.
				const uint32_t shader_record_resource_byte_size = sizeof(D3D12_GPU_DESCRIPTOR_HANDLE) * rt_scene_max_shader_record_discriptor_table_count;

				const uint32_t shader_record_byte_size = rhi::align_to(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, k_shader_identifier_byte_size + shader_record_resource_byte_size);

				const uint32_t shader_table_byte_size = shader_record_byte_size * rt_scene_shader_count;

				// あとで書き込み位置調整に使うので保存.
				rt_shader_table_entry_byte_size_ = shader_record_byte_size;

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
					rhi::DescriptorHeapWrapper::Desc heap_desc = {};
					heap_desc.type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
					heap_desc.allocate_descriptor_count_ = (rt_scene_max_shader_record_discriptor_table_count * rt_scene_shader_count);
					heap_desc.shader_visible = true;
					if (!rt_descriptor_heap_.Initialize(p_device, heap_desc))
					{
						assert(false);
					}
				}


				// レコード書き込み.
				if (auto* mapped = static_cast<uint8_t*>(rt_shader_table_.Map()))
				{
					CComPtr<ID3D12StateObjectProperties> p_rt_so_prop;
					if (FAILED(state_object_.GetStateObject()->QueryInterface(IID_PPV_ARGS(&p_rt_so_prop))))
					{
						assert(false);
					}

					uint32_t table_cnt = 0;

					// raygen
					rt_shader_table_raygen_offset = (shader_record_byte_size * table_cnt);
					{
						memcpy(mapped + rt_shader_table_raygen_offset, p_rt_so_prop->GetShaderIdentifier(L"rayGen"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

						// TODO. Local Root Signature で設定するリソースがある場合はここでGPU Descriptor Handleを書き込む.
					}
					++table_cnt;

					rt_shader_table_miss_offset = (shader_record_byte_size * table_cnt);
					{
						// miss
						{
							memcpy(mapped + (shader_record_byte_size * table_cnt), p_rt_so_prop->GetShaderIdentifier(L"miss"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

							// TODO. Local Root Signature で設定するリソースがある場合はここでGPU Descriptor Handleを書き込む.
						}
						++table_cnt;


						// miss2
						{
							memcpy(mapped + (shader_record_byte_size * table_cnt), p_rt_so_prop->GetShaderIdentifier(L"miss2"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

							// TODO. Local Root Signature で設定するリソースがある場合はここでGPU Descriptor Handleを書き込む.
						}
						++table_cnt;
					}



					// HitGroup
					/// マテリアル分存在するHitGroupは連続領域でInstanceに指定したインデックスでアクセスされるためここ以降に順序に気をつけて書き込み.
					rt_shader_table_hitgroup_offset = shader_record_byte_size * table_cnt;
					{
						// hitGroup
						{
							memcpy(mapped + (shader_record_byte_size * table_cnt), p_rt_so_prop->GetShaderIdentifier(L"hitGroup"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

							// TODO. Local Root Signature で設定するリソースがある場合はここでGPU Descriptor Handleを書き込む.
						}
						++table_cnt;

						// hitGroup2
						{
							memcpy(mapped + (shader_record_byte_size * table_cnt), p_rt_so_prop->GetShaderIdentifier(L"hitGroup2"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

							// TODO. Local Root Signature で設定するリソースがある場合はここでGPU Descriptor Handleを書き込む.
						}
						++table_cnt;
					}
					rt_shader_table_.Unmap();
				}
			}


			// 出力テスト用のTextureとUAV.
			{
				rhi::TextureDep::Desc tex_desc = {};
				tex_desc.type = rhi::TextureType::Texture2D;
				tex_desc.format = rhi::ResourceFormat::Format_R8G8B8A8_UNORM;
				tex_desc.bind_flag = rhi::ResourceBindFlag::UnorderedAccess | rhi::ResourceBindFlag::ShaderResource;
				tex_desc.width = 1920;
				tex_desc.height = 1080;

				if (!ray_result_.Initialize(p_device, tex_desc))
				{
					assert(false);
				}
				// この生成はDeviceの持つシェーダから不可視のPersistentHeap上に作られる. 実際にはDispatch時のHeap上に CopyDescriptors をする.
				if (!ray_result_uav_.Initialize(p_device, &ray_result_, 0, 0, 1))
				{
					assert(false);
				}
				// 同様にPersistent
				if (!ray_result_srv_.InitializeAsTexture(p_device, &ray_result_, 0, 1, 0, 1))
				{
					assert(false);
				}
				// 初期ステート.
				ray_result_state_ = rhi::ResourceState::General;


				// Rt用のHeapにUAVをコピー.
				D3D12_CPU_DESCRIPTOR_HANDLE dst_uav_handle = rt_descriptor_heap_.GetCpuHandleStart();
				uint32_t copy_uav_count = 1;
				p_device->GetD3D12Device()->CopyDescriptors(1, &dst_uav_handle, &copy_uav_count, 1, &ray_result_uav_.GetView().cpu_handle, &copy_uav_count, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );

				// Rt用のHeapにSRVをコピー.
				D3D12_CPU_DESCRIPTOR_HANDLE dst_srv_handle = rt_descriptor_heap_.GetCpuHandleStart();
				dst_srv_handle.ptr += rt_descriptor_heap_.GetHandleIncrementSize() * 1;
				uint32_t copy_srv_count = 1;
				p_device->GetD3D12Device()->CopyDescriptors(1, &dst_srv_handle, &copy_srv_count, 1, &test_tlas_.GetSrv()->GetView().cpu_handle, &copy_srv_count, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
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

				// ビルド後には入力VertexBuffer等のバッファは不要となる.
				// 実際にはラスタライズパイプラインが同時に動く場合はそのままVertexBufferを使うので捨てないことのほうが多いかも.
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

		void RaytraceStructureManager::DispatchRay(rhi::GraphicsCommandListDep* p_command_list)
		{
			auto* d3d_command_list = p_command_list->GetD3D12GraphicsCommandListForDxr();
			auto shader_table_head = rt_shader_table_.GetD3D12Resource()->GetGPUVirtualAddress();


			// to UAV.
			p_command_list->ResourceBarrier(&ray_result_, ray_result_state_, rhi::ResourceState::UnorderedAccess);
			ray_result_state_ = rhi::ResourceState::UnorderedAccess;



			D3D12_DISPATCH_RAYS_DESC raytraceDesc = {};
			raytraceDesc.Width = ray_result_.GetDesc().width;
			raytraceDesc.Height = ray_result_.GetDesc().height;
			raytraceDesc.Depth = 1;

			// RayGen is the first entry in the shader-table
			raytraceDesc.RayGenerationShaderRecord.StartAddress = shader_table_head + rt_shader_table_raygen_offset;
			raytraceDesc.RayGenerationShaderRecord.SizeInBytes = rt_shader_table_entry_byte_size_;

			// Miss is the second entry in the shader-table
			raytraceDesc.MissShaderTable.StartAddress = shader_table_head + rt_shader_table_miss_offset;
			raytraceDesc.MissShaderTable.StrideInBytes = rt_shader_table_entry_byte_size_;
			raytraceDesc.MissShaderTable.SizeInBytes = rt_shader_table_entry_byte_size_;   // Only a s single miss-entry

			// Hit is the third entry in the shader-table
			// マテリアル毎のHitGroupはここから連続領域に格納. Instanceに設定されたHitGroupIndexでアクセスされる.
			raytraceDesc.HitGroupTable.StartAddress = shader_table_head + rt_shader_table_hitgroup_offset;
			raytraceDesc.HitGroupTable.StrideInBytes = rt_shader_table_entry_byte_size_;
			raytraceDesc.HitGroupTable.SizeInBytes = rt_shader_table_entry_byte_size_;

			// Bind the empty root signature
			d3d_command_list->SetComputeRootSignature(state_object_.GetGlobalRootSignature());

			// heap
			auto* res_heap = rt_descriptor_heap_.GetD3D12();
			d3d_command_list->SetDescriptorHeaps(1, &res_heap);

			{
				// Descriptor Table.
				// Global Root Signatureではテーブル0でUAVとAS-SRVをバインドしているのでその通りに.

				D3D12_GPU_DESCRIPTOR_HANDLE heap_head = rt_descriptor_heap_.GetGpuHandleStart();
				auto handle_size = rt_descriptor_heap_.GetHandleIncrementSize();

				D3D12_GPU_DESCRIPTOR_HANDLE table0 = heap_head;
				d3d_command_list->SetComputeRootDescriptorTable(0, table0);
			}


			// Dispatch
			d3d_command_list->SetPipelineState1(state_object_.GetStateObject());

			d3d_command_list->DispatchRays(&raytraceDesc);



			// to SRV.
			p_command_list->ResourceBarrier(&ray_result_, ray_result_state_, rhi::ResourceState::ShaderRead);
			ray_result_state_ = rhi::ResourceState::ShaderRead;
		}


	}
}