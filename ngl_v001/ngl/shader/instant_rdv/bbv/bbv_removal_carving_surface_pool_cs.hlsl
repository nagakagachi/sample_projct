/*
    bbv_removal_carving_surface_pool_cs.hlsl
    SurfaceBrickPool maskを保護しながらMainViewのBBV carvingを実行する。
*/

#include "../instant_rdv_util.hlsli"
#include "../../include/scene_view_struct.hlsli"

ConstantBuffer<BbvSurfaceInjectionViewInfo> cb_injection_src_view_info;
Texture2D TexHardwareDepth;

Buffer<uint> BbvSurfaceBrickPoolStateBuffer;
Buffer<uint> BbvSurfaceBrickPoolBuffer;

[numthreads(k_bbv_removal_carving_thread_group_size, 1, 1)]
void main_cs(uint3 dtid : SV_DispatchThreadID)
{
    const uint active_brick_count = FrustumBrickList[0];
    const uint u32_per_brick = k_bbv_per_voxel_bitmask_u32_count;
    const uint total_u32_jobs = active_brick_count * u32_per_brick;
    if(dtid.x >= total_u32_jobs)
    {
        return;
    }

    const uint brick_list_index = dtid.x / u32_per_brick;
    const uint u32_offset = dtid.x - brick_list_index * u32_per_brick;
    const uint lane_voxel_index = FrustumBrickList[brick_list_index + 1u];
    const uint lane_index = WaveGetLaneIndex();
    const uint slot_first_lane = (lane_index / u32_per_brick) * u32_per_brick;
    const uint voxel_index = WaveReadLaneAt(lane_voxel_index, slot_first_lane);

    int3 voxel_coord_linear = int3(0, 0, 0);
    if(lane_index == slot_first_lane)
    {
        const int3 voxel_coord_toroidal =
            BbvMortonIndexToPhysicalVoxelCoord(
                voxel_index,
                cb_instant_rdv.bbv.grid_resolution);
        voxel_coord_linear = voxel_coord_toroidal_mapping(
            voxel_coord_toroidal,
            cb_instant_rdv.bbv.grid_resolution - cb_instant_rdv.bbv.grid_toroidal_offset,
            cb_instant_rdv.bbv.grid_resolution);
    }
    voxel_coord_linear.x = WaveReadLaneAt(voxel_coord_linear.x, slot_first_lane);
    voxel_coord_linear.y = WaveReadLaneAt(voxel_coord_linear.y, slot_first_lane);
    voxel_coord_linear.z = WaveReadLaneAt(voxel_coord_linear.z, slot_first_lane);

    const float3 brick_origin_ws =
        float3(voxel_coord_linear) * cb_instant_rdv.bbv.cell_size +
        cb_instant_rdv.bbv.grid_min_pos;
    const float bitcell_step_ws =
        cb_instant_rdv.bbv.cell_size * k_bbv_per_voxel_resolution_inv;
    const int2 depth_size =
        cb_injection_src_view_info.cb_view_depth_buffer_offset_size.zw;
    const int2 depth_atlas_offset =
        cb_injection_src_view_info.cb_view_depth_buffer_offset_size.xy;

    const uint state = BbvSurfaceBrickPoolStateBuffer[voxel_index];
    const uint generation = max(
        1u,
        cb_instant_rdv.frame_count & 0xffffu);
    const uint state_generation = state >> 16u;
    const uint pool_index = state & 0xffffu;
    const bool has_pool_mask =
        state_generation == generation &&
        pool_index > 0u &&
        pool_index <= (uint)k_bbv_surface_brick_pool_capacity &&
        pool_index != k_bbv_surface_brick_pool_state_overflow_index &&
        pool_index != k_bbv_surface_brick_pool_state_reserved_index;
    const uint pool_entry_address = has_pool_mask
        ? (1u + (pool_index - 1u) *
            (uint)k_bbv_surface_brick_pool_entry_u32_count)
        : 0u;

    const uint bbv_address = bbv_voxel_bitmask_data_addr(voxel_index);
    uint bit_block = RWBitmaskBrickVoxel[bbv_address + u32_offset];
    if(0 == WaveActiveCountBits(0 != bit_block))
    {
        return;
    }

    uint remain_mask = bit_block;
    uint scan_mask = bit_block;
    [loop]
    while(scan_mask != 0u)
    {
        const uint bit_in_u32 = firstbitlow(scan_mask);
        const uint bit_value = 1u << bit_in_u32;
        scan_mask &= scan_mask - 1u;

        // PoolとBBV注入が共有するbitはMainView surfaceとして保護する。
        if(has_pool_mask &&
           (BbvSurfaceBrickPoolBuffer[pool_entry_address + 1u + u32_offset] &
                bit_value) != 0u)
        {
            continue;
        }

        const uint bit_index = u32_offset * 32u + bit_in_u32;
        const uint3 bitcell_pos =
            calc_bbv_bitcell_pos_from_bit_index(bit_index);
        const float3 bitcell_center_ws =
            brick_origin_ws + (float3(bitcell_pos) + 0.5.xxx) * bitcell_step_ws;
        const float3 bitcell_center_vs =
            mul(cb_injection_src_view_info.cb_view_mtx,
                float4(bitcell_center_ws, 1.0)).xyz;
        const float4 bitcell_center_cs =
            mul(cb_injection_src_view_info.cb_proj_mtx,
                float4(bitcell_center_vs, 1.0));
        if(abs(bitcell_center_cs.w) <= 1e-6)
        {
            continue;
        }

        const float3 ndc = bitcell_center_cs.xyz / bitcell_center_cs.w;
        if(ndc.z < 0.0 || ndc.z > 1.0)
        {
            continue;
        }
        const float2 uv = float2(
            ndc.x * 0.5 + 0.5,
            -ndc.y * 0.5 + 0.5);
        if(any(uv < 0.0) || any(uv > 1.0))
        {
            continue;
        }

        const int2 screen_pos = clamp(
            int2(uv * float2(depth_size)),
            int2(0, 0),
            depth_size - 1);
        const float surface_depth = TexHardwareDepth.Load(
            int3(screen_pos + depth_atlas_offset, 0)).r;
        if(!isValidDepth(surface_depth))
        {
            // MainViewでは深度なし領域の残留occupancyを削除する。
            remain_mask &= ~bit_value;
            continue;
        }

        const float surface_view_z = calc_view_z_from_ndc_z(
            surface_depth,
            cb_injection_src_view_info.cb_ndc_z_to_view_z_coef);
        const float center_len_sq = dot(
            bitcell_center_vs,
            bitcell_center_vs);
        const float3 center_dir = center_len_sq > 1e-10
            ? bitcell_center_vs * rsqrt(center_len_sq)
            : float3(0.0, 0.0, 1.0);
        const float bitcell_sphere_radius =
            0.5 * length(bitcell_step_ws);
        const float3 bitcell_nearest_vs =
            bitcell_center_vs - center_dir * bitcell_sphere_radius;
        if(bitcell_nearest_vs.z < surface_view_z)
        {
            remain_mask &= ~bit_value;
        }
    }

    RWBitmaskBrickVoxel[bbv_address + u32_offset] = remain_mask;
}
