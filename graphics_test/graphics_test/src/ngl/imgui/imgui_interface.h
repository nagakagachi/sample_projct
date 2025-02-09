#pragma once

// ImGuiの有効化.
#define NGL_IMGUI_ENABLE 1


#include "ngl/util/singleton.h"
#include "ngl/rhi/d3d12/device.d3d12.h"

#include "ngl/gfx/rtg/graph_builder.h"

namespace ngl::imgui
{
    
    void AppendImguiRenderTask(rtg::RenderTaskGraphBuilder& builder, rtg::RtgResourceHandle h_swapchain);
    
    class ImguiInterface : public Singleton<ImguiInterface>
    {
    public:

        bool Initialize(rhi::DeviceDep* p_device, rhi::SwapChainDep* p_swapchain);
        void Finalize();

        // フレーム開始時.
        bool BeginFrame();
        void EndFrame();

        // レンダリング.
        //  Swapchainへのレンダリングを想定.
        bool Render(rhi::GraphicsCommandListDep* p_command_list,
            rhi::SwapChainDep* p_swapchain, uint32_t swapchain_index, rhi::RenderTargetViewDep* p_swapchain_rtv, rhi::EResourceState rtv_state_prev, rhi::EResourceState rtv_state_next);
        
        bool WindProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    private:
        bool initialized_ = false;
        rhi::FrameDescriptorHeapPageInterface descriptor_heap_interface_{};
        
    };
}