﻿#pragma once


#include "ngl/rhi/rhi.h"
#include "ngl/rhi/rhi_ref.h"
#include "ngl/rhi/rhi_object_garbage_collect.h"

#include "rhi_util.d3d12.h"

namespace ngl
{
namespace rhi
{
	class DeviceDep;
	class GraphicsCommandListDep;
	class DescriptorSetDep;


	// D3D12では単純にシェーダバイナリを保持するだけ
	class ShaderDep
	{
	public:
		ShaderDep();
		virtual ~ShaderDep();

		// コンパイル済みシェーダバイナリから初期化
		bool Initialize(DeviceDep* p_device, ShaderStage stage, const void* shader_binary_ptr, u32 shader_binary_size);

		struct InitFileDesc
		{
			// シェーダファイルパス.
			// "./sample.hlsl"
			const char* shader_file_path = nullptr;
			// エントリポイント名. 
			// "main_ps"
			const char* entry_point_name = nullptr;
			// シェーダステージ.
			ShaderStage		stage = ShaderStage::Vertex;
			// シェーダモデル文字列.
			// "4_0", "5_0", "5_1" etc.
			const char* shader_model_version = nullptr;

			// コンパイルオプション設定.
			bool			option_debug_mode = false;
			bool			option_enable_validation = false;
			bool			option_enable_optimization = false;
			bool			option_matrix_row_major = false;

			// TODO. インクルードやDefineオプションなど

		};
		// ファイルから
		bool Initialize(DeviceDep* p_device, const InitFileDesc& desc);
		void Finalize();

		u32		GetShaderBinarySize() const;
		const void* GetShaderBinaryPtr() const;
		ShaderStage GetShaderStageType() const;
	private:
		ShaderStage stage_;
		std::vector<u8>	data_;
	};

	/*
		ShaderReflectionによって取得した情報

		note: 現時点ではDXCでコンパイルしたシェーダの定数バッファデフォルト値が取得できない(ShaderModel5以下の場合はD3DCompilerによるコンパイルをしてデフォルト値も取得できる)
	*/
	class ShaderReflectionDep
	{
	public:
		struct CbInfo
		{
			// Cb名
			char	name[64];
			// cb Slot Index
			u16		index;
			// cb Byte Size
			u16		size;
			u16		num_member;
		};
		struct CbVariableInfo
		{
			// CbVariable名
			char	name[64];
			// Constant Buffer内のByte Offset
			u16		offset_in_cb;
			// Constant Buffer内のByte Size
			u16		size;
			// デフォルト値ブロック内でのこのVarのデフォルト値オフセット.
			u16		default_value_offset;
		};

		struct InputParamInfo
		{
			//char	semantic_name[32];
			text::HashCharPtr<32>	semantic_name;

			u8		semantic_index;

			// if float3 then 3. if int4 then 4.
			u8		num_component;
		};

		struct ResourceSlotInfo
		{
			ResourceViewName			name;
			RootParameterType			type = RootParameterType::_Max;
			s32							bind_point = -1;

		};

		ShaderReflectionDep();
		~ShaderReflectionDep();

		bool Initialize(DeviceDep* p_device, const ShaderDep* p_shader);
		void Finalize();


		u32 NumInputParamInfo() const;
		const InputParamInfo* GetInputParamInfo(u32 index) const;

		u32 NumCbInfo() const;
		const CbInfo* GetCbInfo(u32 index) const;
		const CbVariableInfo* GetCbVariableInfo(u32 index, u32 variable_index) const;

		template<typename T>
		bool GetCbDefaultValue(u32 index, u32 variable_index, T& out) const
		{
			if (cb_.size() <= index || cb_[index].num_member <= variable_index)
				return false;
			const auto cb_var_offset = cb_variable_offset_[index] + variable_index;
			if (cb_variable_[cb_var_offset].size != sizeof(T))
				return false;

			const auto* src = &cb_default_value_buffer_[cb_variable_[cb_var_offset].default_value_offset];
			std::memcpy(&out, src, cb_variable_[cb_var_offset].size);
			return true;
		}

		u32 NumResourceSlotInfo() const;
		const ResourceSlotInfo* GetResourceSlotInfo(u32 index) const;

		// Computeの numthreads 情報.
		void GetComputeThreadGroupSize(u32& out_threadgroup_size_x, u32& out_threadgroup_size_y, u32& out_threadgroup_size_z) const;
	private:
		// Constant Buffer
		// cb
		std::vector<CbInfo> cb_;
		// cb index to start index on variable_.
		std::vector<u32> cb_variable_offset_;
		// variable array.
		std::vector<CbVariableInfo> cb_variable_;
		// デフォルト値バッファ.
		std::vector<u8> cb_default_value_buffer_;

		// Input Output
		std::vector<InputParamInfo>	input_param_;

		std::vector<ResourceSlotInfo>	resource_slot_;

		// For Compute
		u32	threadgroup_size_x = 0;
		u32	threadgroup_size_y = 0;
		u32	threadgroup_size_z = 0;
	};

	/*
		ResourceViewとレジスタのマッピング. このオブジェクトを利用してリソース名からDescriptorSetの対応するスロットへのDescriptor設定をする.
		実質RootSignature.
		このシステムで利用するシェーダリソースのレジスタ番号は以下のような制限とする.

			CBV		0-15
			SRV		0-47
			UAV		0-15
			Sampler	0-15
	*/
	class PipelineResourceViewLayoutDep
	{
	public:
		struct Desc
		{
			const ShaderDep* vs = nullptr;
			const ShaderDep* hs = nullptr;
			const ShaderDep* ds = nullptr;
			const ShaderDep* gs = nullptr;
			const ShaderDep* ps = nullptr;
			const ShaderDep* cs = nullptr;
		};

		// RootSignatureで定義されたあるシェーダステージのあるリソースタイプのテーブルインデックスを保持
		//	commandlist->SetGraphicsRootDescriptorTable での書き込み先テーブル番号となる.
		struct ResourceTable
		{
			// 何番目のDescriptorTableがVSのCBVテーブルか
			s8		vs_cbv_table		= -1;
			// 何番目のDescriptorTableがVSのSRVテーブルか
			s8		vs_srv_table		= -1;
			// 何番目のDescriptorTableがVSのSamplerテーブルか
			s8		vs_sampler_table	= -1;

			s8		hs_cbv_table		= -1;
			s8		hs_srv_table		= -1;
			s8		hs_sampler_table	= -1;

			s8		ds_cbv_table		= -1;
			s8		ds_srv_table		= -1;
			s8		ds_sampler_table	= -1;

			s8		gs_cbv_table		= -1;
			s8		gs_srv_table		= -1;
			s8		gs_sampler_table	= -1;

			// 何番目のDescriptorTableがPSのCBVテーブルか
			s8		ps_cbv_table		= -1;
			// 何番目のDescriptorTableがPSのSRVテーブルか
			s8		ps_srv_table		= -1;
			// 何番目のDescriptorTableがPSのSamplerテーブルか
			s8		ps_sampler_table	= -1;
			// 何番目のDescriptorTableがPSのUAVテーブルか
			s8		ps_uav_table		= -1;

			s8		cs_cbv_table		= -1;
			s8		cs_srv_table		= -1;
			s8		cs_sampler_table	= -1;
			s8		cs_uav_table		= -1;

		};	// struct InputIndex


		struct ShaderStageSlot
		{
			RootParameterType	type = RootParameterType::_Max;
			s16					slot = -1;
		};
		struct Slot
		{
			ShaderStageSlot vs_stage = {};
			ShaderStageSlot hs_stage = {};
			ShaderStageSlot ds_stage = {};
			ShaderStageSlot gs_stage = {};
			ShaderStageSlot ps_stage = {};
			ShaderStageSlot cs_stage = {};
		};

		PipelineResourceViewLayoutDep();
		~PipelineResourceViewLayoutDep();

		bool Initialize(DeviceDep* p_device, const Desc& desc);
		void Finalize();

		const ResourceTable& GetResourceTable() const
		{
			return resource_table_;
		}

		// 名前でDescriptorSetへハンドル設定
		void SetDescriptorHandle(DescriptorSetDep* p_desc_set, const char* name, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle) const;

		ID3D12RootSignature* GetD3D12RootSignature();
		const ID3D12RootSignature* GetD3D12RootSignature() const;

	private:
		CComPtr<ID3D12RootSignature>	root_signature_;

		// 頂点シェーダリフレクションは入力レイアウト情報などのために保持する
		ShaderReflectionDep		vs_reflection_;

		// リソース名とスロット(register)の対応情報map.
		std::unordered_map<ResourceViewName, Slot> slot_map_;

		// CommandListにDescriptorをセットする際の各シェーダステージ/リソースタイプの対応するRootTableインデックス.
		ResourceTable					resource_table_;
	};

	// パイプラインステート Graphics.
	class GraphicsPipelineStateDep : public RhiObjectBase
	{
	public:
		struct Desc
		{
			const ShaderDep* vs = nullptr;
			const ShaderDep* ps = nullptr;
			const ShaderDep* ds = nullptr;
			const ShaderDep* hs = nullptr;
			const ShaderDep* gs = nullptr;

			BlendState			blend_state = {};
			u32					sample_mask = ~(u32(0));
			RasterizerState		rasterizer_state = {};
			DepthStencilState	depth_stencil_state = {};

			InputLayout				input_layout = {};
			PrimitiveTopologyType	primitive_topology_type = PrimitiveTopologyType::Triangle;

			u32					num_render_targets = 0;
			ResourceFormat		render_target_formats[8] = {};
			ResourceFormat		depth_stencil_format = ResourceFormat::Format_D24_UNORM_S8_UINT;
			SampleDesc			sample_desc = {};
			u32					node_mask = 0;
		};


		GraphicsPipelineStateDep();
		~GraphicsPipelineStateDep();

		// PSO生成後なら使用したShaderのByteCodeのメモリは破棄しても問題ない模様(DirectX12).
		bool Initialize(DeviceDep* p_device, const Desc& desc);
		void Finalize();

		const PipelineResourceViewLayoutDep* GetPipelineResourceViewLayout() const
		{
			return &view_layout_;
		}

		// 名前でDescriptorSetへハンドル設定
		void SetDescriptorHandle(DescriptorSetDep* p_desc_set, const char* name, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle) const;

	public:
		ID3D12PipelineState* GetD3D12PipelineState();
		ID3D12RootSignature* GetD3D12RootSignature();
		const ID3D12PipelineState* GetD3D12PipelineState() const;
		const ID3D12RootSignature* GetD3D12RootSignature() const;
	private:
		CComPtr<ID3D12PipelineState>			pso_;
		PipelineResourceViewLayoutDep			view_layout_;
	};


	// パイプラインステート Compute.
	class ComputePipelineStateDep : public RhiObjectBase
	{
	public:
		struct Desc
		{
			const ShaderDep* cs = nullptr;
			u32			node_mask = 0;
		};


		ComputePipelineStateDep();
		~ComputePipelineStateDep();

		// PSO生成後なら使用したShaderのByteCodeのメモリは破棄しても問題ない模様(DirectX12).
		bool Initialize(DeviceDep* p_device, const Desc& desc);
		void Finalize();

		const PipelineResourceViewLayoutDep* GetPipelineResourceViewLayout() const
		{
			return &view_layout_;
		}

		// 名前でDescriptorSetへハンドル設定
		void SetDescriptorHandle(DescriptorSetDep* p_desc_set, const char* name, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle) const;

		// Dispatch Helper. 総ThreadCountから自動でGroupCountを計算してDispatchする.
		void DispatchHelper(GraphicsCommandListDep* p_command_list, u32 thread_count_x, u32 thread_count_y, u32 thread_count_z) const;

		u32 GetThreadGroupSizeX() const { return threadgroup_size_x_; }
		u32 GetThreadGroupSizeY() const { return threadgroup_size_y_; }
		u32 GetThreadGroupSizeZ() const { return threadgroup_size_z_; }
	public:
		ID3D12PipelineState* GetD3D12PipelineState();
		ID3D12RootSignature* GetD3D12RootSignature();
		const ID3D12PipelineState* GetD3D12PipelineState() const;
		const ID3D12RootSignature* GetD3D12RootSignature() const;
	private:
		CComPtr<ID3D12PipelineState>			pso_;
		PipelineResourceViewLayoutDep			view_layout_;

		u32										threadgroup_size_x_ = 0;
		u32										threadgroup_size_y_ = 0;
		u32										threadgroup_size_z_ = 0;
	};
}
}