#if 0

fsp_probe_ray_trace_cs.hlsl

FSP update multipass の Trace パス。
1thread = 1ray で BBV をトレースし、結果は hit voxel index(+1) だけを
圧縮保存する。0 は sky hit を表す。
Radiance 読み取りは Resolve パスへ分離して trace を軽量化する。

#endif

#include "../instant_rdv_util.hlsli"
#include "../../include/scene_view_struct.hlsli"

ConstantBuffer<SceneViewInfo> cb_ngl_sceneview;

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
        RWFspProbeRayResultBuffer[ray_global_index] = 0u;
        return;
    }

    const FspProbePoolData probe_pool_data = FspProbePoolBuffer[probe_index];
    if((0u == (probe_pool_data.flags & k_fsp_probe_flag_allocated)) || (probe_pool_data.owner_cell_index == k_fsp_invalid_probe_index))
    {
        RWFspProbeRayResultBuffer[ray_global_index] = 0u;
        return;
    }

    uint cascade_index = 0u;
    uint local_cell_index = 0u;
    if(!FspDecodeGlobalCellIndex(probe_pool_data.owner_cell_index, cascade_index, local_cell_index))
    {
        RWFspProbeRayResultBuffer[ray_global_index] = 0u;
        return;
    }

    const FspCascadeGridParam cascade = FspGetCascadeParam(cascade_index);
    const float cascade_relocation_offset_normalize_distance =
        (cascade.grid.cell_size * cb_instant_rdv.fsp_relocation_offset_scale_for_cascade_cell_size);
    const float3 probe_offset =
        decode_uint_to_range1_vec3(probe_pool_data.probe_offset_v3) * cascade_relocation_offset_normalize_distance;
    const float3 probe_pos_ws = FspCalcCellCenterWs(cascade_index, local_cell_index) + probe_offset;

    const uint oct_x = local_ray_index % k_fsp_probe_octmap_width;
    const uint oct_y = local_ray_index / k_fsp_probe_octmap_width;
    float2 oct_uv = (float2(oct_x, oct_y) + 0.5.xx) / float(k_fsp_probe_octmap_width);
    if(0 != cb_instant_rdv.debug_fsp_update_ray_jitter_enable)
    {
        // 1セル=1レイ対応は維持しつつ、セル内だけ時変ジッタする。
        const float2 jitter01 = float2(
            noise_float_to_float(float2(float(probe_index + cb_instant_rdv.frame_count * 131u), float(local_ray_index))),
            noise_float_to_float(float2(float(probe_index * 17u + cb_instant_rdv.frame_count * 73u), float(local_ray_index * 13u + 7u))));
        const float2 jitter = (jitter01 - 0.5.xx) / float(k_fsp_probe_octmap_width);
        oct_uv = clamp(oct_uv + jitter, 1e-4.xx, (1.0 - 1e-4).xx);
    }

    const float3 sample_ray_dir = OctDecode(oct_uv);
    const float trace_distance = k_fsp_probe_distance_max;
    int hit_voxel_index = -1;
    float4 debug_ray_info = 0.0.xxxx;
    const float4 curr_ray_t_ws = trace_bbv(
        hit_voxel_index, debug_ray_info,
        probe_pos_ws, sample_ray_dir, trace_distance,
        cb_instant_rdv.bbv.grid_min_pos, cb_instant_rdv.bbv.cell_size, cb_instant_rdv.bbv.grid_resolution,
        cb_instant_rdv.bbv.grid_toroidal_offset, BitmaskBrickVoxel);

    const bool is_sky_visible = (curr_ray_t_ws.x < 0.0) || (hit_voxel_index < 0);
    RWFspProbeRayResultBuffer[ray_global_index] = is_sky_visible ? 0u : (uint(hit_voxel_index) + 1u);
}
