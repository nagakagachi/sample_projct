/*
    bbv_surface_brick_pool_allocate_cs.hlsl
    Tile CandidateのBrickごとにGlobal SurfaceBrickPool slotを一度だけ割り当てる。
*/

#include "../instant_rdv_util.hlsli"

Buffer<uint> BbvSurfaceBrickPoolCandidateBuffer;
RWBuffer<uint> RWBbvSurfaceBrickPoolState;
RWBuffer<uint> RWBbvSurfaceBrickPool;
RWBuffer<uint> RWBbvSurfaceBrickPoolDebug;

void AllocateSurfaceBrick(uint brick_index)
{
    const uint generation = max(1u, cb_instant_rdv.frame_count & 0xffffu);
    const uint observed_state = RWBbvSurfaceBrickPoolState[brick_index];
    if((observed_state >> 16u) == generation)
    {
        return;
    }

    const uint reserved_state =
        (generation << 16u) | k_bbv_surface_brick_pool_state_reserved_index;
    uint cas_old_value = 0u;
    InterlockedCompareExchange(
        RWBbvSurfaceBrickPoolState[brick_index],
        observed_state,
        reserved_state,
        cas_old_value);
    if(cas_old_value != observed_state)
    {
        uint ignored_previous = 0u;
        InterlockedAdd(RWBbvSurfaceBrickPoolDebug[1], 1u, ignored_previous);
        return;
    }

    uint previous_count = 0u;
    InterlockedAdd(RWBbvSurfaceBrickPool[0], 1u, previous_count);
    const uint pool_index = previous_count + 1u;
    if(pool_index > (uint)k_bbv_surface_brick_pool_capacity)
    {
        uint ignored_previous = 0u;
        InterlockedAdd(RWBbvSurfaceBrickPoolDebug[0], 1u, ignored_previous);
        InterlockedExchange(
            RWBbvSurfaceBrickPoolState[brick_index],
            (generation << 16u) |
                k_bbv_surface_brick_pool_state_overflow_index,
            ignored_previous);
        return;
    }

    const uint entry_address = 1u +
        (pool_index - 1u) *
        (uint)k_bbv_surface_brick_pool_entry_u32_count;
    RWBbvSurfaceBrickPool[entry_address] = brick_index;

    DeviceMemoryBarrier();
    uint ignored_previous = 0u;
    InterlockedExchange(
        RWBbvSurfaceBrickPoolState[brick_index],
        (generation << 16u) | pool_index,
        ignored_previous);
}

[numthreads(64, 1, 1)]
void main_cs(uint3 dtid : SV_DispatchThreadID)
{
    uint candidate_element_count = 0u;
    BbvSurfaceBrickPoolCandidateBuffer.GetDimensions(candidate_element_count);
    const uint candidate_capacity = (candidate_element_count - 1u) >> 1u;
    const uint candidate_count = min(
        BbvSurfaceBrickPoolCandidateBuffer[0],
        candidate_capacity);

    if(dtid.x >= candidate_count)
    {
        return;
    }
    const uint packed_key =
        BbvSurfaceBrickPoolCandidateBuffer[1u + dtid.x * 2u];
    const uint brick_index = packed_key >> 4u;
    if(brick_index >= bbv_brick_count())
    {
        return;
    }
    AllocateSurfaceBrick(brick_index);
}
