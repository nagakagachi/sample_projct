#if 0

bbv_surface_touched_fsp_candidate_cs.hlsl

Touched Brickの512bit FineVoxel maskを列挙し、FSP SurfaceCellMaskへ直接書き込む。

#endif

#include "../instant_rdv_util.hlsli"

[numthreads(64, 1, 1)]
void main_cs(uint3 gtid : SV_GroupThreadID, uint3 gid : SV_GroupID)
{
    const uint touched_index = gid.x;
    const uint touched_count = min(BbvSurfaceTouchedBrickList[0], bbv_brick_count());
    if(touched_index >= touched_count)
    {
        return;
    }

    const uint brick_index = BbvSurfaceTouchedBrickList[touched_index + 1u];
    if(brick_index >= bbv_brick_count())
    {
        return;
    }

    const int3 toroidal_coord =
        BbvMortonIndexToPhysicalVoxelCoord(brick_index, cb_instant_rdv.bbv.grid_resolution);
    const int3 linear_coord = voxel_coord_toroidal_mapping(
        toroidal_coord,
        cb_instant_rdv.bbv.grid_resolution - cb_instant_rdv.bbv.grid_toroidal_offset,
        cb_instant_rdv.bbv.grid_resolution);
    const float3 brick_min_ws =
        float3(linear_coord) * cb_instant_rdv.bbv.cell_size +
        cb_instant_rdv.bbv.grid_min_pos;
    const float fine_cell_size =
        cb_instant_rdv.bbv.cell_size * k_bbv_per_voxel_resolution_inv;
    const uint word_index = gtid.x >> 2u;
    const uint word_shift = (gtid.x & 3u) * 8u;
    uint pending_bits =
        (BitmaskBrickVoxel[bbv_voxel_bitmask_data_addr(brick_index) + word_index] >> word_shift) & 0xffu;

    [unroll]
    for(uint bit_iteration = 0u; bit_iteration < 8u; ++bit_iteration)
    {
        const bool has_fine_voxel = pending_bits != 0u;
        const uint local_bit = has_fine_voxel ? firstbitlow(pending_bits) : 0u;
        const uint bit_index = gtid.x * 8u + local_bit;
        const uint3 local_coord = calc_bbv_bitcell_pos_from_bit_index(bit_index);
        const float3 fine_voxel_center_ws =
            brick_min_ws + (float3(local_coord) + 0.5.xxx) * fine_cell_size;

        uint2 owner_mask_words = 0u.xx;
        uint2 owner_mask_bits = 0u.xx;
        const uint owner_count = FspGetSurfaceOwnerMaskAddresses(
            fine_voxel_center_ws,
            owner_mask_words,
            owner_mask_bits);
        FspInjectCellMaskWave(
            has_fine_voxel && owner_count > 0u,
            owner_mask_words.x,
            owner_mask_bits.x);
        FspInjectCellMaskWave(
            has_fine_voxel && owner_count > 1u,
            owner_mask_words.y,
            owner_mask_bits.y);

        if(has_fine_voxel)
        {
            pending_bits &= pending_bits - 1u;
        }
    }
}
