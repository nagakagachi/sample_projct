#if 0

fsp_update_cs.hlsl

V1 では coarse ray sample を止め、probe pool の stale release と
active probe list build に使う。

#endif

#include "../instant_rdv_util.hlsli"
// SceneView定数バッファ構造定義.
#include "../../include/scene_view_struct.hlsli"

ConstantBuffer<SceneViewInfo> cb_ngl_sceneview;

#define FSP_OCTA_UPDATE_TEMPORAL_RATE (0.95)

[numthreads(PROBE_UPDATE_THREAD_GROUP_SIZE, 1, 1)]
void main_cs(
	uint3 dtid	: SV_DispatchThreadID,
	uint3 gtid : SV_GroupThreadID,
	uint3 gid : SV_GroupID,
	uint gindex : SV_GroupIndex
)
{
    const uint active_probe_count = FspActiveProbeListCurr[0];
    if(dtid.x >= active_probe_count)
    {
        return;
    }

    const uint probe_index = FspActiveProbeListCurr[dtid.x + 1];
    if(probe_index >= cb_instant_rdv.fsp_probe_pool_size)
    {
        return;
    }

    FspProbePoolData probe_pool_data = RWFspProbePoolBuffer[probe_index];
    if(0 == (probe_pool_data.flags & k_fsp_probe_flag_allocated))
    {
        return;
    }

    const uint owner_cell_index = probe_pool_data.owner_cell_index;
    const bool has_valid_owner = (owner_cell_index != k_fsp_invalid_probe_index);
    if(has_valid_owner)
    {
        RWFspCellStateBuffer[owner_cell_index].probe_offset_v3 = probe_pool_data.probe_offset_v3;
    }

    if(!has_valid_owner)
    {
        return;
    }

    uint cascade_index = 0;
    uint local_cell_index = 0;
    if(!FspDecodeGlobalCellIndex(owner_cell_index, cascade_index, local_cell_index))
    {
        return;
    }

    /*
    // 処理スキップ機構. 指定カスケードのみ処理.
    const uint target_cascade_index = cb_instant_rdv.frame_count % uint(max(cb_instant_rdv.fsp_cascade_count, 1));
    if(cascade_index != target_cascade_index)
    {
        return;
    }
    */

    const FspCascadeGridParam cascade = FspGetCascadeParam(cascade_index);
    const float cascade_relocation_offset_normalize_distance = (cascade.grid.cell_size * cb_instant_rdv.fsp_relocation_offset_scale_for_cascade_cell_size);
    const float3 probe_offset = decode_uint_to_range1_vec3(probe_pool_data.probe_offset_v3) * cascade_relocation_offset_normalize_distance;
    const float3 probe_pos_ws = FspCalcCellCenterWs(cascade_index, local_cell_index) + probe_offset;
    const bool enable_ray_jitter = (0 != cb_instant_rdv.debug_fsp_update_ray_jitter_enable);

    float sky_visibility_accum = 0.0;
    [unroll]
    for(uint oy = 0; oy < k_fsp_probe_octmap_width; ++oy)
    {
        [unroll]
        for(uint ox = 0; ox < k_fsp_probe_octmap_width; ++ox)
        {
            const uint2 oct_cell_id = uint2(ox, oy);
            float2 oct_uv = (float2(oct_cell_id) + float2(0.5, 0.5)) / float(k_fsp_probe_octmap_width);
            if(enable_ray_jitter)
            {
                // Octセルごとに1レイ割り当てを維持しつつ、セル内だけジッタして方向を時変化させる。
                const float2 jitter01 = float2(
                    noise_float_to_float(float2(float(probe_index + cb_instant_rdv.frame_count * 131u), float(ox + oy * k_fsp_probe_octmap_width))),
                    noise_float_to_float(float2(float(probe_index * 17u + cb_instant_rdv.frame_count * 73u), float(ox * 7u + oy * 13u))));
                const float2 jitter = (jitter01 - 0.5.xx) / float(k_fsp_probe_octmap_width);
                oct_uv = clamp(oct_uv + jitter, 1e-4.xx, (1.0 - 1e-4).xx);
            }
            const float3 sample_ray_dir = OctDecode(oct_uv);

            const float trace_distance = k_fsp_probe_distance_max;
            int hit_voxel_index = -1;
            float4 debug_ray_info;
            float4 curr_ray_t_ws = trace_bbv(
                hit_voxel_index, debug_ray_info,
                probe_pos_ws, sample_ray_dir, trace_distance,
                cb_instant_rdv.bbv.grid_min_pos, cb_instant_rdv.bbv.cell_size, cb_instant_rdv.bbv.grid_resolution,
                cb_instant_rdv.bbv.grid_toroidal_offset, BitmaskBrickVoxel);

            const bool is_sky_visible = (0.0 > curr_ray_t_ws.x);
            const float sky_visibility = is_sky_visible ? 1.0 : 0.0;
            const float3 hit_radiance = is_sky_visible
                ? 0.0.xxx
                : max(BitmaskBrickVoxelOptionData[hit_voxel_index].resolved_radiance, 0.0.xxx);

            const uint2 atlas_texel_pos = FspProbeAtlasTexelCoord(probe_index, oct_cell_id);
            const float4 atlas_prev = RWFspProbeAtlasTex[atlas_texel_pos];
            const float4 atlas_curr = float4(hit_radiance, sky_visibility);
            RWFspProbeAtlasTex[atlas_texel_pos] = lerp(atlas_curr, atlas_prev, FSP_OCTA_UPDATE_TEMPORAL_RATE);
            sky_visibility_accum += sky_visibility;
        }
    }

    probe_pool_data.last_update_frame = cb_instant_rdv.frame_count;
    RWFspProbePoolBuffer[probe_index] = probe_pool_data;
}
