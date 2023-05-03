#pragma once

#include <unordered_map>

#include "ngl/text/hash_text.h"

#include "ngl/rhi/d3d12/device.d3d12.h"
#include "ngl/rhi/d3d12/shader.d3d12.h"
#include "ngl/rhi/d3d12/command_list.d3d12.h"
#include "ngl/rhi/d3d12/resource_view.d3d12.h"

#include "ngl/resource/resource_manager.h"
#include "ngl/gfx/resource/resource_shader.h"

namespace ngl::render
{
	using GraphResouceNameText = text::HashCharPtr<64>;

	// 簡易リソース登録クラス.
	//	将来的にはRenderGraphの構築とリソース割り当て, 自動ステート遷移などをしたい.
	//	現状はメイン側で必要なリソースを全部登録してPass側で直接引いている.
	class GraphBuilder
	{

	public:
		GraphBuilder(rhi::DeviceDep* p_device)
		{
			p_device_ = p_device;
		}

		void AddFrameResource(const GraphResouceNameText& name, rhi::RefCbvDep v)
		{
			frame_cbv_map_[name] = v;
		}
		void AddFrameResource(const GraphResouceNameText& name, rhi::RefSrvDep v)
		{
			frame_srv_map_[name] = v;
		}
		void AddFrameResource(const GraphResouceNameText& name, rhi::RefUavDep v)
		{
			frame_uav_map_[name] = v;
		}
		void AddFrameResource(const GraphResouceNameText& name, rhi::RefSampDep v)
		{
			frame_samp_map_[name] = v;
		}

		void AddFrameResource(const GraphResouceNameText& name, rhi::RefTextureDep v)
		{
			frame_tex_map_[name] = v;
		}


		rhi::RefCbvDep GetFrameCbv(const GraphResouceNameText& name)
		{
			if (frame_cbv_map_.end() == frame_cbv_map_.find(name))
				return {};
			return frame_cbv_map_[name];
		}
		rhi::RefSrvDep GetFrameSrv(const GraphResouceNameText& name)
		{
			if (frame_srv_map_.end() == frame_srv_map_.find(name))
				return {};
			return frame_srv_map_[name];
		}
		rhi::RefUavDep GetFrameUav(const GraphResouceNameText& name)
		{
			if (frame_uav_map_.end() == frame_uav_map_.find(name))
				return {};
			return frame_uav_map_[name];
		}
		rhi::RefSampDep GetFrameSamp(const GraphResouceNameText& name)
		{
			if (frame_samp_map_.end() == frame_samp_map_.find(name))
				return {};
			return frame_samp_map_[name];
		}

		rhi::RefTextureDep GetFrameTexture(const GraphResouceNameText& name)
		{
			if (frame_tex_map_.end() == frame_tex_map_.find(name))
				return {};
			return frame_tex_map_[name];
		}

	public:
		rhi::DeviceDep* GetDevice() { return p_device_; }
	private:
		rhi::DeviceDep* p_device_ = {};

		std::unordered_map<GraphResouceNameText, rhi::RefCbvDep>	frame_cbv_map_;
		std::unordered_map<GraphResouceNameText, rhi::RefSrvDep>	frame_srv_map_;
		std::unordered_map<GraphResouceNameText, rhi::RefUavDep>	frame_uav_map_;
		std::unordered_map<GraphResouceNameText, rhi::RefSampDep>	frame_samp_map_;

		std::unordered_map<GraphResouceNameText, rhi::RefTextureDep>	frame_tex_map_;


	};

}