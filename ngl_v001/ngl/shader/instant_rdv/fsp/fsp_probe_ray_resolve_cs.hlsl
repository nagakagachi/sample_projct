#if 0

fsp_probe_ray_resolve_cs.hlsl

FSP update multipass の Resolve パス。
Trace パスが保存した hit voxel index を使って radiance を取得し、
ProbeAtlas を更新する。trace 側から radiance read/write を分離している。

#endif

#include "../instant_rdv_util.hlsli"

#define FSP_OCTA_UPDATE_TEMPORAL_RATE (0.95)
#define FSP_PROBE_RAY_COUNT_PER_PROBE (k_fsp_probe_octmap_width * k_fsp_probe_octmap_width)
#define FSP_RAY_LINEAR_THREAD_GROUP_SIZE 128u

[numthreads(FSP_RAY_LINEAR_THREAD_GROUP_SIZE, 1, 1)]
void main_cs(
    uint3 gtid : SV_GroupThreadID,
    uint gindex : SV_GroupIndex,
    uint3 gid : SV_GroupID)
{
    const uint ray_global_index = gid.x * FSP_RAY_LINEAR_THREAD_GROUP_SIZE + gindex;
    const uint total_ray_count = FspProbeTotalRayCountBuffer[0];
    if(ray_global_index >= total_ray_count)
    {
        return;
    }

    const uint probe_list_index = ray_global_index / FSP_PROBE_RAY_COUNT_PER_PROBE;
    const uint local_ray_index = ray_global_index - probe_list_index * FSP_PROBE_RAY_COUNT_PER_PROBE;
    const uint probe_index = FspProbeRayRequestBuffer[probe_list_index];
    if(probe_index >= (uint)cb_instant_rdv.fsp_probe_pool_size)
    {
        return;
    }

    FspProbePoolData probe_pool_data = RWFspProbePoolBuffer[probe_index];
    if((0u == (probe_pool_data.flags & k_fsp_probe_flag_allocated)) || (probe_pool_data.owner_cell_index == k_fsp_invalid_probe_index))
    {
        return;
    }

    const uint oct_x = local_ray_index % k_fsp_probe_octmap_width;
    const uint oct_y = local_ray_index / k_fsp_probe_octmap_width;
    const uint2 oct_cell_id = uint2(oct_x, oct_y);

    const uint hit_voxel_index_plus_1 = FspProbeRayResultBuffer[ray_global_index];
    const bool is_sky_visible = (0u == hit_voxel_index_plus_1);
    const float sky_visibility = is_sky_visible ? 1.0 : 0.0;
    const float3 hit_radiance = is_sky_visible
        ? 0.0.xxx
        : max(BitmaskBrickVoxelOptionData[hit_voxel_index_plus_1 - 1u].resolved_radiance, 0.0.xxx);

    const uint2 atlas_texel_pos = FspProbeAtlasTexelCoord(probe_index, oct_cell_id);
    const float4 atlas_prev = RWFspProbeAtlasTex[atlas_texel_pos];
    const float4 atlas_curr = float4(hit_radiance, sky_visibility);
    RWFspProbeAtlasTex[atlas_texel_pos] = lerp(atlas_curr, atlas_prev, FSP_OCTA_UPDATE_TEMPORAL_RATE);

    // 1probe=固定ray本数運用では local_ray_index==0 が probe単位の代表スレッドになる。
    if(local_ray_index == 0u)
    {
        probe_pool_data.last_update_frame = cb_instant_rdv.frame_count;
        RWFspProbePoolBuffer[probe_index] = probe_pool_data;
    }
}
