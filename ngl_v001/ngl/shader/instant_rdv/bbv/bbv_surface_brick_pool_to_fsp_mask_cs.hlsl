/*
    bbv_surface_brick_pool_to_fsp_mask_cs.hlsl
    SurfaceBrickPoolの同一maskからFSP SurfaceCellMaskを生成する。
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

    const int3 toroidal_coord =
        BbvMortonIndexToPhysicalVoxelCoord(
            brick_index,
            cb_instant_rdv.bbv.grid_resolution);
    const int3 linear_coord = voxel_coord_toroidal_mapping(
        toroidal_coord,
        cb_instant_rdv.bbv.grid_resolution - cb_instant_rdv.bbv.grid_toroidal_offset,
        cb_instant_rdv.bbv.grid_resolution);
    const float3 brick_min_ws =
        float3(linear_coord) * cb_instant_rdv.bbv.cell_size +
        cb_instant_rdv.bbv.grid_min_pos;
    const float fine_cell_size =
        cb_instant_rdv.bbv.cell_size * k_bbv_per_voxel_resolution_inv;

    const uint source_word_index = gtid.x >> 2u;
    const uint source_word_shift = (gtid.x & 3u) * 8u;
    uint pending_bits =
        (BbvSurfaceBrickPoolBuffer[entry_address + 1u + source_word_index] >>
            source_word_shift) & 0xffu;

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
        const uint3 local_coord =
            calc_bbv_bitcell_pos_from_bit_index(bit_index);
        const float3 fine_voxel_center_ws =
            brick_min_ws + (float3(local_coord) + 0.5.xxx) * fine_cell_size;

        uint2 word_indices = 0u.xx;
        uint2 bit_masks = 0u.xx;
        const uint owner_count = FspGetSurfaceOwnerMaskAddresses(
            fine_voxel_center_ws,
            word_indices,
            bit_masks);
        if(owner_count > 0u)
        {
            InterlockedOr(
                RWFspSurfaceCellMaskBuffer[word_indices.x],
                bit_masks.x);
        }
        if(owner_count > 1u)
        {
            InterlockedOr(
                RWFspSurfaceCellMaskBuffer[word_indices.y],
                bit_masks.y);
        }
    }
}
