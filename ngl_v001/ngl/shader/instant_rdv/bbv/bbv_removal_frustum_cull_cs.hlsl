#if 0
bbv_removal_frustum_cull_cs.hlsl

BBV Removal用の保守的なWorld-space Brick AABB Frustum Cull。
Plane距離 + 投影AABB半径が負のときだけBrickを棄却する。
近/遠平面を先に検査し、候補リスト、wave compaction、
indirect carving downstreamは変更しない。
#endif

#include "../instant_rdv_util.hlsli"

ConstantBuffer<BbvSurfaceInjectionViewInfo> cb_injection_src_view_info;

bool reject_by_frustum_plane(float4 plane, float3 brick_center_ws, float3 brick_extents_ws)
{
    const float3 normal = plane.xyz;
    if(dot(normal, normal) <= 1e-20)
    {
        // 無限遠投影では有限な遠平面境界がないため、検査しない。
        return false;
    }

    const float distance = dot(normal, brick_center_ws) + plane.w;
    const float projected_aabb_radius = dot(abs(normal), brick_extents_ws);
    return (distance + projected_aabb_radius) < 0.0;
}

bool is_brick_inside_frustum_aabb(float3 brick_center_ws, float brick_size_ws)
{
    const float3 brick_extents_ws = 0.5 * brick_size_ws.xxx;

    // モバイルでの早期棄却のため、近/遠平面を先に検査する。
    if(reject_by_frustum_plane(cb_injection_src_view_info.cb_frustum_planes[4], brick_center_ws, brick_extents_ws) ||
       reject_by_frustum_plane(cb_injection_src_view_info.cb_frustum_planes[5], brick_center_ws, brick_extents_ws))
    {
        return false;
    }

    [unroll]
    for(uint plane_index = 0; plane_index < 4; ++plane_index)
    {
        if(reject_by_frustum_plane(cb_injection_src_view_info.cb_frustum_planes[plane_index], brick_center_ws, brick_extents_ws))
        {
            return false;
        }
    }
    return true;
}

[numthreads(k_bbv_removal_carving_thread_group_size, 1, 1)]
void main_cs(uint3 dtid : SV_DispatchThreadID)
{
    const uint brick_count = bbv_brick_count();
    bool has_candidate = false;
    uint candidate_voxel_index = 0;
    if(dtid.x < brick_count)
    {
        const int3 voxel_coord_toroidal =
            BbvMortonIndexToPhysicalVoxelCoord(dtid.x, cb_instant_rdv.bbv.grid_resolution);
        const int3 voxel_coord_linear = voxel_coord_toroidal_mapping(
            voxel_coord_toroidal,
            cb_instant_rdv.bbv.grid_resolution - cb_instant_rdv.bbv.grid_toroidal_offset,
            cb_instant_rdv.bbv.grid_resolution);

        const float3 brick_center_ws = (float3(voxel_coord_linear) + 0.5) * cb_instant_rdv.bbv.cell_size + cb_instant_rdv.bbv.grid_min_pos;
        if(is_brick_inside_frustum_aabb(brick_center_ws, cb_instant_rdv.bbv.cell_size))
        {
            // Brick count 再集計は後段で行うため、ここでは bitmask 実データで Empty 判定する。
            const uint bitmask_addr = bbv_voxel_bitmask_data_addr(dtid.x);
            uint occupied_bits = 0;
            [unroll]
            for(uint i = 0; i < k_bbv_per_voxel_bitmask_u32_count; ++i)
            {
                occupied_bits |= RWBitmaskBrickVoxel[bitmask_addr + i];
            }
            has_candidate = (0 != occupied_bits);
            candidate_voxel_index = dtid.x;
        }
    }

    // wave内で候補laneをコンパクションし、global counterへのatomicを1 waveあたり1回に集約する。
    const uint lane_local_offset = WavePrefixCountBits(has_candidate);
    const uint wave_candidate_count = WaveActiveCountBits(has_candidate);

    uint wave_reserved_begin = 0;
    if(WaveIsFirstLane() && (0 != wave_candidate_count))
    {
        InterlockedAdd(RWFrustumBrickList[0], wave_candidate_count, wave_reserved_begin);
    }
    const uint wave_base_index = WaveReadLaneFirst(wave_reserved_begin);
    if(has_candidate)
    {
        RWFrustumBrickList[wave_base_index + lane_local_offset + 1] = candidate_voxel_index;
    }
}
