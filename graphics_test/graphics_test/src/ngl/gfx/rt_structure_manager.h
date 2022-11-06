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
			CComPtr<ID3D12StateObject>		state_oject_ = {};
		};


		// RaytracingのAS管理.
		class RaytraceStructureManager
		{
		public:
			RaytraceStructureManager();
			~RaytraceStructureManager();

			bool Initialize(rhi::DeviceDep* p_device, RaytraceStateObject* p_state);
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



			// テスト用StateObject.
			RaytraceStateObject* p_state_object_ = {};

			rhi::BufferDep	rt_shader_table_;
			uint32_t		rt_shader_table_entry_byte_size_ = 0;
			// ShaderTable上のHitGroup領域の先頭へのオフセット.
			uint32_t		rt_shader_table_raygen_offset = 0;
			uint32_t		rt_shader_table_miss_offset = 0;
			uint32_t		rt_shader_table_hitgroup_offset = 0;


			// Raytrace用のCBV,SRV,UAV用Heap.
			// 更新や足りなくなったら作り直される予定.
			rhi::DescriptorHeapWrapper	rt_descriptor_heap_;


			// テスト用のRayDispatch出力先UAV.
			rhi::TextureDep				ray_result_;
			rhi::ShaderResourceViewDep	ray_result_srv_;
			rhi::UnorderedAccessView	ray_result_uav_;
			rhi::ResourceState			ray_result_state_ = {};
		};

	}
}