#if 0
bbv_depthtest_frustum_cull_cs.hlsl

DepthTest ベース更新向けの Frustum Cull。
候補 Brick を ActiveList 形式で RWFrustumBrickList に収集する。
0番はカウンタ、1番以降に voxel index を格納する。
#endif

#include "../srvs_util.hlsli"

ConstantBuffer<BbvSurfaceInjectionViewInfo> cb_injection_src_view_info;

groupshared uint gs_candidate_voxel_index[k_bbv_depthtest_carving_thread_group_size];
groupshared uint gs_candidate_count;
groupshared uint gs_unique_voxel_index[k_bbv_depthtest_carving_thread_group_size];
groupshared uint gs_unique_count;

[numthreads(k_bbv_depthtest_carving_thread_group_size, 1, 1)]
void main_cs(uint3 dtid : SV_DispatchThreadID, uint gindex : SV_GroupIndex)
{
    if(0 == gindex)
    {
        gs_candidate_count = 0;
        gs_unique_count = 0;
    }
    GroupMemoryBarrierWithGroupSync();

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

    if(has_candidate)
    {
        uint candidate_index = 0;
        InterlockedAdd(gs_candidate_count, 1, candidate_index);
        if(candidate_index < k_bbv_depthtest_carving_thread_group_size)
        {
            gs_candidate_voxel_index[candidate_index] = candidate_voxel_index;
        }
    }
    GroupMemoryBarrierWithGroupSync();

    if(0 == gindex)
    {
        uint local_unique_count = 0;
        const uint local_candidate_count = min(gs_candidate_count, k_bbv_depthtest_carving_thread_group_size);
        for(uint i = 0; i < local_candidate_count; ++i)
        {
            const uint candidate_voxel_index = gs_candidate_voxel_index[i];
            bool is_duplicate = false;
            for(uint j = 0; j < local_unique_count; ++j)
            {
                if(gs_unique_voxel_index[j] == candidate_voxel_index)
                {
                    is_duplicate = true;
                    break;
                }
            }
            if(!is_duplicate)
            {
                gs_unique_voxel_index[local_unique_count++] = candidate_voxel_index;
            }
        }

        gs_unique_count = local_unique_count;
        if(0 == local_unique_count)
        {
            return;
        }

        uint reserved_begin = 0;
        InterlockedAdd(RWFrustumBrickList[0], local_unique_count, reserved_begin);

        const uint active_capacity = brick_count;
        const uint writable_count = (reserved_begin < active_capacity)
            ? min(local_unique_count, active_capacity - reserved_begin)
            : 0u;

        for(uint i = 0; i < k_bbv_depthtest_carving_thread_group_size; ++i)
        {
            if(i >= writable_count)
            {
                break;
            }
            RWFrustumBrickList[reserved_begin + 1 + i] = gs_unique_voxel_index[i];
        }

        if(writable_count < local_unique_count)
        {
            InterlockedAdd(RWFrustumBrickList[0], 0u - (local_unique_count - writable_count));
        }
    }
}
