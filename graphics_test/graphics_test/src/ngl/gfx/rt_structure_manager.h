#pragma once

// -------------------
#include <cstring>
// for wchar convert.
#include <stdlib.h>
#include <codecvt>
#include <locale>
// --------------------

#include "ngl/math/math.h"

#include "ngl/rhi/d3d12/rhi.d3d12.h"
#include "ngl/rhi/d3d12/rhi_command_list.d3d12.h"
#include "ngl/rhi/d3d12/rhi_resource.d3d12.h"
#include "ngl/rhi/d3d12/rhi_resource_view.d3d12.h"

#include "mesh_resource.h"

namespace ngl
{
	namespace gfx
	{
		// ASはシステムから固定のレジスタへ設定する.
		static constexpr uint32_t k_system_raytracing_structure_srv_register = 65535;

		// Local Root Signature のレジスタは 1000 スタートとする.
		static constexpr uint32_t k_system_raytracing_local_register_start = 1000;

		// Global Root Signature の各リソース数.
		static constexpr uint32_t k_rt_global_descriptor_cbvsrvuav_table_size = 16;
		static constexpr uint32_t k_rt_global_descriptor_sampler_table_size = 16;

		// Local Root Signature の各リソース数.
		static constexpr uint32_t k_rt_local_descriptor_cbvsrvuav_table_size = 16;



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



		struct RaytraceStructureBottomGeometryResource
		{
			const rhi::ShaderResourceViewDep* vertex_srv = nullptr;
			const rhi::ShaderResourceViewDep* index_srv = nullptr;
		};


		struct RaytraceStructureBottomGeometryDesc
		{
			const gfx::MeshShapePart* mesh_data = nullptr;
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
			bool Setup(rhi::DeviceDep* p_device, const std::vector<RaytraceStructureBottomGeometryDesc>& geometry_desc_array);

			// SetupAs... の情報を元に構造構築コマンドを発行する.
			// Buildタイミングをコントロールするために分離している.
			// TODO RenderDocでのLaunchはクラッシュするのでNsight推奨.
			bool Build(rhi::DeviceDep* p_device, rhi::GraphicsCommandListDep* p_command_list);

			bool IsSetuped() const;
			bool IsBuilt() const;

			// BLAS Buffer.
			rhi::BufferDep* GetBuffer();
			// BLAS Buffer.
			const rhi::BufferDep* GetBuffer() const;

			// 内部Geometry数.
			uint32_t NumGeometry() const
			{
				return static_cast<uint32_t>(geom_desc_array_.size());
			}
			// 内部Geometry情報.
			RaytraceStructureBottomGeometryResource GetGeometryData(uint32_t index);
			// 内部Geometry情報.
			RaytraceStructureBottomGeometryResource GetGeometryData(uint32_t index) const;

		private:
			bool is_built_ = false;

			// リソースとして頂点バッファやインデックスバッファへアクセスをするために保持.
			std::vector<RaytraceStructureBottomGeometryDesc> geometry_desc_array_;

			// setup data.
			SETUP_TYPE		setup_type_ = SETUP_TYPE::NONE;
			
			// build info.
			// Setupでバッファや設定を登録される. これを用いてRenderThreadでCommandListにビルドタスクを発行する.
			std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geom_desc_array_ = {};
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
			bool Setup(rhi::DeviceDep* p_device, std::vector<RaytraceStructureBottom*>& blas_array,
				const std::vector<uint32_t>& instance_geom_id_array,
				const std::vector<math::Mat34>& instance_transform_array,
				const std::vector<uint32_t>& instance_hitgroup_id_array
			);

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


			uint32_t NumInstance() const;
			const std::vector<uint32_t>& GetInstanceBlasIndexArray() const;
			const std::vector<math::Mat34>& GetInstanceTransformArray() const;
			const std::vector<uint32_t>& GetInstanceHitgroupIndexArray() const;


			uint32_t NumBlas() const;
			const std::vector<RaytraceStructureBottom*>& GetBlasArray() const;

		private:
			bool is_built_ = false;

			// setup data.
			SETUP_TYPE		setup_type_ = SETUP_TYPE::NONE;

			std::vector<RaytraceStructureBottom*> blas_array_ = {};
			std::vector<uint32_t> instance_blas_id_array_;
			std::vector<math::Mat34> transform_array_;
			// Instance毎のHitgroupID.
			// 現状はBLAS内のGeometryはすべて同じHitgroupとしているが後で異なるものを設定できるようにしたい.
			std::vector<uint32_t> hitgroup_id_array_;

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


		// RaytraceStateObjectの初期化用シェーダ情報.
		struct RaytraceShaderRegisterInfo
		{
			struct HitgroupInfo
			{
				std::string hitgorup_name = {};
				std::string any_hit_name = {};
				std::string closest_hit_name = {};
				std::string intersection_name = {};
			};

			// 登録シェーダ.
			const rhi::ShaderDep* p_shader_library = nullptr;

			// 登録シェーダから公開するRayGenerationShader名配列.
			std::vector<std::string> ray_generation_shader_array = {};
			// 登録シェーダから公開するMissShader名配列.
			std::vector<std::string> miss_shader_array = {};

			// ShaderLib内でのHitgroup定義配列.
			//	 hitgroupを構成する関数は一つのShaderLib内に含まれているものとする (別のShaderLibからIntersectionだけを利用するといったことはしない).
			std::vector<HitgroupInfo> hitgroup_array = {};
		};

		// RaytracingのStateObject生成と管理.
		// マテリアルシェーダが動的に追加されるような状況では作り直しになるが, ShaderTable側を一旦デフォルトに割り当てて数フレーム後に作り直したStateで置き換える等を検討中.
		class RaytraceStateObject
		{
		public:
			RaytraceStateObject() {}
			~RaytraceStateObject() {}

			bool Initialize(rhi::DeviceDep* p_device,
				const std::vector<RaytraceShaderRegisterInfo>& shader_info_array, 
				uint32_t payload_byte_size = sizeof(float) * 4, uint32_t attribute_byte_size = sizeof(float) * 2, uint32_t max_trace_recursion = 1);

			CComPtr<ID3D12StateObject> GetStateObject() const
			{
				return state_oject_;
			}
			CComPtr<ID3D12RootSignature> GetGlobalRootSignature() const
			{
				return global_root_signature_;
			}

			const char* GetHitgroupName(uint32_t hitgroup_id) const 
			{
				assert(hitgroup_database_.size() > hitgroup_id);
				return hitgroup_database_[hitgroup_id].hitgorup_name.c_str();
			}

		private:
			bool initialized_ = false;

			// 内部管理用.
			struct RayGenerationInfo
			{
				int  shader_index = -1;// 登録元のShaderRef.
				
				std::string ray_generation_name = {};
			};
			// 内部管理用.
			struct MissInfo
			{
				int  shader_index = -1;// 登録元のShaderRef.

				std::string miss_name = {};
			};
			// 内部管理用.
			struct HitgroupInfo
			{
				int  shader_index = -1;// 登録元のShaderRef.

				std::string hitgorup_name = {};

				std::string any_hit_name = {};
				std::string closest_hit_name = {};
				std::string intersection_name = {};
			};

			std::vector<const rhi::ShaderDep*>	shader_database_ = {};
			std::vector<RayGenerationInfo>	raygen_database_ = {};
			std::vector<MissInfo>			miss_database_ = {};
			std::vector<HitgroupInfo>		hitgroup_database_ = {};


			uint32_t						payload_byte_size_ = sizeof(float)*4;
			uint32_t						attribute_byte_size_ = sizeof(float) * 2;
			uint32_t						max_trace_recursion_ = 1;

			CComPtr<ID3D12RootSignature>	global_root_signature_ = {};
			CComPtr<ID3D12RootSignature>	local_root_signature_fixed_ = {};
			CComPtr<ID3D12StateObject>		state_oject_ = {};
		};


		class RaytraceShaderTable
		{
		public:
			RaytraceShaderTable() {}
			~RaytraceShaderTable() {}


			rhi::BufferDep	shader_table_;

			uint32_t		table_entry_byte_size_ = 0;

			uint32_t		table_raygen_offset_ = 0;
			uint32_t		table_miss_offset_ = 0;
			uint32_t		table_miss_count_ = 0;
			uint32_t		table_hitgroup_offset_ = 0;
			uint32_t		table_hitgroup_count_ = 0;
		};
		static bool CreateShaderTable(
			RaytraceShaderTable& out,
			rhi::DeviceDep* p_device,
			rhi::FrameDescriptorAllocInterface& desc_alloc_interface, uint32_t desc_alloc_id,
			const RaytraceStructureTop& tlas, const RaytraceStateObject& state_object, const char* raygen_name, const char* miss_name);


		// RaytraceSceneに登録する1Modelを構成するGeometry情報.
		struct RaytraceBlasInstanceGeometryDesc
		{
			// このModelを構成するGeometry情報. Submesh分のVertexBufferやIndexBuffer.
			RaytraceStructureBottomGeometryDesc*	pp_desc = nullptr;
			uint32_t								num_desc = 0;
		};

		// RaytracingのAS管理.
		class RaytraceStructureManager
		{
		public:
			RaytraceStructureManager();
			~RaytraceStructureManager();

			bool Initialize(rhi::DeviceDep* p_device, 
				RaytraceStateObject* p_state, 
				const std::vector<RaytraceBlasInstanceGeometryDesc>& geom_array,
				const std::vector<uint32_t>& instance_geom_id_array,
				const std::vector<math::Mat34>& instance_transform_array,
				const std::vector<uint32_t>& instance_hitgroup_id_array
				);

			void UpdateOnRender(rhi::DeviceDep* p_device, rhi::GraphicsCommandListDep* p_command_list);
			void DispatchRay(rhi::GraphicsCommandListDep* p_command_list);


			void SetCameraInfo(const math::Vec3& position, const math::Vec3& dir, const math::Vec3& up);

			const rhi::ShaderResourceViewDep* GetResultSrv() const
			{
				return &ray_result_srv_;
			}

		private:
			uint32_t frame_count_ = 0;

			// BLAS.
			// 管理責任はRaytraceStructureManager自身.
			std::vector<RaytraceStructureBottom*>	blas_array_ = {};

			// TLAS.
			RaytraceStructureTop test_tlas_;


			rhi::BufferDep				cb_test_scene_view[2];
			rhi::ConstantBufferViewDep	cbv_test_scene_view[2];


			// テスト用StateObject.
			// 管理責任は外部.
			RaytraceStateObject* p_state_object_ = {};

			// テスト用ShaderTable.
			// 管理責任はRaytraceStructureManager自身.
			RaytraceShaderTable shader_table_;

			// Descriptor用. 別のRaytraceStructureManagerインスタンスのAllocIDと被ると意図しないタイミングで解放されることがあるので注意.
			rhi::FrameDescriptorAllocInterface	desc_alloc_interface_ = {};
			uint32_t	desc_alloc_id_ = rhi::FrameDescriptorManager::k_invalid_alloc_group_id;

			// テスト用のRayDispatch出力先UAV.
			rhi::TextureDep				ray_result_;
			rhi::ShaderResourceViewDep	ray_result_srv_;
			rhi::UnorderedAccessViewDep	ray_result_uav_;
			rhi::ResourceState			ray_result_state_ = {};


			math::Vec3 camera_pos_ = {};
			math::Vec3 camera_dir_ = {};
			math::Vec3 camera_up_ = {};
		};

	}
}


namespace
{
	// Convert a wide Unicode string to an UTF8 string
	// DXRのShaderLibrary内エントリアクセスがwcharのためUtilityとして用意.
	std::string wst_to_str(const std::wstring& wstr)
	{
		if (wstr.empty()) return std::string();
		int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
		std::string strTo(size_needed, 0);
		WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
		return strTo;
	}

	// Convert an UTF8 string to a wide Unicode String
	// DXRのShaderLibrary内エントリアクセスがwcharのためUtilityとして用意.
	std::wstring str_to_wstr(const std::string& str)
	{
		if (str.empty()) return std::wstring();
		int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
		std::wstring wstrTo(size_needed, 0);
		MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
		return wstrTo;
	}
}


