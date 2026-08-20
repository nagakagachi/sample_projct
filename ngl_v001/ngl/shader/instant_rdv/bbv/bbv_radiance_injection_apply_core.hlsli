#pragma once

#include "../instant_rdv_util.hlsli"
#include "../../include/scene_view_struct.hlsli"

#ifndef NGL_INSTANT_RDV_RADIANCE_ENABLE_SHORT_RAY_FALLBACK
    #define NGL_INSTANT_RDV_RADIANCE_ENABLE_SHORT_RAY_FALLBACK 0
#endif

ConstantBuffer<BbvSurfaceInjectionViewInfo> cb_injection_src_view_info;

Texture2D TexHardwareDepth;
Texture2D<float4> TexInputRadiance;

bool bbv_try_calc_voxel_index_from_pos_ws(float3 pos_ws, out uint voxel_index)
{
    const float3 voxel_coordf = (pos_ws - cb_instant_rdv.bbv.grid_min_pos) * cb_instant_rdv.bbv.cell_size_inv;
    const int3 voxel_coord = floor(voxel_coordf);
    if(any(voxel_coord < 0) || any(voxel_coord >= cb_instant_rdv.bbv.grid_resolution))
    {
        voxel_index = 0;
        return false;
    }

    const int3 voxel_coord_toroidal = voxel_coord_toroidal_mapping(voxel_coord, cb_instant_rdv.bbv.grid_toroidal_offset, cb_instant_rdv.bbv.grid_resolution);
    voxel_index =
        BbvPhysicalVoxelCoordToMortonIndex(voxel_coord_toroidal, cb_instant_rdv.bbv.grid_resolution);
    return true;
}

uint3 bbv_to_fixed_point_radiance(float3 input_radiance)
{
    const float3 clamped_radiance = min(max(input_radiance, 0.0.xxx), k_bbv_radiance_input_clamp.xxx);
    return (uint3)(clamped_radiance * k_bbv_radiance_fixed_point_scale + 0.5.xxx);
}

void bbv_accumulate_radiance_to_voxel_wave(uint voxel_index, uint3 fixed_point_radiance, bool has_injection)
{
    // Wave内で同一 voxel_index への加算を集約し、代表laneのみ atomic を実行する。
    uint4 pending_lanes = WaveActiveBallot(has_injection);
    while(ballot_any(pending_lanes))
    {
        const uint leader_lane = first_lane_from_ballot(pending_lanes);
        const uint leader_voxel = WaveReadLaneAt(voxel_index, leader_lane);
        const bool is_same_target = has_injection && (voxel_index == leader_voxel);
        const uint4 same_target_lanes = WaveActiveBallot(is_same_target);

        const uint merged_r = WaveActiveSum(is_same_target ? fixed_point_radiance.x : 0u);
        const uint merged_g = WaveActiveSum(is_same_target ? fixed_point_radiance.y : 0u);
        const uint merged_b = WaveActiveSum(is_same_target ? fixed_point_radiance.z : 0u);
        const uint merged_count = WaveActiveCountBits(is_same_target);

        if(WaveGetLaneIndex() == leader_lane)
        {
            InterlockedAdd(RWBbvRadianceAccumBuffer[bbv_radiance_accum_r_addr(leader_voxel)], merged_r);
            InterlockedAdd(RWBbvRadianceAccumBuffer[bbv_radiance_accum_g_addr(leader_voxel)], merged_g);
            InterlockedAdd(RWBbvRadianceAccumBuffer[bbv_radiance_accum_b_addr(leader_voxel)], merged_b);
            InterlockedAdd(RWBbvRadianceAccumBuffer[bbv_radiance_accum_count_addr(leader_voxel)], merged_count);
        }

        pending_lanes &= ~same_target_lanes;
    }
}

[numthreads(k_bbv_radiance_injection_tile_width, k_bbv_radiance_injection_tile_width, 1)]
void main_cs(
    uint3 dtid   : SV_DispatchThreadID,
    uint3 gtid   : SV_GroupThreadID,
    uint3 gid    : SV_GroupID,
    uint gindex  : SV_GroupIndex
)
{
    const int2 src_resolution = cb_injection_src_view_info.cb_view_depth_buffer_offset_size.zw;
    const int2 group_grid_resolution = bbv_radiance_injection_group_grid_resolution(src_resolution);
    if(any(int2(gid.xy) >= group_grid_resolution))
    {
        return;
    }

    int2 tile_coord = 0;
    if(!bbv_radiance_injection_group_coord_to_tile_coord(gid.xy, tile_coord, src_resolution))
    {
        return;
    }

    const int2 src_texel_in_view =
        tile_coord * k_bbv_radiance_injection_tile_width +
        int2(gtid.xy);
    if(any(src_texel_in_view >= src_resolution))
    {
        return;
    }

    const int2 src_texel = src_texel_in_view + cb_injection_src_view_info.cb_view_depth_buffer_offset_size.xy;
    const float depth = TexHardwareDepth.Load(int3(src_texel, 0)).r;
    if(!isValidDepth(depth))
    {
        return;
    }

    const float view_z = calc_view_z_from_ndc_z(depth, cb_injection_src_view_info.cb_ndc_z_to_view_z_coef);
    const float2 screen_uv = (float2(src_texel_in_view) + 0.5.xx) / float2(src_resolution);
    // 始点は従来通り pixel world position を使う。
    float3 pos_ws = mul(cb_injection_src_view_info.cb_view_inv_mtx, float4(CalcViewSpacePosition(screen_uv, view_z, cb_injection_src_view_info.cb_proj_mtx), 1.0));

    const bool use_surface_brick_pool_position =
        cb_injection_src_view_info.cb_is_main_view != 0 &&
        cb_injection_src_view_info.cb_padding0.x != 0;
    float3 ray_dir_ws = 0.0.xxx;
    if(!use_surface_brick_pool_position)
    {
        const float2 near_far_plane_d =
            GetNearFarPlaneDepthFromProjectionMatrix(
                cb_injection_src_view_info.cb_proj_mtx);
        const float near_plane_view_z = calc_view_z_from_ndc_z(
            near_far_plane_d.x,
            cb_injection_src_view_info.cb_ndc_z_to_view_z_coef);
        const float3 view_ray_origin_ws = mul(
            cb_injection_src_view_info.cb_view_inv_mtx,
            float4(CalcViewSpacePosition(
                screen_uv,
                near_plane_view_z,
                cb_injection_src_view_info.cb_proj_mtx), 1.0));
        const float3 to_pixel_vec_ws = pos_ws - view_ray_origin_ws;
        const float to_pixel_len_sq = dot(to_pixel_vec_ws, to_pixel_vec_ws);
        if(to_pixel_len_sq > 1e-10)
        {
            ray_dir_ws = normalize(to_pixel_vec_ws);
            pos_ws += ray_dir_ws *
                cb_instant_rdv.bbv_occupancy_injection_world_offset;
        }
    }

    uint voxel_index = 0;
    if(!bbv_try_calc_voxel_index_from_pos_ws(pos_ws, voxel_index))
    {
        return;
    }

    const float3 input_radiance = TexInputRadiance.Load(int3(src_texel, 0)).rgb;
    const uint3 fixed_point_radiance = bbv_to_fixed_point_radiance(input_radiance);
    bool has_target_voxel = true;
    uint target_voxel_index = voxel_index;

    #if NGL_INSTANT_RDV_RADIANCE_ENABLE_SHORT_RAY_FALLBACK
        // Variation有効時:
        // 始点BrickがOccupiedなら通常注入。
        // Emptyなら「奥方向へ最大1Brick」の短距離探索で最初のOccupiedへ注入する。
        const bool is_start_occupied = (0 < BitmaskBrickVoxel[bbv_voxel_coarse_occupancy_info_addr(voxel_index)]);
        if(!is_start_occupied && all(ray_dir_ws == 0.0.xxx))
        {
            has_target_voxel = false;
        }
        if(has_target_voxel && !is_start_occupied)
        {
            const float short_ray_length_ws = cb_instant_rdv.bbv.cell_size * k_bbv_radiance_short_ray_length_in_brick;
            const float ray_step_ws = short_ray_length_ws / max(float(k_bbv_radiance_short_ray_step_count), 1.0);
            bool found_fallback_target = false;

            [loop]
            for(int step_index = 1; step_index <= k_bbv_radiance_short_ray_step_count; ++step_index)
            {
                const float3 sample_pos_ws = pos_ws + ray_dir_ws * (ray_step_ws * float(step_index));
                uint sample_voxel_index = 0;
                if(!bbv_try_calc_voxel_index_from_pos_ws(sample_pos_ws, sample_voxel_index))
                {
                    continue;
                }

                if(0 < BitmaskBrickVoxel[bbv_voxel_coarse_occupancy_info_addr(sample_voxel_index)])
                {
                    target_voxel_index = sample_voxel_index;
                    found_fallback_target = true;
                    break;
                }
            }
            has_target_voxel = found_fallback_target;
        }
    #endif

    bbv_accumulate_radiance_to_voxel_wave(target_voxel_index, fixed_point_radiance, has_target_voxel);
}
