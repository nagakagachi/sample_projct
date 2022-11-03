#pragma once

#include <cstring>
// for wchar convert.
#include <stdlib.h>

#include <codecvt>
#include <locale>


#include "ngl/rhi/d3d12/rhi.d3d12.h"
#include "ngl/rhi/d3d12/rhi_command_list.d3d12.h"
#include "ngl/rhi/d3d12/rhi_resource.d3d12.h"
#include "ngl/rhi/d3d12/rhi_resource_view.d3d12.h"

namespace
{
	// Convert a wide Unicode string to an UTF8 string
	std::string wst_to_str(const std::wstring& wstr)
	{
		if (wstr.empty()) return std::string();
		int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
		std::string strTo(size_needed, 0);
		WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
		return strTo;
	}

	// Convert an UTF8 string to a wide Unicode String
	std::wstring str_to_wstr(const std::string& str)
	{
		if (str.empty()) return std::wstring();
		int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
		std::wstring wstrTo(size_needed, 0);
		MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
		return wstrTo;
	}
}

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

			// BLAS setup. 必要なBufferやViewの生成まで実行する. Buffer上にAccelerationStructureをビルドするのはBuild関数まで遅延する.
			// index_buffer : optional.
			// bufferの管理責任は外部.
			bool Setup(rhi::DeviceDep* p_device, rhi::BufferDep* vertex_buffer, rhi::BufferDep* index_buffer = nullptr);

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

			// build info.
			// Setupでバッファや設定を登録される. これを用いてRenderThreadでCommandListにビルドタスクを発行する.
			D3D12_RAYTRACING_GEOMETRY_DESC geom_desc_ = {};
			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS build_setup_info_ = {};


			// built data.
			rhi::BufferDep scratch_;
			rhi::BufferDep main_;
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

			// TLAS setup. 必要なBufferやViewの生成まで実行する. Buffer上にAccelerationStructureをビルドするのはBuild関数まで遅延する.
			// index_buffer : optional.
			// bufferの管理責任は外部.
			bool Setup(rhi::DeviceDep* p_device, RaytraceStructureBottom* p_blas, const std::vector<Mat34>& instance_transform_array);

			// SetupAs... の情報を元に構造構築コマンドを発行する.
			// Buildタイミングをコントロールするために分離している.
			// TODO RenderDocでのLaunchはクラッシュするのでNsight推奨.
			bool Build(rhi::DeviceDep* p_device, rhi::GraphicsCommandListDep* p_command_list);

			bool IsSetuped() const;
			bool IsBuilt() const;

			// Get AccelerationStructure Buffer.
			rhi::BufferDep* GetBuffer();
			const rhi::BufferDep* GetBuffer() const;
			// Get AccelerationStructure Srv.
			rhi::ShaderResourceViewDep* GetSrv();
			const rhi::ShaderResourceViewDep* GetSrv() const;

		private:
			bool is_built_ = false;

			// setup data.
			SETUP_TYPE		setup_type_ = SETUP_TYPE::NONE;
			RaytraceStructureBottom* p_blas_ = nullptr;
			std::vector<Mat34> transform_array_;

			// build info.
			// Setupでバッファや設定を登録される. これを用いてRenderThreadでCommandListにビルドタスクを発行する.
			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS build_setup_info_ = {};

			// built data.
			rhi::BufferDep instance_buffer_;
			rhi::BufferDep scratch_;
			rhi::BufferDep main_;
			rhi::ShaderResourceViewDep main_srv_;
			int tlas_byte_size_ = 0;
		};



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

					if (anyhit)
					{
						anyhit_name_cache_ = str_to_wstr(anyhit);
						hit_group_desc_.AnyHitShaderImport = anyhit_name_cache_.c_str();
					}
					if (closesthit)
					{
						closesthit_name_cache_ = str_to_wstr(closesthit);
						hit_group_desc_.ClosestHitShaderImport = closesthit_name_cache_.c_str();
					}
					if (intersection)
					{
						intersection_name_cache_ = str_to_wstr(intersection);
						hit_group_desc_.IntersectionShaderImport = intersection_name_cache_.c_str();
					}
					if (hitgroup_name)
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
				const D3D12_STATE_SUBOBJECT*			p_subobject_ = {};
				std::vector<std::wstring>				export_name_cache_;
				std::vector<const wchar_t*>				export_name_array_;
			};
		}


		// Raytracing Scene Manager.
		class RaytraceStructureManager
		{
		public:
			RaytraceStructureManager();
			~RaytraceStructureManager();

			bool Initialize(rhi::DeviceDep* p_device);
			void UpdateOnRender(rhi::DeviceDep* p_device, rhi::GraphicsCommandListDep* p_command_list);

			void DispatchRay(rhi::GraphicsCommandListDep* p_command_list);


			const rhi::ShaderResourceViewDep* GetResultSrv() const
			{
				return &ray_result_srv_;
			}

		private:
			// Vtx Buffer for BLAS.
			rhi::BufferDep test_geom_vb_;
			// BLAS.
			RaytraceStructureBottom test_blas_;
			// TLAS.
			RaytraceStructureTop test_tlas_;


			// Shader lib.
			rhi::ShaderDep rt_shader_lib0_;
			// local root signature.
			CComPtr<ID3D12RootSignature> rt_local_root_signature0_;

			// global root signature.
			CComPtr<ID3D12RootSignature> rt_global_root_signature_;

			// Raytracing StateObject.
			CComPtr<ID3D12StateObject> rt_state_oject_;

			rhi::BufferDep	rt_shader_table_;
			uint32_t		rt_shader_table_entry_byte_size_ = 0;
			// ShaderTable上のHitGroup領域の先頭へのオフセット.

			uint32_t		rt_shader_table_raygen_offset = 0;
			uint32_t		rt_shader_table_miss_offset = 0;
			uint32_t		rt_shader_table_hitgroup_offset = 0;


			// Raytrace用のCBV,SRV,UAV用Heap.
			// 更新や足りなくなったら作り直される予定.
			//CComPtr<ID3D12DescriptorHeap>	rt_resource_descriptor_heap_;
			rhi::DescriptorHeapWrapper	rt_descriptor_heap_;


			// テスト用のRayDispatch出力先UAV.
			rhi::TextureDep				ray_result_;
			rhi::ShaderResourceViewDep	ray_result_srv_;
			rhi::UnorderedAccessView	ray_result_uav_;
			rhi::ResourceState			ray_result_state_ = {};
		};

	}
}