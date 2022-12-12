#include "rt_structure_manager.h"

#include <unordered_map>
#include <algorithm>

namespace ngl
{
	namespace gfx
	{

		RaytraceStructureBottom::RaytraceStructureBottom()
		{
		}
		RaytraceStructureBottom::~RaytraceStructureBottom()
		{
			for (auto& e : geometry_vertex_srv_array_)
			{
				if (e)
				{
					delete e;
				}
			}
			geometry_vertex_srv_array_.clear();
			for (auto& e : geometry_index_srv_array_)
			{
				if (e)
				{
					delete e;
				}
			}
			geometry_index_srv_array_.clear();
		}
		bool RaytraceStructureBottom::Setup(rhi::DeviceDep* p_device, const std::vector<RaytraceStructureBottomGeometryDesc>& geometry_desc_array)
		{
			// CommandListが不要な生成部だけを実行する.

			if (is_built_)
				return false;

			if (0 >= geometry_desc_array.size())
			{
				assert(false);
				return false;
			}

			// コピー.
			geometry_desc_array_ = geometry_desc_array;

			setup_type_ = SETUP_TYPE::BLAS_TRIANGLE;
			geom_desc_array_.clear();
			geom_desc_array_.reserve(geometry_desc_array.size());// 予約.
			for (auto& g : geometry_desc_array)
			{
				if (nullptr != g.vertex_buffer)
				{
					geom_desc_array_.push_back({});
					auto& geom_desc = geom_desc_array_[geom_desc_array_.size() - 1];// Tail.


					const auto vertex_ngl_format = ngl::rhi::ResourceFormat::Format_R32G32B32_FLOAT;
					auto index_ngl_format = ngl::rhi::ResourceFormat::Format_R32_UINT;

					geom_desc = {};
					geom_desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;	// Triangle Geom.
					geom_desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;	// Opaque.
					geom_desc.Triangles.VertexBuffer.StartAddress = g.vertex_buffer->GetD3D12Resource()->GetGPUVirtualAddress();
					geom_desc.Triangles.VertexBuffer.StrideInBytes = g.vertex_buffer->GetElementByteSize();
					geom_desc.Triangles.VertexCount = g.vertex_buffer->getElementCount();
					geom_desc.Triangles.VertexFormat = rhi::ConvertResourceFormat(vertex_ngl_format);// DXGI_FORMAT_R32G32B32_FLOAT;// vec3 データとしてはVec4のArrayでStrideでスキップしている可能性がある.
					if (g.index_buffer)
					{
						geom_desc.Triangles.IndexBuffer = g.index_buffer->GetD3D12Resource()->GetGPUVirtualAddress();
						geom_desc.Triangles.IndexCount = g.index_buffer->getElementCount();
						if (g.index_buffer->GetElementByteSize() == 4)
						{
							geom_desc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
							index_ngl_format = ngl::rhi::ResourceFormat::Format_R32_UINT;
						}
						else if (g.index_buffer->GetElementByteSize() == 2)
						{
							geom_desc.Triangles.IndexFormat = DXGI_FORMAT_R16_UINT;
							index_ngl_format = ngl::rhi::ResourceFormat::Format_R16_UINT;
						}
						else
						{
							// u16 u32 以外は未対応.
							assert(false);
							continue;
						}
					}


					// 内部で必要なSrvを作ってしまう.

					{
						auto p_vertex_srv = new ngl::rhi::ShaderResourceViewDep();
						geometry_vertex_srv_array_.push_back(p_vertex_srv);

						auto p_index_srv = new ngl::rhi::ShaderResourceViewDep();
						geometry_index_srv_array_.push_back(p_index_srv);

						if (!p_vertex_srv->InitializeAsTyped(p_device, g.vertex_buffer, vertex_ngl_format, 0, g.vertex_buffer->getElementCount()))
						{
							assert(false);
						}
						if (!p_index_srv->InitializeAsTyped(p_device, g.index_buffer, index_ngl_format, 0, g.index_buffer->getElementCount()))
						{
							assert(false);
						}
					}
				}
				else
				{
					// スキップ.
					assert(false);
					continue;
				}
			}

			// ここで設定した情報はそのままBuildで利用される.
			build_setup_info_ = {};
			build_setup_info_.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
			build_setup_info_.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
			build_setup_info_.NumDescs = static_cast<uint32_t>(geom_desc_array_.size());
			build_setup_info_.pGeometryDescs = geom_desc_array_.data();
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

		// 内部Geometry情報.
		RaytraceStructureBottomGeometryResource RaytraceStructureBottom::GetGeometryData(uint32_t index)
		{
			RaytraceStructureBottomGeometryResource ret = {};
			if (NumGeometry() <= index)
			{
				assert(false);
				return ret;
			}
			ret.vertex_srv = geometry_vertex_srv_array_[index];
			ret.index_srv = geometry_index_srv_array_[index];
			return ret;
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
		bool RaytraceStructureTop::Setup(rhi::DeviceDep* p_device, std::vector<RaytraceStructureBottom*>& blas_array,
			const std::vector<uint32_t>& instance_geom_id_array,
			const std::vector<math::Mat34>& instance_transform_array,
			const std::vector<uint32_t>& instance_hitgroup_id_array
		)
		{
			if (is_built_)
				return false;

			if (0 >= blas_array.size())
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



			std::vector<int> id_remap;
			id_remap.resize(blas_array.size());

			// 有効でSetup済みのBLASのみ収集.
			blas_array_.clear();
			for (int i = 0; i < blas_array.size(); ++i)
			{
				auto& e = blas_array[i];

				if (nullptr != e && e->IsSetuped())
				{
					blas_array_.push_back(e);
					id_remap[i] = static_cast<int>(blas_array_.size() - 1);
				}
				else
				{
					id_remap[i] = -1;
				}
			}

			{
				instance_blas_id_array_.clear();
				transform_array_.clear();
				hitgroup_id_array_.clear();

				// 参照BLASが有効なInstanceのみ収集.
				for (int i = 0; i < instance_geom_id_array.size(); ++i)
				{
					if (id_remap.size() <= instance_geom_id_array[i])
						continue;
					const int blas_id =  id_remap[instance_geom_id_array[i]];
					if (0 > blas_id)
						continue;

					
					instance_blas_id_array_.push_back(blas_id);

					if (instance_transform_array.size() > i)
						transform_array_.push_back(instance_transform_array[i]);

					if (instance_hitgroup_id_array.size() > i)
						hitgroup_id_array_.push_back(instance_hitgroup_id_array[i]);
				}
			}

			assert(0 < blas_array_.size());
			assert(0 < instance_blas_id_array_.size());

			// BLASはSetup済みである必要がある(Setupでバッファ確保等までは完了している必要がある. BLASのBuildはこの時点では不要.)
			//assert(p_blas_ && p_blas_->IsSetuped());
			//assert(0 < transform_array_.size());

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
					mapped[inst_i].InstanceContributionToHitGroupIndex = inst_i; // 現状はInstance毎に個別のShaderTableエントリ.
					mapped[inst_i].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
					mapped[inst_i].InstanceMask = ~0u;// 0xff;
					
					// InstanceのBLASを設定.
					mapped[inst_i].AccelerationStructure = blas_array_[instance_blas_id_array_[inst_i]]->GetBuffer()->GetD3D12Resource()->GetGPUVirtualAddress();

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


		uint32_t RaytraceStructureTop::NumBlas() const
		{
			return static_cast<uint32_t>(blas_array_.size());
		}
		const std::vector<RaytraceStructureBottom*>& RaytraceStructureTop::GetBlasArray() const
		{
			return blas_array_;
		}
		uint32_t RaytraceStructureTop::NumInstance() const
		{
			return static_cast<uint32_t>(hitgroup_id_array_.size());
		}
		const std::vector<uint32_t>& RaytraceStructureTop::GetInstanceBlasIndexArray() const
		{
			return instance_blas_id_array_;
		}
		const std::vector<math::Mat34>& RaytraceStructureTop::GetInstanceTransformArray() const
		{
			return transform_array_;
		}
		const std::vector<uint32_t>& RaytraceStructureTop::GetInstanceHitgroupIndexArray() const
		{
			return hitgroup_id_array_;
		}
		// -------------------------------------------------------------------------------


		// Raytrace用のStateObject生成のためのSubobject関連ヘルパー.
		namespace subobject
		{
			// Subobjectが必ず持つ情報の管理する基底.
			struct SubobjectBase
			{
				SubobjectBase(D3D12_STATE_SUBOBJECT_TYPE type, void* p_desc)
				{
					subobject_.Type = type;
					subobject_.pDesc = p_desc;
				}

				D3D12_STATE_SUBOBJECT	subobject_ = {};

			protected:
			};

			// Subobject DXIL Library.
			// 複数のシェーダを含んだオブジェクト.
			struct SubobjectDxilLibrary : public SubobjectBase
			{
				SubobjectDxilLibrary()
					: SubobjectBase(D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &library_desc_)
				{}

				void Setup(const rhi::ShaderDep* p_shader, const char* entry_point_array[], int num_entry_point)
				{
					library_desc_ = {};
					export_name_cache_.resize(num_entry_point);
					export_desc_.resize(num_entry_point);
					if (p_shader)
					{
						p_shader_lib_ = p_shader;

						library_desc_.DXILLibrary.pShaderBytecode = p_shader_lib_->GetShaderBinaryPtr();
						library_desc_.DXILLibrary.BytecodeLength = p_shader_lib_->GetShaderBinarySize();
						library_desc_.NumExports = num_entry_point;
						library_desc_.pExports = export_desc_.data();

						for (int i = 0; i < num_entry_point; ++i)
						{
							wchar_t tmp_ws[64];
							mbstowcs_s(nullptr, tmp_ws, entry_point_array[i], std::size(tmp_ws));
							// 内部にキャッシュ.
							export_name_cache_[i] = tmp_ws;

							export_desc_[i] = {};
							export_desc_[i].Name = export_name_cache_[i].c_str();
							export_desc_[i].Flags = D3D12_EXPORT_FLAG_NONE;
							export_desc_[i].ExportToRename = nullptr;
						}
					}
				}
			private:
				D3D12_DXIL_LIBRARY_DESC			library_desc_ = {};
				const rhi::ShaderDep* p_shader_lib_ = nullptr;
				std::vector<std::wstring>		export_name_cache_;
				std::vector<D3D12_EXPORT_DESC>	export_desc_;
			};

			// HitGroup.
			// マテリアル毎のRaytraceシェーダグループ.
			struct SubobjectHitGroup : public SubobjectBase
			{
				SubobjectHitGroup()
					: SubobjectBase(D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &hit_group_desc_)
				{}

				void Setup(const char* anyhit, const char* closesthit, const char* intersection, const char* hitgroup_name)
				{
					hit_group_desc_ = {};
					hit_group_desc_.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;

					if (anyhit && ('\0' != anyhit[0]))
					{
						anyhit_name_cache_ = str_to_wstr(anyhit);
						hit_group_desc_.AnyHitShaderImport = anyhit_name_cache_.c_str();
					}
					if (closesthit && ('\0' != closesthit[0]))
					{
						closesthit_name_cache_ = str_to_wstr(closesthit);
						hit_group_desc_.ClosestHitShaderImport = closesthit_name_cache_.c_str();
					}
					if (intersection && ('\0' != intersection[0]))
					{
						intersection_name_cache_ = str_to_wstr(intersection);
						hit_group_desc_.IntersectionShaderImport = intersection_name_cache_.c_str();
					}
					if (hitgroup_name && ('\0' != hitgroup_name[0]))
					{
						hitgroup_name_cache_ = str_to_wstr(hitgroup_name);
						hit_group_desc_.HitGroupExport = hitgroup_name_cache_.c_str();
					}
				}
			private:
				D3D12_HIT_GROUP_DESC hit_group_desc_ = {};
				std::wstring anyhit_name_cache_ = {};
				std::wstring closesthit_name_cache_ = {};
				std::wstring intersection_name_cache_ = {};
				std::wstring hitgroup_name_cache_ = {};
			};


			// Shader Config.
			// RtPSOの全体に対する設定と思われる.
			struct SubobjectRaytracingShaderConfig : public SubobjectBase
			{
				// デフォルト値として BuiltInTriangleIntersectionAttributes のサイズ (float2 barycentrics).
				static constexpr uint32_t k_default_attribute_size = 2 * sizeof(float);

				SubobjectRaytracingShaderConfig()
					: SubobjectBase(D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &shader_config_)
				{}

				// RaytracingのPayloadとAttributeのサイズ.
				void Setup(uint32_t raytracing_payload_size, uint32_t raytracing_attribute_size = k_default_attribute_size)
				{
					shader_config_.MaxAttributeSizeInBytes = raytracing_attribute_size;
					shader_config_.MaxPayloadSizeInBytes = raytracing_payload_size;
				}
			private:
				D3D12_RAYTRACING_SHADER_CONFIG shader_config_ = {};
			};

			// Pipeline Config.
			// RtPSOの全体に対する設定と思われる.
			struct SubobjectRaytracingPipelineConfig : public SubobjectBase
			{
				SubobjectRaytracingPipelineConfig()
					: SubobjectBase(D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &pipeline_config_)
				{}

				// Rayの最大再帰回数.
				void Setup(uint32_t max_trace_recursion_depth)
				{
					pipeline_config_.MaxTraceRecursionDepth = max_trace_recursion_depth;
				}
			private:
				D3D12_RAYTRACING_PIPELINE_CONFIG pipeline_config_ = {};
			};

			// Global Root Signature.
			struct SubobjectGlobalRootSignature : public SubobjectBase
			{
				SubobjectGlobalRootSignature()
					: SubobjectBase(D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, nullptr)
				{}
				void Setup(CComPtr<ID3D12RootSignature> p_root_signature)
				{
					p_root_signature_ = p_root_signature;
					// NOTE. RootSignatureのポインタではなく, RootSignatureのポインタ変数のアドレス であることに注意 (これで2日溶かした).
					this->subobject_.pDesc = &p_root_signature_.p;
				}
			private:
				CComPtr<ID3D12RootSignature> p_root_signature_;
			};

			// Local Root Signature.
			struct SubobjectLocalRootSignature : public SubobjectBase
			{
				SubobjectLocalRootSignature()
					: SubobjectBase(D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE, nullptr)
				{}

				void Setup(CComPtr<ID3D12RootSignature> p_root_signature)
				{
					p_root_signature_ = p_root_signature;
					// NOTE. RootSignatureのポインタではなく, RootSignatureのポインタ変数のアドレス であることに注意 (これで2日溶かした).
					this->subobject_.pDesc = &p_root_signature_.p;
				}
			private:
				CComPtr<ID3D12RootSignature> p_root_signature_;
			};

			// Association
			// 基本的には SubobjectLocalRootSignature と一対一で Local Root Signatureとシェーダレコード(shadow hitGroup等)をバインドするためのもの.
			// NVIDIAサンプルではShaderNameとShaderConfig(Payloadサイズ等)のバインドもしているように見える.
			struct SubobjectExportsAssociation : public SubobjectBase
			{
				SubobjectExportsAssociation()
					: SubobjectBase(D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION, &exports_)
				{}

				void Setup(const D3D12_STATE_SUBOBJECT* p_subobject, const char* export_name_array[], int num)
				{
					exports_ = {};

					export_name_cache_.resize(num);
					export_name_array_.resize(num);
					for (int i = 0; i < num; ++i)
					{
						wchar_t tmp_ws[64];
						mbstowcs_s(nullptr, tmp_ws, export_name_array[i], std::size(tmp_ws));
						// 内部にキャッシュ.
						export_name_cache_[i] = tmp_ws;
						// 名前配列.
						export_name_array_[i] = export_name_cache_[i].c_str();
					}

					exports_.pSubobjectToAssociate = p_subobject;
					exports_.pExports = export_name_array_.data();
					exports_.NumExports = num;

				}
			private:
				D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION	exports_ = {};
				std::vector<std::wstring>				export_name_cache_;
				std::vector<const wchar_t*>				export_name_array_;
			};
		}

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
				// ASは重複しないであろうレジスタにASを設定.
				// t[k_system_raytracing_structure_srv_register] -> AccelerationStructure.

				std::vector<D3D12_ROOT_PARAMETER> root_param;
				{
					// GlobalRootSignature 0 は固定でAccelerationStructure用SRV.
					root_param.push_back({});
					auto& parame_elem = root_param.back();
					parame_elem.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
					parame_elem.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;  // SRV Descriptor直接.
					parame_elem.Descriptor.ShaderRegister = k_system_raytracing_structure_srv_register; // システムからASを設定.
					parame_elem.Descriptor.RegisterSpace = 0; // space 0
				}


				// CBV		-> GlobalRoot Table1. b0.
				// SRV		-> GlobalRoot Table2. t0.
				// Sampler	-> GlobalRoot Table3. s0.
				// UAV		-> GlobalRoot Table4. u0.

				std::vector<D3D12_DESCRIPTOR_RANGE> range_array;
				range_array.resize(4);
				range_array[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
				range_array[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
				range_array[0].NumDescriptors = k_rt_global_descriptor_cbvsrvuav_table_size;
				range_array[0].BaseShaderRegister = 0;// バインド先開始レジスタ.
				range_array[0].RegisterSpace = 0;

				range_array[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				range_array[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
				range_array[1].NumDescriptors = k_rt_global_descriptor_cbvsrvuav_table_size;
				range_array[1].BaseShaderRegister = 0;// バインド先開始レジスタ.
				range_array[1].RegisterSpace = 0;

				range_array[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
				range_array[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
				range_array[2].NumDescriptors = k_rt_global_descriptor_sampler_table_size;
				range_array[2].BaseShaderRegister = 0;// バインド先開始レジスタ.
				range_array[2].RegisterSpace = 0;

				range_array[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
				range_array[3].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
				range_array[3].NumDescriptors = k_rt_global_descriptor_cbvsrvuav_table_size;
				range_array[3].BaseShaderRegister = 0;// バインド先開始レジスタ.
				range_array[3].RegisterSpace = 0;

				for(auto i = 0; i < range_array.size(); ++i)
				{
					// GlobalRootSignature Parameter[1] 以降は色々固定のTable.

					root_param.push_back({});
					auto& parame_elem = root_param.back();
					parame_elem.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
					parame_elem.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
					parame_elem.DescriptorTable.NumDescriptorRanges = 1; // 1Table 1用途.
					parame_elem.DescriptorTable.pDescriptorRanges = &range_array[i];
				}

				if (!rhi::helper::SerializeAndCreateRootSignature(global_root_signature_, p_device, root_param.data(), (uint32_t)root_param.size()))
				{
					assert(false);
					return false;
				}
			}
			// Subobject Global Root Signature.
			subobject::SubobjectGlobalRootSignature so_grs = {};
			so_grs.Setup(global_root_signature_);

			/*
			// Local Root Signature.
			*/
			{
				// local_root_signature_fixed_
				std::vector<D3D12_ROOT_PARAMETER> root_param;

				// 現状ではLocal側はCBVとSRVのみで, SamplerやUAVはGlobalにしか登録しないことを考えている.
				std::vector<D3D12_DESCRIPTOR_RANGE> range_array;
				range_array.resize(2);

				range_array[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
				range_array[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // LocalRootSigだとこれが使えるかわからないのでダメだったら自前でオフセット値入れる.
				range_array[0].BaseShaderRegister = k_system_raytracing_local_register_start;
				range_array[0].NumDescriptors = k_rt_local_descriptor_cbvsrvuav_table_size;
				range_array[0].RegisterSpace = 0;

				range_array[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				range_array[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // LocalRootSigだとこれが使えるかわからないのでダメだったら自前でオフセット値入れる.
				range_array[1].BaseShaderRegister = k_system_raytracing_local_register_start;
				range_array[1].NumDescriptors = k_rt_local_descriptor_cbvsrvuav_table_size;
				range_array[1].RegisterSpace = 0;

				for (auto i = 0; i < range_array.size(); ++i)
				{
					root_param.push_back({});
					auto& parame_elem = root_param.back();
					parame_elem.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
					parame_elem.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
					parame_elem.DescriptorTable.NumDescriptorRanges = 1;
					parame_elem.DescriptorTable.pDescriptorRanges = &range_array[i];
				}

				if (!rhi::helper::SerializeAndCreateLocalRootSignature(local_root_signature_fixed_, p_device, root_param.data(), (uint32_t)root_param.size()))
				{
					assert(false);
					return false;
				}
			}
			subobject::SubobjectLocalRootSignature so_lgs_fixed = {};
			so_lgs_fixed.Setup(local_root_signature_fixed_);

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
			num_subobject += 1; // Local Root Signature 簡単のため全体で共有.
			num_subobject += 1; // Local Root Signature と全HitGroupとの関連付け.

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

			const auto lgs_fixed_index = cnt_subobject;// 共有Local Root Signature Subobjectの登録位置.
			state_subobject_array[cnt_subobject++] = (so_lgs_fixed.subobject_);

			subobject::SubobjectExportsAssociation so_assosiation_lgs_hitgroup = {};
			// LGSの位置が確定した後にそのアドレスを使った関連付けが必要なので一旦ここで.
			{
				std::vector<const char*> hitgroup_name_ptr_array;
				hitgroup_name_ptr_array.resize(hitgroup_database_.size());
				for (auto i = 0; i < hitgroup_database_.size(); ++i)
				{
					hitgroup_name_ptr_array[i] = hitgroup_database_[i].hitgorup_name.c_str();
				}

				so_assosiation_lgs_hitgroup.Setup(&state_subobject_array[lgs_fixed_index], hitgroup_name_ptr_array.data(), (int)hitgroup_name_ptr_array.size());
			}
			state_subobject_array[cnt_subobject++] = (so_assosiation_lgs_hitgroup.subobject_);


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


		// per_entry_descriptor_param_count が0だとAlignmentエラーになるため注意.
		// BLAS内Geometryは個別のShaderRecordを持つ(multiplier_for_subgeometry_index = 1)
		bool CreateShaderTable(RaytraceShaderTable& out, rhi::DeviceDep* p_device,
			rhi::FrameDescriptorAllocInterface& desc_alloc_interface, uint32_t desc_alloc_id,
			const RaytraceStructureTop& tlas, const RaytraceStateObject& state_object, const char* raygen_name, const char* miss_name)
		{
			out = {};


			// NOTE. 固定のDescriptorTableで CVBとSRVの2テーブルをLocalRootSignatureのリソースとして定義している.
			const uint32_t per_entry_descriptor_table_count = 2;

			const auto num_instance = tlas.NumInstance();
			uint32_t num_all_instance_geometry = 0;
			{
				const auto& instance_blas_index_array = tlas.GetInstanceBlasIndexArray();
				const auto& blas_array = tlas.GetBlasArray();
				for (auto i = 0u; i < num_instance; ++i)
				{
					num_all_instance_geometry += blas_array[instance_blas_index_array[i]]->NumGeometry();
				}
			}

			// Shader Table.
			// TODO. ASのインスタンス毎のマテリアルシェーダ情報からStateObjectのShaderIdentifierを取得してテーブルを作る.
			// https://github.com/Monsho/D3D12Samples/blob/95d1c3703cdcab816bab0b5dcf1a1e42377ab803/Sample013/src/main.cpp
			// https://github.com/microsoft/DirectX-Specs/blob/master/d3d/Raytracing.md#shader-tables

			constexpr uint32_t k_shader_identifier_byte_size = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
			// Table一つにつきベースのGPU Descriptor Handleを書き込むためのサイズ計算.
			const uint32_t shader_record_resource_byte_size = sizeof(D3D12_GPU_DESCRIPTOR_HANDLE) * per_entry_descriptor_table_count;

			const uint32_t shader_record_byte_size = rhi::align_to(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, k_shader_identifier_byte_size + shader_record_resource_byte_size);

			// 現状は全Instanceが別Table.
			// RayGenとMissが一つずつとする(Missは一応複数登録可だが簡易化のため一旦1つに).
			constexpr uint32_t num_raygen = 1;
			const uint32_t num_miss = 1;
			// Hitgroupのrecordは全Instanceの全Geometry分としている.
			const uint32_t shader_table_byte_size = shader_record_byte_size * (num_raygen + num_miss + num_all_instance_geometry);

			// あとで書き込み位置調整に使うので保存.
			out.table_entry_byte_size_ = shader_record_byte_size;

			// バッファ確保.
			rhi::BufferDep::Desc rt_shader_table_desc = {};
			rt_shader_table_desc.element_count = 1;
			rt_shader_table_desc.element_byte_size = shader_table_byte_size;
			rt_shader_table_desc.heap_type = rhi::ResourceHeapType::Upload;// CPUから直接書き込むため.
			if (!out.shader_table_.Initialize(p_device, rt_shader_table_desc))
			{
				assert(false);
				return false;
			}

			// レコード書き込み.
			if (auto* mapped = static_cast<uint8_t*>(out.shader_table_.Map()))
			{
				CComPtr<ID3D12StateObjectProperties> p_rt_so_prop;
				if (FAILED(state_object.GetStateObject()->QueryInterface(IID_PPV_ARGS(&p_rt_so_prop))))
				{
					assert(false);
				}

				uint32_t table_cnt = 0;

				// raygen
				out.table_raygen_offset_ = (shader_record_byte_size * table_cnt);
				{
					{
						memcpy(mapped + out.table_raygen_offset_, p_rt_so_prop->GetShaderIdentifier(str_to_wstr(raygen_name).c_str()), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

						// TODO. Local Root Signature で設定するリソースがある場合はここでGPU Descriptor Handleを書き込む.
					}
					++table_cnt;
				}

				out.table_miss_offset_ = (shader_record_byte_size * table_cnt);
				out.table_miss_count_ = 1;
				{
					// miss
					{
						memcpy(mapped + (shader_record_byte_size * table_cnt), p_rt_so_prop->GetShaderIdentifier(str_to_wstr(miss_name).c_str()), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

						// TODO. Local Root Signature で設定するリソースがある場合はここでGPU Descriptor Handleを書き込む.
					}
					++table_cnt;
				}


				// HitGroup
				/// マテリアル分存在するHitGroupは連続領域でInstanceに指定したインデックスでアクセスされるためここ以降に順序に気をつけて書き込み.
				// InstanceのBLASに複数のGeometryが含まれる場合はここでその分のrecordが書き込まれる.

				const auto table_hitgroup_offset = shader_record_byte_size * table_cnt;
				uint32_t hitgroup_count = 0;
				for (uint32_t i = 0u; i < num_instance; ++i)
				{
					// 現状はBLAS内Geometryはすべて同じHitgroupとしている.
					// Geometry毎に別マテリアル等とする場合はここをGeomety毎とする.
					const uint32_t hitgroup_id = tlas.GetInstanceHitgroupIndexArray()[i];
					const char* hitgroup_name = state_object.GetHitgroupName(hitgroup_id);
					assert(nullptr != hitgroup_name);

					// Geometry毎のHitgroup分けの確認用.
					const uint32_t debug_hitgroup_id = 0;
					const char* debug_hitgroup_name = state_object.GetHitgroupName(debug_hitgroup_id);

					// 内部Geometry毎にRecord.
					const auto& blas_index = tlas.GetInstanceBlasIndexArray()[i];
					const auto& blas = tlas.GetBlasArray()[blas_index];
					for (uint32_t geom_i = 0; geom_i < blas->NumGeometry(); ++geom_i)
					{
						auto* geom_hit_group_name = hitgroup_name;
						if (0 == (geom_i & 0x01))
						{
							// デバッグ用にGeometry毎にHitgroup変更テスト.
							geom_hit_group_name = debug_hitgroup_name;
						}

						// hitGroup
						{
							// TODO. Local Root Signature で設定するリソースがある場合はここでGPU Descriptor Handleを書き込む.
							// 固定LocalRootSigにより
							//	DescriptorTable0 -> b1000からCBV最大16
							//	DescriptorTable1 -> t1000からSRV最大16
							// というレイアウトで登録する.
							// ここでフレームIndexで自動解放されないタイプのFrameDescriptor(と同じHeapから確保できる)のDescriptorが必要.
							

							DescriptorHandleSet desc_handle_cbv;
							DescriptorHandleSet desc_handle_srv;
							const bool result_alloc_desc_cbv = desc_alloc_interface.Allocate(desc_alloc_id, k_rt_local_descriptor_cbvsrvuav_table_size, desc_handle_cbv.h_cpu, desc_handle_cbv.h_gpu);
							const bool result_alloc_desc_srv = desc_alloc_interface.Allocate(desc_alloc_id, k_rt_local_descriptor_cbvsrvuav_table_size, desc_handle_srv.h_cpu, desc_handle_srv.h_gpu);
							assert(result_alloc_desc_cbv && result_alloc_desc_srv);

							// 描画用HeapにDescriptorコピー.
							{
								const auto desc_stride = desc_alloc_interface.GetFrameDescriptorManager()->GetHandleIncrementSize();
								auto func_get_offseted_desc_handle = [](const D3D12_CPU_DESCRIPTOR_HANDLE& h_cpu, uint32_t offset)
								{
									D3D12_CPU_DESCRIPTOR_HANDLE ret = h_cpu;
									ret.ptr += offset;
									return ret;
								};

								const auto geom_data = blas->GetGeometryData(geom_i);
								assert(geom_data.vertex_srv);
								assert(geom_data.index_srv);

								p_device->GetD3D12Device()->CopyDescriptorsSimple(1, desc_handle_srv.h_cpu, geom_data.vertex_srv->GetView().cpu_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
								p_device->GetD3D12Device()->CopyDescriptorsSimple(1, func_get_offseted_desc_handle(desc_handle_srv.h_cpu, desc_stride * 1), geom_data.index_srv->GetView().cpu_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
							

								// まだLocalにCBVは無い.
							}

							// 書き込み
							
							// Shader Identifier
							memcpy(mapped + (table_hitgroup_offset + shader_record_byte_size * hitgroup_count), p_rt_so_prop->GetShaderIdentifier(str_to_wstr(geom_hit_group_name).c_str()), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
							
							auto record_res_offset = (table_hitgroup_offset + shader_record_byte_size * hitgroup_count) + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

							// CBV Table
							memcpy(mapped + record_res_offset, &desc_handle_cbv.h_gpu, sizeof(D3D12_GPU_DESCRIPTOR_HANDLE));
							record_res_offset += sizeof(D3D12_GPU_DESCRIPTOR_HANDLE);

							// SRV Table
							memcpy(mapped + record_res_offset, &desc_handle_srv.h_gpu, sizeof(D3D12_GPU_DESCRIPTOR_HANDLE));
							record_res_offset += sizeof(D3D12_GPU_DESCRIPTOR_HANDLE);
						}
						++hitgroup_count;
					}
				}
				out.table_hitgroup_offset_ = table_hitgroup_offset;
				out.table_hitgroup_count_ = hitgroup_count;

				out.shader_table_.Unmap();
			}
			return true;
		}
		// -------------------------------------------------------------------------------
		namespace
		{
			struct CbSceneView
			{
				math::Mat34 cb_view_mtx;
				math::Mat34 cb_view_inv_mtx;
				math::Mat44 cb_proj_mtx;
				math::Mat44 cb_proj_inv_mtx;

				// 正規化デバイス座標(NDC)のZ値からView空間Z値を計算するための係数. PerspectiveProjectionMatrixの方式によってCPU側で計算される値を変えることでシェーダ側は同一コード化.
				//	view_z = cb_ndc_z_to_view_z_coef.x / ( ndc_z * cb_ndc_z_to_view_z_coef.y + cb_ndc_z_to_view_z_coef.z )
				//
				//		cb_ndc_z_to_view_z_coef = 
				//			Standard RH: (-far_z * near_z, near_z - far_z, far_z, 0.0)
				//			Standard LH: ( far_z * near_z, near_z - far_z, far_z, 0.0)
				//			Reverse RH: (-far_z * near_z, far_z - near_z, near_z, 0.0)
				//			Reverse LH: ( far_z * near_z, far_z - near_z, near_z, 0.0)
				//			Infinite Far Reverse RH: (-near_z, 1.0, 0.0, 0.0)
				//			Infinite Far Reverse RH: ( near_z, 1.0, 0.0, 0.0)
				math::Vec4	cb_ndc_z_to_view_z_coef;
			};
		}

		RaytraceStructureManager::RaytraceStructureManager()
		{
		}
		RaytraceStructureManager::~RaytraceStructureManager()
		{
			// BLASインスタンス解放.
			for (auto i = 0; i < blas_array_.size(); ++i)
			{
				if (blas_array_[i])
				{
					delete blas_array_[i];
					blas_array_[i] = nullptr;
				}
			}

			// Descriptor解放.
			desc_alloc_interface_.Deallocate(desc_alloc_id_);
		}
		bool RaytraceStructureManager::Initialize(rhi::DeviceDep* p_device, RaytraceStateObject* p_state,
			const std::vector<RaytraceBlasInstanceGeometryDesc>& geom_array,
			const std::vector<uint32_t>& instance_geom_id_array,
			const std::vector<math::Mat34>& instance_transform_array,
			const std::vector<uint32_t>& instance_hitgroup_id_array)
		{
			// Descriptor確保用Interface初期化.
			{
				rhi::FrameDescriptorAllocInterface::Desc descriptor_interface_desc = {};
				descriptor_interface_desc.allow_frame_flip_index = false;	// FrameFlipと被らないようにfalse指定.
				desc_alloc_interface_.Initialize(p_device->GetFrameDescriptorManager(), descriptor_interface_desc);

				// 確保IDはFrameFlipと被らないIDに設定.
				// とりあえず最上位bit 1 としておく.
				desc_alloc_id_ = (1u << 31u) | (1u);
			}

			// StateObjectは外部から.
			assert(p_state);
			if (!p_state)
			{
				return false;
			}
			p_state_object_ = p_state;

			// 入力ジオメトリ情報からBLAS生成.
			blas_array_.clear();
			blas_array_.reserve(geom_array.size());
			for (auto& g : geom_array)
			{
				if (nullptr == g.pp_desc || 0 >= g.num_desc)
					continue;

				std::vector<RaytraceStructureBottomGeometryDesc> blas_geom_desc_arrray = {};
				blas_geom_desc_arrray.reserve(g.num_desc);
				
				for (uint32_t gi = 0; gi < g.num_desc; ++gi)
				{
					blas_geom_desc_arrray.push_back({});
					auto& geom_desc = blas_geom_desc_arrray[blas_geom_desc_arrray.size() - 1];

					geom_desc.vertex_buffer = g.pp_desc[gi].vertex_buffer;
					geom_desc.index_buffer = g.pp_desc[gi].index_buffer;
				}

				// New Blas.
				blas_array_.push_back(new RaytraceStructureBottom());
				auto& blas = blas_array_[blas_array_.size() - 1];
				// Setup.
				blas->Setup(p_device, blas_geom_desc_arrray);
			}


			if (!test_tlas_.Setup(p_device, blas_array_, instance_geom_id_array, instance_transform_array, instance_hitgroup_id_array))
			{
				assert(false);
				return false;
			}

			// 念のためDescriptor解放. 別のインスタンスで同じIDを使っている場合は問題となるため注意.
			desc_alloc_interface_.Deallocate(desc_alloc_id_);
			if (!CreateShaderTable(shader_table_, p_device, desc_alloc_interface_, desc_alloc_id_, test_tlas_, *p_state_object_, "rayGen", "miss"))
			{
				assert(false);
				return false;
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
			}

			// SceneView定数バッファ.
			{
				rhi::BufferDep::Desc buff_desc = {};
				buff_desc.heap_type = rhi::ResourceHeapType::Upload;
				buff_desc.bind_flag = rhi::ResourceBindFlag::ConstantBuffer;
				buff_desc.element_count = 1;
				buff_desc.element_byte_size = sizeof(CbSceneView);
				for (auto i = 0; i < std::size(cb_test_scene_view); ++i)
				{
					cb_test_scene_view[i].Initialize(p_device, buff_desc);
					cbv_test_scene_view[i].Initialize(&cb_test_scene_view[i], {});
				}
			}

			return true;
		}

		void RaytraceStructureManager::UpdateOnRender(rhi::DeviceDep* p_device, rhi::GraphicsCommandListDep* p_command_list)
		{
			++frame_count_;
			const uint32_t safe_frame_count_ = frame_count_ % 10000;

			// 全BLASビルド.
			for (auto& e : blas_array_)
			{
				if (nullptr != e && !e->IsBuilt())
				{
					e->Build(p_device, p_command_list);
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

#if 0
			static float test_view_rot_radian = 0.0f;
			test_view_rot_radian += 2.0f * math::k_pi_f / 600.0f;
			if (2.0f * math::k_pi_f < test_view_rot_radian)
			{
				test_view_rot_radian -= 2.0f * math::k_pi_f;
			}
			float test_view_pos_y = 2.2f;
			
			math::Mat34 view_mat = math::CalcViewMatrix(math::Vec3(0, test_view_pos_y, 0.0f), math::Vec3(sin(test_view_rot_radian), 0, std::cosf(test_view_rot_radian)), math::Vec3(0, 1, 0));
#else
			math::Mat34 view_mat = math::CalcViewMatrix(camera_pos_, camera_dir_, camera_up_);
#endif
			const float fov_y = math::Deg2Rad(50.0f);
			const float near_z = 0.1f;
			const float far_z = 10000.0f;
#if 1
			// Infinite Far Reverse Perspective
			math::Mat44 proj_mat = math::CalcReverseInfiniteFarPerspectiveMatrix(fov_y, 16.0f / 9.0f, 0.1f);
			math::Vec4 ndc_z_to_view_z_coef = math::CalcViewDepthReconstructCoefForInfiniteFarReversePerspective(near_z);
#elif 0
			// Reverse Perspective
			math::Mat44 proj_mat = math::CalcReversePerspectiveMatrix(fov_y, 16.0f / 9.0f, 0.1f, far_z);
			math::Vec4 ndc_z_to_view_z_coef = math::CalcViewDepthReconstructCoefForReversePerspective(near_z, far_z);
#else
			// 標準Perspective
			math::Mat44 proj_mat = math::CalcStandardPerspectiveMatrix(fov_y, 16.0f / 9.0f, 0.1f, far_z);
			math::Vec4 ndc_z_to_view_z_coef = math::CalcViewDepthReconstructCoefForStandardPerspective(near_z, far_z);
#endif
			// 定数バッファ更新.
			{
				const auto cb_index = frame_count_ % std::size(cb_test_scene_view);
				if (auto* mapped = static_cast<CbSceneView*>(cb_test_scene_view[cb_index].Map()))
				{
					mapped->cb_view_mtx = view_mat;
					mapped->cb_proj_mtx = proj_mat;
					mapped->cb_view_inv_mtx = math::Mat34::Inverse(view_mat);
					mapped->cb_proj_inv_mtx = math::Mat44::Inverse(proj_mat);

					mapped->cb_ndc_z_to_view_z_coef = ndc_z_to_view_z_coef;

					cb_test_scene_view[cb_index].Unmap();
				}
			}
		}

		void RaytraceStructureManager::DispatchRay(rhi::GraphicsCommandListDep* p_command_list)
		{
			const auto cb_index = frame_count_ % std::size(cb_test_scene_view);

			rhi::DeviceDep* p_device = p_command_list->GetDevice();
			auto* d3d_device = p_device->GetD3D12Device();

			auto* d3d_command_list = p_command_list->GetD3D12GraphicsCommandListForDxr();
			auto shader_table_head = shader_table_.shader_table_.GetD3D12Resource()->GetGPUVirtualAddress();


			// to UAV.
			p_command_list->ResourceBarrier(&ray_result_, ray_result_state_, rhi::ResourceState::UnorderedAccess);
			ray_result_state_ = rhi::ResourceState::UnorderedAccess;



			// Bind the empty root signature
			d3d_command_list->SetComputeRootSignature(p_state_object_->GetGlobalRootSignature());
			// State.
			d3d_command_list->SetPipelineState1(p_state_object_->GetStateObject());

			// Globalリソース設定.
			{
				// ASとCBV,SRV,UAVの3種それぞれに固定数分でframe descriptor heap確保.
				const int num_frame_descriptor_cbvsrvuav_count = k_rt_global_descriptor_cbvsrvuav_table_size * 3;
				const int num_frame_descriptor_sampler_count = k_rt_global_descriptor_sampler_table_size;


				const auto resource_descriptor_step_size = p_command_list->GetFrameDescriptorInterface()->GetFrameDescriptorManager()->GetHandleIncrementSize();
				const auto sampler_descriptor_step_size = p_command_list->GetFrameSamplerDescriptorHeapInterface()->GetHandleIncrementSize();
				auto get_descriptor_with_pos = [](const DescriptorHandleSet& base,  int i, u32 handle_step_size) -> DescriptorHandleSet
				{
					DescriptorHandleSet ret(base);
					ret.h_cpu.ptr += handle_step_size * i;
					ret.h_gpu.ptr += handle_step_size * i;
					return ret;
				};


				DescriptorHandleSet res_heap_head;
				DescriptorHandleSet sampler_heap_head;
				// CbvSrvUavのFrame Heap確保.
				if (!p_command_list->GetFrameDescriptorInterface()->Allocate(num_frame_descriptor_cbvsrvuav_count, res_heap_head.h_cpu, res_heap_head.h_gpu))
				{
					assert(false);
				}
				// SamplerのFrame Heap確保. ここの確保でHeapのページが足りない場合は別のHeapが確保されて切り替わる.
				// そのためSetDescriptorHeaps用のSampler用Heapを取得する場合は確保のあとにGetD3D12DescriptorHeapをすること.
				if (!p_command_list->GetFrameSamplerDescriptorHeapInterface()->Allocate(num_frame_descriptor_sampler_count, sampler_heap_head.h_cpu, sampler_heap_head.h_gpu))
				{
					assert(false);
				}

				// frame heap 上のそれぞれの配置.
				DescriptorHandleSet descriptor_table_base_cbv = get_descriptor_with_pos(res_heap_head, k_rt_global_descriptor_cbvsrvuav_table_size * 0, resource_descriptor_step_size);
				DescriptorHandleSet descriptor_table_base_srv = get_descriptor_with_pos(res_heap_head, k_rt_global_descriptor_cbvsrvuav_table_size * 1, resource_descriptor_step_size);
				DescriptorHandleSet descriptor_table_base_uav = get_descriptor_with_pos(res_heap_head, k_rt_global_descriptor_cbvsrvuav_table_size * 2, resource_descriptor_step_size);
				DescriptorHandleSet descriptor_table_base_sampler = get_descriptor_with_pos(sampler_heap_head, k_rt_global_descriptor_sampler_table_size * 0, sampler_descriptor_step_size);
				{
					// FrameHeapにコピーする.

					// Scene CBV
					d3d_device->CopyDescriptorsSimple(1, descriptor_table_base_cbv.h_cpu, cbv_test_scene_view[cb_index].GetView().cpu_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


					// uav 出力先のテストUAV.
					d3d_device->CopyDescriptorsSimple(1, descriptor_table_base_uav.h_cpu, ray_result_uav_.GetView().cpu_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

					// TODO.
				}

				// Heap設定. desc_alloc_interface_ はHeapとしてはCommandListと同じ巨大なHeapから切り出して利用しているため同一Heapで良い.
				{
					std::vector<ID3D12DescriptorHeap*> use_heap_array = {};
					use_heap_array.push_back(p_command_list->GetFrameDescriptorInterface()->GetFrameDescriptorManager()->GetD3D12DescriptorHeap());
					use_heap_array.push_back(p_command_list->GetFrameSamplerDescriptorHeapInterface()->GetD3D12DescriptorHeap());

					// 使用しているHeapをセット.
					d3d_command_list->SetDescriptorHeaps((uint32_t)use_heap_array.size(), use_heap_array.data());
				}

				// Table設定.
				d3d_command_list->SetComputeRootShaderResourceView(0, test_tlas_.GetBuffer()->GetD3D12Resource()->GetGPUVirtualAddress());
				d3d_command_list->SetComputeRootDescriptorTable(1, descriptor_table_base_cbv.h_gpu);
				d3d_command_list->SetComputeRootDescriptorTable(2, descriptor_table_base_srv.h_gpu);
				d3d_command_list->SetComputeRootDescriptorTable(3, sampler_heap_head.h_gpu);
				d3d_command_list->SetComputeRootDescriptorTable(4, descriptor_table_base_uav.h_gpu);
			}


			// Dispatch.
			D3D12_DISPATCH_RAYS_DESC raytraceDesc = {};
			raytraceDesc.Width = ray_result_.GetDesc().width;
			raytraceDesc.Height = ray_result_.GetDesc().height;
			raytraceDesc.Depth = 1;

			// RayGeneration Shaderのテーブル位置.
			raytraceDesc.RayGenerationShaderRecord.StartAddress = shader_table_head + shader_table_.table_raygen_offset_;
			raytraceDesc.RayGenerationShaderRecord.SizeInBytes = shader_table_.table_entry_byte_size_;

			// Miss Shaderのテーブル位置.
			raytraceDesc.MissShaderTable.StartAddress = shader_table_head + shader_table_.table_miss_offset_;
			raytraceDesc.MissShaderTable.StrideInBytes = shader_table_.table_entry_byte_size_;
			raytraceDesc.MissShaderTable.SizeInBytes = shader_table_.table_entry_byte_size_ * shader_table_.table_miss_count_;// shader_table_.table_entry_byte_size_;   // Only a s single miss-entry

			// HitGroup群の先頭のテーブル位置.
			// マテリアル毎のHitGroupはここから連続領域に格納. Instanceに設定されたHitGroupIndexでアクセスされる.
			raytraceDesc.HitGroupTable.StartAddress = shader_table_head + shader_table_.table_hitgroup_offset_;
			raytraceDesc.HitGroupTable.StrideInBytes = shader_table_.table_entry_byte_size_;
			raytraceDesc.HitGroupTable.SizeInBytes = shader_table_.table_entry_byte_size_ * shader_table_.table_hitgroup_count_;
			
			d3d_command_list->DispatchRays(&raytraceDesc);


			// to SRV.
			p_command_list->ResourceBarrier(&ray_result_, ray_result_state_, rhi::ResourceState::ShaderRead);
			ray_result_state_ = rhi::ResourceState::ShaderRead;
		}

		void  RaytraceStructureManager::SetCameraInfo(const math::Vec3& position, const math::Vec3& dir, const math::Vec3& up)
		{
			camera_pos_ = position;
			camera_dir_ = dir;
			camera_up_ = up;
		}

	}
}