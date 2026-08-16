/*
    bbv_surface_brick_pool_clear_cs.hlsl
    Frame-local SurfaceBrickPoolのState Gridとヘッダを初期化する。
*/

#define SURFACE_BRICK_POOL_CLEAR_THREAD_GROUP_SIZE 256

#include "../instant_rdv_util.hlsli"

RWBuffer<uint> RWBbvSurfaceBrickPoolState;
RWBuffer<uint> RWBbvSurfaceBrickPool;
RWBuffer<uint> RWBbvSurfaceBrickPoolCandidate;
RWBuffer<uint> RWBbvSurfaceBrickPoolDebug;

[numthreads(SURFACE_BRICK_POOL_CLEAR_THREAD_GROUP_SIZE, 1, 1)]
void main_cs(uint3 dtid : SV_DispatchThreadID)
{
    // Packed generation prevents a per-frame State Grid clear. Clear only at
    // the 16-bit generation wrap boundary (and on the first frame).
    const bool clear_state_grid =
        (cb_instant_rdv.frame_count & 0xffffu) == 0u ||
        cb_instant_rdv.assp_dummy_padding1.x != 0;
    uint state_element_count = 0u;
    RWBbvSurfaceBrickPoolState.GetDimensions(state_element_count);
    if(clear_state_grid && dtid.x < state_element_count)
    {
        RWBbvSurfaceBrickPoolState[dtid.x] = 0u;
    }

    uint pool_element_count = 0u;
    RWBbvSurfaceBrickPool.GetDimensions(pool_element_count);
    if(dtid.x < pool_element_count)
    {
        RWBbvSurfaceBrickPool[dtid.x] = 0u;
    }

    if(dtid.x == 0u)
    {
        RWBbvSurfaceBrickPoolCandidate[0] = 0u;
        RWBbvSurfaceBrickPoolDebug[0] = 0u;
        RWBbvSurfaceBrickPoolDebug[1] = 0u;
    }
}
