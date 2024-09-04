#include "imgui_util.h"

#if NGL_IMGUI_ENABLE
// imgui.
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

#include "ngl/rhi/d3d12/command_list.d3d12.h"
#include "ngl/rhi/d3d12/resource_view.d3d12.h"

namespace  ngl::imgui
{
    bool ImguiInterface::Initialize(rhi::DeviceDep* p_device, rhi::SwapChainDep* p_swapchain)
    {
#if NGL_IMGUI_ENABLE
        HWND hwnd = p_device->GetWindow()->Dep().GetWindowHandle();
        auto* p_d3d12_device = p_device->GetD3D12Device();
        int num_swapchain_buffer = p_swapchain->NumResource();
        auto rt_d3d_format = rhi::ConvertResourceFormat(p_swapchain->GetDesc().format);
        // imguiがSRVのDescriptorHeapを要求するため, Heap単位でページ的に確保できるFrameDescriptorHeapPageを利用.
        rhi::FrameDescriptorHeapPageInterface::Desc fdhpi_desc{};
        {
            fdhpi_desc.type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        }
        if(!descriptor_heap_interface_.Initialize(p_device->GetFrameDescriptorHeapPagePool(), fdhpi_desc))
        {
            assert(false);
            return false;
        }
        D3D12_CPU_DESCRIPTOR_HANDLE d3d_desc_handle_cpu{};
        D3D12_GPU_DESCRIPTOR_HANDLE d3d_desc_handle_gpu{};
        descriptor_heap_interface_.Allocate(1, d3d_desc_handle_cpu, d3d_desc_handle_gpu);// 一つ確保してPageを取得させる.
        ID3D12DescriptorHeap* d3d_desc_heap = descriptor_heap_interface_.GetD3D12DescriptorHeap();// ページのHeap.
        
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

        // Setup Platform/Renderer backends
        ImGui_ImplWin32_Init(hwnd);
        ImGui_ImplDX12_Init(p_d3d12_device, num_swapchain_buffer, rt_d3d_format,
            d3d_desc_heap,
            // You'll need to designate a descriptor from your descriptor heap for Dear ImGui to use internally for its font texture's SRV
            d3d_desc_handle_cpu,
            d3d_desc_handle_gpu
         );
        
        initialized_ = true;
#endif
        return true;
    }
    void ImguiInterface::Finalize()
    {
#if NGL_IMGUI_ENABLE
        if(!initialized_)
        {
            return;
        }
        
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        descriptor_heap_interface_.Finalize();

        initialized_ = false;
#endif
    }

    bool ImguiInterface::BeginFrame()
    {
#if NGL_IMGUI_ENABLE
        if(!initialized_)
        {
            return false;
        }
        
        // (Your code process and dispatch Win32 messages)
        // Start the Dear ImGui frame
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGui::ShowDemoWindow(); // Show demo window! :)
#endif
        return true;
    }

    bool ImguiInterface::Render(rhi::GraphicsCommandListDep* p_command_list, rhi::SwapChainDep* p_swapchain, uint32_t swapchain_index, rhi::RenderTargetViewDep* p_swapchain_rtv, rhi::EResourceState rtv_state_prev, rhi::EResourceState rtv_state_next)
    {
#if NGL_IMGUI_ENABLE
        if(!initialized_)
        {
            return false;
        }
        
        ImGui::Render();
        
        ID3D12DescriptorHeap* d3d_desc_heap = descriptor_heap_interface_.GetD3D12DescriptorHeap();// ページのHeap.
        D3D12_CPU_DESCRIPTOR_HANDLE rtv_desc_handle_cpu =  p_swapchain_rtv->GetD3D12DescriptorHandle();
        ID3D12GraphicsCommandList* d3d_command_list = p_command_list->GetD3D12GraphicsCommandList();

        constexpr rhi::EResourceState k_rtv_target_state = rhi::EResourceState::RenderTarget;
        // rtvのステートを RenderTargetへ.
        p_command_list->ResourceBarrier(p_swapchain, swapchain_index, rtv_state_prev, k_rtv_target_state);

        // RTV設定.
        d3d_command_list->OMSetRenderTargets(1, &rtv_desc_handle_cpu, FALSE, nullptr);
        d3d_command_list->SetDescriptorHeaps(1, &d3d_desc_heap);

        // Imguiレンダリング.
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), p_command_list->GetD3D12GraphicsCommandList());
        
        // rtvのステートを nextへ.
        p_command_list->ResourceBarrier(p_swapchain, swapchain_index, k_rtv_target_state, rtv_state_next);
#else
        
        // imgui無効時.
        // ステート遷移のみ実行.
        p_command_list->ResourceBarrier(p_swapchain, swapchain_index, rtv_state_prev, rtv_state_next);
        
#endif
        return true;
    }

    bool ImguiInterface::WindProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
#if NGL_IMGUI_ENABLE
        if(!initialized_)
        {
            return false;
        }
        
        return ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
#else
        return false;
#endif
    }


}
