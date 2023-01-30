#pragma once


#include "ngl/math/math.h"

namespace ngl
{
namespace rhi
{
    class GraphicsCommandListDep;
}

namespace gfx
{
    namespace helper
    {
        void SetFullscreenViewportAndScissor(rhi::GraphicsCommandListDep* p_command_list, int width, int height);

    }
}
}
