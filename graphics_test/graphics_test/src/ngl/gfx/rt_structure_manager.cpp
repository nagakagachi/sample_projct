﻿#include "rt_structure_manager.h"

#include <unordered_map>
#include <algorithm>

namespace ngl
{
	namespace gfx
	{
		static constexpr uint32_t k_frame_descriptor_cbvsrvuav_table_size = 16;
		static constexpr uint32_t k_frame_descriptor_sampler_table_size = 16;


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
		bool RaytraceStructureTop::Setup(rhi::DeviceDep* p_device, RaytraceStructureBottom* p_blas, 
			const std::vector<Mat34>& instance_transform_array, const std::vector<uint32_t>& instance_hitgroup_id_array)
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
			{
				transform_array_ = instance_transform_array;// copy.
			
				hitgroup_id_array_.resize(transform_array_.size());
				const auto num_copy_hitgroup_id_count = std::min(instance_hitgroup_id_array.size(), hitgroup_id_array_.size());
				std::copy_n(instance_hitgroup_id_array.begin(), num_copy_hitgroup_id_count, hitgroup_id_array_.begin());
				if (num_copy_hitgroup_id_count < hitgroup_id_array_.size())
				{
					// コピーされなかった部分を0fill.
					std::fill_n(hitgroup_id_array_.data() + num_copy_hitgroup_id_count, hitgroup_id_array_.size() - num_copy_hitgroup_id_count, 0);
				}
			}

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
					mapped[inst_i].InstanceContributionToHitGroupIndex = inst_i; // 現状はInstance毎に個別のShaderTableエントリ.
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

		uint32_t RaytraceStructureTop::NumInstance() const
		{
			return static_cast<uint32_t>(hitgroup_id_array_.size());
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

				/*
					D3D12_ROOT_SIGNATURE_DESC root_signature_desc = {};
					root_signature_desc.NumParameters = 0;
					root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;// Local設定.
					rhi::helper::SerializeAndCreateRootSignature(p_device, root_signature_desc, rt_local_root_signature0_);

					Setup(rt_local_root_signature0_);
				*/
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
				const D3D12_STATE_SUBOBJECT* p_subobject_ = {};
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
				// おそらく使わないであろうレジスタにASを設定.
				// t65535 -> AccelerationStructure.

				std::vector<D3D12_ROOT_PARAMETER> root_param;
				{
					// GlobalRootSignature 0 は固定でAccelerationStructure用SRV.
					root_param.push_back({});
					auto& parame_elem = root_param.back();
					parame_elem.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
					parame_elem.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;  // SRV Descriptor直接.
					parame_elem.Descriptor.ShaderRegister = 65535; // t65535にシステムからASを設定.
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
				range_array[0].NumDescriptors = k_frame_descriptor_cbvsrvuav_table_size;
				range_array[0].BaseShaderRegister = 0;// バインド先開始レジスタ.
				range_array[0].RegisterSpace = 0;

				range_array[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				range_array[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
				range_array[1].NumDescriptors = k_frame_descriptor_cbvsrvuav_table_size;
				range_array[1].BaseShaderRegister = 0;// バインド先開始レジスタ.
				range_array[1].RegisterSpace = 0;

				range_array[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
				range_array[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
				range_array[2].NumDescriptors = k_frame_descriptor_sampler_table_size;
				range_array[2].BaseShaderRegister = 0;// バインド先開始レジスタ.
				range_array[2].RegisterSpace = 0;

				range_array[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
				range_array[3].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
				range_array[3].NumDescriptors = k_frame_descriptor_cbvsrvuav_table_size;
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


		// per_entry_descriptor_param_count が0だとAlignmentエラーになるため注意.
		bool CreateShaderTable(RaytraceShaderTable& out, rhi::DeviceDep* p_device,
			const RaytraceStructureTop& tlas, const RaytraceStateObject& state_object, const char* raygen_name, const char* miss_name, uint32_t per_entry_descriptor_param_count)
		{
			out = {};

			// Shader Table.
			// TODO. ASのインスタンス毎のマテリアルシェーダ情報からStateObjectのShaderIdentifierを取得してテーブルを作る.
			// https://github.com/Monsho/D3D12Samples/blob/95d1c3703cdcab816bab0b5dcf1a1e42377ab803/Sample013/src/main.cpp
			// https://github.com/microsoft/DirectX-Specs/blob/master/d3d/Raytracing.md#shader-tables

			constexpr uint32_t k_shader_identifier_byte_size = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
			// Table一つにつきベースのGPU Descriptor Handleを書き込むためのサイズ計算.
			const uint32_t shader_record_resource_byte_size = sizeof(D3D12_GPU_DESCRIPTOR_HANDLE) * per_entry_descriptor_param_count;

			const uint32_t shader_record_byte_size = rhi::align_to(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, k_shader_identifier_byte_size + shader_record_resource_byte_size);

			// 現状は全Instanceが別Table.
			// RayGenとMissが一つずつとする(Missは一応複数登録可だが簡易化のため一旦1つに).
			constexpr uint32_t num_raygen = 1;
			const uint32_t num_miss = 1;
			const uint32_t shader_table_byte_size = shader_record_byte_size * (num_raygen + num_miss + tlas.NumInstance());

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
				out.table_hitgroup_offset_ = shader_record_byte_size * table_cnt;
				for (uint32_t i = 0u; i < tlas.NumInstance(); ++i)
				{
					const uint32_t hitgroup_id = tlas.GetInstanceHitgroupIndexArray()[i];

					const char* hitgroup_name = state_object.GetHitgroupName(hitgroup_id);
					assert(nullptr != hitgroup_name);

					// hitGroup
					{
						memcpy(mapped + (shader_record_byte_size * table_cnt), p_rt_so_prop->GetShaderIdentifier(str_to_wstr(hitgroup_name).c_str()), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

						// TODO. Local Root Signature で設定するリソースがある場合はここでGPU Descriptor Handleを書き込む.
					}
					++table_cnt;
				}

				out.shader_table_.Unmap();
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
		bool RaytraceStructureManager::Initialize(rhi::DeviceDep* p_device, RaytraceStateObject* p_state)
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

			// StateObjectは外部から.
			assert(p_state);
			if (!p_state)
			{
				return false;
			}
			p_state_object_ = p_state;




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
			std::vector<uint32_t> test_inst_hitgroup_index;
			{
				{
					test_inst_transforms.push_back(Mat34::Identity());
					test_inst_hitgroup_index.push_back(1);
				}
				{
					Mat34 tmp_m = Mat34::Identity();
					tmp_m.m[0][3] = 2.0f;
					test_inst_transforms.push_back(tmp_m);
					test_inst_hitgroup_index.push_back(0);
				}
				{
					Mat34 tmp_m = Mat34::Identity();
					tmp_m.m[0][3] = -2.0f;
					test_inst_transforms.push_back(tmp_m);
					test_inst_hitgroup_index.push_back(1);
				}
				{
					Mat34 tmp_m = Mat34::Identity();
					tmp_m.m[0][3] = -2.0f;
					tmp_m.m[1][3] = -1.0f;
					test_inst_transforms.push_back(tmp_m);
					test_inst_hitgroup_index.push_back(0);
				}
			}
			if (!test_tlas_.Setup(p_device, &test_blas_, test_inst_transforms, test_inst_hitgroup_index))
			{
				assert(false);
				return false;
			}


			if (!CreateShaderTable(shader_table_, p_device, test_tlas_, *p_state_object_, "rayGen", "miss", 1))
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
			rhi::DeviceDep* p_device = p_command_list->GetDevice();
			auto* d3d_device = p_device->GetD3D12Device();

			auto* d3d_command_list = p_command_list->GetD3D12GraphicsCommandListForDxr();
			//auto shader_table_head = rt_shader_table_.GetD3D12Resource()->GetGPUVirtualAddress();
			auto shader_table_head = shader_table_.shader_table_.GetD3D12Resource()->GetGPUVirtualAddress();


			// to UAV.
			p_command_list->ResourceBarrier(&ray_result_, ray_result_state_, rhi::ResourceState::UnorderedAccess);
			ray_result_state_ = rhi::ResourceState::UnorderedAccess;



			// Bind the empty root signature
			d3d_command_list->SetComputeRootSignature(p_state_object_->GetGlobalRootSignature());
			// State.
			d3d_command_list->SetPipelineState1(p_state_object_->GetStateObject());

			// リソース設定.
			{
				// ASとCBV,SRV,UAVの3種それぞれに固定数分でframe descriptor heap確保.
				const int num_frame_descriptor_cbvsrvuav_count = k_frame_descriptor_cbvsrvuav_table_size * 3;
				const int num_frame_descriptor_sampler_count = k_frame_descriptor_sampler_table_size;


				struct DescriptorHandleSet
				{
					DescriptorHandleSet() { }
					DescriptorHandleSet(const D3D12_CPU_DESCRIPTOR_HANDLE& cpu_handle, const D3D12_GPU_DESCRIPTOR_HANDLE& gpu_handle)
					{
						h_cpu = cpu_handle;
						h_gpu = gpu_handle;
					}
					D3D12_CPU_DESCRIPTOR_HANDLE h_cpu = {};
					D3D12_GPU_DESCRIPTOR_HANDLE h_gpu = {};
				};

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
				DescriptorHandleSet descriptor_table_base_cbv = get_descriptor_with_pos(res_heap_head, k_frame_descriptor_cbvsrvuav_table_size * 0, resource_descriptor_step_size);
				DescriptorHandleSet descriptor_table_base_srv = get_descriptor_with_pos(res_heap_head, k_frame_descriptor_cbvsrvuav_table_size * 1, resource_descriptor_step_size);
				DescriptorHandleSet descriptor_table_base_uav = get_descriptor_with_pos(res_heap_head, k_frame_descriptor_cbvsrvuav_table_size * 2, resource_descriptor_step_size);
				DescriptorHandleSet descriptor_table_base_sampler = get_descriptor_with_pos(sampler_heap_head, k_frame_descriptor_sampler_table_size * 0, sampler_descriptor_step_size);
				{
					// FrameHeapにコピーする.

					// uav 今のところこれだけだが今後ほかのSRVやその他UAV等も.
					d3d_device->CopyDescriptorsSimple(1, descriptor_table_base_uav.h_cpu, ray_result_uav_.GetView().cpu_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

					// TODO.
				}

				// Heap設定.
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
			raytraceDesc.MissShaderTable.SizeInBytes = shader_table_.table_entry_byte_size_;   // Only a s single miss-entry

			// HitGroup群の先頭のテーブル位置.
			// マテリアル毎のHitGroupはここから連続領域に格納. Instanceに設定されたHitGroupIndexでアクセスされる.
			raytraceDesc.HitGroupTable.StartAddress = shader_table_head + shader_table_.table_hitgroup_offset_;
			raytraceDesc.HitGroupTable.StrideInBytes = shader_table_.table_entry_byte_size_;
			raytraceDesc.HitGroupTable.SizeInBytes = shader_table_.table_entry_byte_size_;
			
			d3d_command_list->DispatchRays(&raytraceDesc);


			// to SRV.
			p_command_list->ResourceBarrier(&ray_result_, ray_result_state_, rhi::ResourceState::ShaderRead);
			ray_result_state_ = rhi::ResourceState::ShaderRead;
		}


	}
}