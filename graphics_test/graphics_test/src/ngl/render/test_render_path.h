#pragma once


#include "ngl/gfx/rtg/graph_builder.h"
#include "ngl/gfx/command_helper.h"


namespace ngl::gfx
{
	class SceneRepresentation;
	class RtSceneManager;
}

namespace ngl::test
{

    struct RenderFrameDesc
    {
        // Viewのカメラ情報.
        ngl::math::Vec3		camera_pos = {};
        ngl::math::Mat33	camera_pose = ngl::math::Mat33::Identity();
        float				camera_fov_y = ngl::math::Deg2Rad(60.0f);
			
        ngl::rhi::DeviceDep* p_device = {};

        // 描画解像度.
        ngl::u32 screen_w = 0;
        ngl::u32 screen_h = 0;

        // 外部リソースとしてSwapchain情報.
        ngl::rhi::RhiRef<ngl::rhi::SwapChainDep> ref_swapchain = {};
        ngl::rhi::RefRtvDep		ref_swapchain_rtv = {};
        ngl::rhi::EResourceState	swapchain_state_prev = {};
        ngl::rhi::EResourceState	swapchain_state_next = {};
    	
        const ngl::gfx::SceneRepresentation* p_scene = {};

    	// RaytraceScene.
    	gfx::RtSceneManager* p_rt_scene = {};

        ngl::rhi::RefSrvDep ref_test_tex_srv = {};

    	// 前フレームでの結果ヒストリ.
        ngl::rtg::RtgResourceHandle	h_prev_lit = {};
    	// 先行する別のrtgの出力をPropagateして使うテスト.
    	ngl::rtg::RtgResourceHandle	h_other_graph_out_tex = {};
    };
	
    struct RenderFrameOut
    {
        ngl::rtg::RtgResourceHandle h_propagate_lit = {};
    };
	
    
	auto TestFrameRenderingPath(
			const RenderFrameDesc& render_frame_desc,
			RenderFrameOut& out_frame_out,
			ngl::rtg::RenderTaskGraphManager& rtg_manager,
			std::vector<ngl::rtg::RtgSubmitCommandSequenceElem>& out_graphics_cmd,
			std::vector<ngl::rtg::RtgSubmitCommandSequenceElem>& out_compute_cmd
		) -> void;
    
}

