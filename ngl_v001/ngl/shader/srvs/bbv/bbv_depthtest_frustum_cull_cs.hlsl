#if 0
bbv_depthtest_frustum_cull_cs.hlsl

DepthTest ベース更新向けの Frustum Cull。
候補 Brick を ActiveList 形式で RWFrustumBrickList に収集する。
0番はカウンタ、1番以降に voxel index を格納する。
#endif

#include "../srvs_util.hlsli"

ConstantBuffer<BbvSurfaceInjectionViewInfo> cb_injection_src_view_info;

[numthreads(k_bbv_depthtest_carving_thread_group_size, 1, 1)]
void main_cs(uint3 dtid : SV_DispatchThreadID)
{
    const uint brick_count = bbv_brick_count();
    bool has_candidate = false;
    uint candidate_voxel_index = 0;
    if(dtid.x < brick_count)
    {
        const int3 voxel_coord_toroidal = index_to_voxel_coord(dtid.x, cb_srvs.bbv.grid_resolution);
        const int3 voxel_coord_linear = voxel_coord_toroidal_mapping(
            voxel_coord_toroidal,
            cb_srvs.bbv.grid_resolution - cb_srvs.bbv.grid_toroidal_offset,
            cb_srvs.bbv.grid_resolution);

        const float3 brick_center_ws = (float3(voxel_coord_linear) + 0.5) * cb_srvs.bbv.cell_size + cb_srvs.bbv.grid_min_pos;
        const float3 brick_center_vs = mul(cb_injection_src_view_info.cb_view_mtx, float4(brick_center_ws, 1.0));
        const float4 brick_center_cs = mul(cb_injection_src_view_info.cb_proj_mtx, float4(brick_center_vs, 1.0));
        if(abs(brick_center_cs.w) > 1e-6)
        {
            const float3 ndc = brick_center_cs.xyz / brick_center_cs.w;
            const bool inside_frustum =
                (abs(ndc.x) <= 1.0) &&
                (abs(ndc.y) <= 1.0);
            if(inside_frustum)
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
