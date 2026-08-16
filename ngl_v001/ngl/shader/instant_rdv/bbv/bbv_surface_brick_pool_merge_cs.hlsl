/*
    bbv_surface_brick_pool_merge_cs.hlsl
    Tile Candidate maskをAllocate済みGlobal SurfaceBrickPoolへマージする。
*/

#include "../instant_rdv_util.hlsli"

Buffer<uint> BbvSurfaceBrickPoolCandidateBuffer;
Buffer<uint> BbvSurfaceBrickPoolStateBuffer;
RWBuffer<uint> RWBbvSurfaceBrickPool;

void MergeSurfaceCandidate(uint packed_key, uint merged_mask)
{
    const uint brick_index = packed_key >> 4u;
    const uint mask_word = packed_key & 15u;
    if(brick_index >= bbv_brick_count() ||
       mask_word >= (uint)k_bbv_surface_brick_pool_mask_word_count)
    {
        return;
    }

    const uint generation = max(1u, cb_instant_rdv.frame_count & 0xffffu);
    const uint packed_state = BbvSurfaceBrickPoolStateBuffer[brick_index];
    const uint pool_index = packed_state & 0xffffu;
    if((packed_state >> 16u) != generation ||
       pool_index == 0u ||
       pool_index > (uint)k_bbv_surface_brick_pool_capacity)
    {
        return;
    }

    const uint entry_address = 1u +
        (pool_index - 1u) *
        (uint)k_bbv_surface_brick_pool_entry_u32_count;
    InterlockedOr(
        RWBbvSurfaceBrickPool[entry_address + 1u + mask_word],
        merged_mask);
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
    const uint candidate_address = 1u + dtid.x * 2u;
    const uint packed_key =
        BbvSurfaceBrickPoolCandidateBuffer[candidate_address];
    const uint candidate_mask =
        BbvSurfaceBrickPoolCandidateBuffer[candidate_address + 1u];
    MergeSurfaceCandidate(packed_key, candidate_mask);
}
