#if 0

fsp_probe_ray_resolve_cs.hlsl

FSP update multipass の Resolve パス。
Trace パスが保存した hit voxel index を使って radiance を取得し、
ProbeAtlas を更新する。trace 側から radiance read/write を分離している。

#endif

#include "../instant_rdv_util.hlsli"

#define FSP_OCTA_UPDATE_TEMPORAL_RATE (0.95)
#define FSP_RAY_LINEAR_THREAD_GROUP_SIZE 128u
#define FSP_RAY_RESULT_STRIDE 2u
#define FSP_RAY_RESULT_PACKED_REQUEST_KEY 0u
#define FSP_RAY_RESULT_HIT_INFO 1u

[numthreads(FSP_RAY_LINEAR_THREAD_GROUP_SIZE, 1, 1)]
void main_cs(
    uint3 gtid : SV_GroupThreadID,
    uint gindex : SV_GroupIndex,
    uint3 gid : SV_GroupID)
{
    const uint result_linear_index = gid.x * FSP_RAY_LINEAR_THREAD_GROUP_SIZE + gindex;
    const uint total_result_count = FspProbeRayResultBuffer[0];
    if(result_linear_index >= total_result_count)
    {
        return;
    }

    const uint result_word_offset = 1u + result_linear_index * FSP_RAY_RESULT_STRIDE;
    const uint packed_request_key = FspProbeRayResultBuffer[result_word_offset + FSP_RAY_RESULT_PACKED_REQUEST_KEY];
    const uint hit_voxel_index_plus_1 = FspProbeRayResultBuffer[result_word_offset + FSP_RAY_RESULT_HIT_INFO];

    const uint probe_index = FspUnpackRayRequestProbeIndex(packed_request_key);
    const uint oct_cell_index = FspUnpackRayRequestOctCellIndex(packed_request_key);
    if(probe_index >= (uint)cb_instant_rdv.fsp_probe_pool_size)
    {
        return;
    }
    if(oct_cell_index >= (k_fsp_probe_octmap_width * k_fsp_probe_octmap_width))
    {
        return;
    }

    FspProbePoolData probe_pool_data = RWFspProbePoolBuffer[probe_index];
    if((0u == (probe_pool_data.flags & k_fsp_probe_flag_allocated)) || (probe_pool_data.owner_cell_index == k_fsp_invalid_probe_index))
    {
        return;
    }

    const uint oct_x = oct_cell_index % k_fsp_probe_octmap_width;
    const uint oct_y = oct_cell_index / k_fsp_probe_octmap_width;
    const uint2 oct_cell_id = uint2(oct_x, oct_y);
    const bool is_sky_visible = (0u == hit_voxel_index_plus_1);
    const float sky_visibility = is_sky_visible ? 1.0 : 0.0;
    const float3 hit_radiance = is_sky_visible
        ? 0.0.xxx
        : max(BitmaskBrickVoxelOptionData[hit_voxel_index_plus_1 - 1u].resolved_radiance, 0.0.xxx);

    const uint2 atlas_texel_pos = FspProbeAtlasTexelCoord(probe_index, oct_cell_id);
    const float4 atlas_prev = RWFspProbeAtlasTex[atlas_texel_pos];
    const float4 atlas_curr = float4(hit_radiance, sky_visibility);
    RWFspProbeAtlasTex[atlas_texel_pos] = lerp(atlas_curr, atlas_prev, FSP_OCTA_UPDATE_TEMPORAL_RATE);

    // 現状は「1 oct cell = 1 ray = 1 result」前提なので atlas への直書きで衝突しない。
    // 将来 multi-ray per oct cell へ変更する場合は、ここを加算/集約方式へ差し替えること。
    if(oct_cell_index == 0u)
    {
        probe_pool_data.last_update_frame = cb_instant_rdv.frame_count;
        RWFspProbePoolBuffer[probe_index] = probe_pool_data;
    }
}
