/*
    bbv_surface_brick_pool_inject_cs.hlsl
    SurfaceBrickPoolの512bit maskをBBV bitmaskへ注入する。
*/

#include "../instant_rdv_util.hlsli"

Buffer<uint> BbvSurfaceBrickPoolBuffer;

[numthreads(64, 1, 1)]
void main_cs(uint3 gtid : SV_GroupThreadID, uint3 gid : SV_GroupID)
{
    const uint entry_index = gid.x;
    const uint pool_count = min(
        BbvSurfaceBrickPoolBuffer[0],
        (uint)k_bbv_surface_brick_pool_capacity);
    if(entry_index >= pool_count)
    {
        return;
    }

    const uint entry_address = 1u + entry_index *
        (uint)k_bbv_surface_brick_pool_entry_u32_count;
    const uint brick_index = BbvSurfaceBrickPoolBuffer[entry_address];
    if(brick_index >= bbv_brick_count())
    {
        return;
    }

    const uint source_word_index = gtid.x >> 2u;
    const uint source_word_shift = (gtid.x & 3u) * 8u;
    uint pending_bits =
        (BbvSurfaceBrickPoolBuffer[entry_address + 1u + source_word_index] >>
            source_word_shift) & 0xffu;
    const uint bbv_address = bbv_voxel_bitmask_data_addr(brick_index);

    [unroll]
    for(uint bit_iteration = 0u; bit_iteration < 8u; ++bit_iteration)
    {
        if(pending_bits == 0u)
        {
            break;
        }
        const uint local_bit = firstbitlow(pending_bits);
        pending_bits &= pending_bits - 1u;

        const uint bit_index = gtid.x * 8u + local_bit;
        const uint target_word_index = bit_index >> 5u;
        const uint target_bit_index = bit_index & 31u;
        InterlockedOr(
            RWBitmaskBrickVoxel[bbv_address + target_word_index],
            1u << target_bit_index);
    }
}
