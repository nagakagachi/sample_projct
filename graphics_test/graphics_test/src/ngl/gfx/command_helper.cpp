#pragma once

#include "command_helper.h"

#include "ngl/math/math.h"

#include "ngl/rhi/d3d12/command_list.d3d12.h"

namespace ngl
{
namespace gfx
{
    namespace helper
    {
        void SetFullscreenViewportAndScissor(rhi::GraphicsCommandListDep* p_command_list, int width, int height)
        {
			D3D12_VIEWPORT viewport;
			viewport.MinDepth = 0.0f;
			viewport.MaxDepth = 1.0f;
			viewport.TopLeftX = 0.0f;
			viewport.TopLeftY = 0.0f;
			viewport.Width = static_cast<float>(width);
			viewport.Height = static_cast<float>(height);

			D3D12_RECT scissor_rect;
			scissor_rect.left = 0;
			scissor_rect.top = 0;
			scissor_rect.right = width;
			scissor_rect.bottom = height;

			p_command_list->SetViewports(1, &viewport);
			p_command_list->SetScissor(1, &scissor_rect);
        }
    } 
}
}
