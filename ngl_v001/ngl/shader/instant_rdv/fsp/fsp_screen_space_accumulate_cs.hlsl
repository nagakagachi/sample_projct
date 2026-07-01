#if 0
fsp_screen_space_accumulate_cs.hlsl

FSPの画面空間収集パス。
Depthから候補セルを求め、groupsharedで重複を圧縮して
セル可視フレーム/セルdepth hintワークへ集約する。
#endif

#define TILE_WIDTH 16
#define GROUP_SLOT_COUNT (TILE_WIDTH * TILE_WIDTH)
#define GROUP_HASH_PROBE_MAX 16

#include "../instant_rdv_util.hlsli"
#include "../../include/scene_view_struct.hlsli"

ConstantBuffer<SceneViewInfo> cb_ngl_sceneview;
Texture2D TexHardwareDepth;

static const uint k_fsp_depth_hint_metric_init = 0xffffffffu;
static const uint k_fsp_depth_hint_pixel_bits = 24u;
static const uint k_fsp_depth_hint_metric_shift = k_fsp_depth_hint_pixel_bits;
static const uint k_fsp_depth_hint_pixel_mask = (1u << k_fsp_depth_hint_pixel_bits) - 1u;
static const float k_fsp_depth_hint_max_metric_q = 254.0;

groupshared uint gs_cell_slot_index[GROUP_SLOT_COUNT];
groupshared uint gs_cell_slot_min_key[GROUP_SLOT_COUNT];

uint FspBuildDepthHintPackedKey(
    uint cascade_index,
    uint global_cell_index,
    float3 hint_sample_pos_ws,
    uint pixel_id,
    float3 view_origin)
{
    const uint cascade_local_cell_index = global_cell_index - cb_instant_rdv.fsp_cascade[cascade_index].cell_offset;
    const float3 probe_cell_center = FspCalcCellCenterWs(cascade_index, cascade_local_cell_index);
    const float half_cell_size = cb_instant_rdv.fsp_cascade[cascade_index].grid.cell_size * 0.5;
    const float3 clamped_offset_ws = clamp(hint_sample_pos_ws - probe_cell_center, -half_cell_size.xxx, half_cell_size.xxx);

    const float3 to_cell = probe_cell_center - view_origin;
    const float to_cell_len_sq = dot(to_cell, to_cell);
    const float3 to_cell_dir = (to_cell_len_sq > 1e-6) ? (to_cell * rsqrt(to_cell_len_sq)) : float3(0.0, 0.0, 1.0);
    const float half_diag = half_cell_size * 1.73205080757;
    const float frontness = dot(clamped_offset_ws, to_cell_dir);
    const float metric_norm = saturate((frontness + half_diag) / max(2.0 * half_diag, 1e-6));
    const uint metric_q = min((uint)(metric_norm * k_fsp_depth_hint_max_metric_q + 0.5), (uint)k_fsp_depth_hint_max_metric_q);
    const uint packed_pixel_id = pixel_id & k_fsp_depth_hint_pixel_mask;
    return (metric_q << k_fsp_depth_hint_metric_shift) | packed_pixel_id;
}

[numthreads(TILE_WIDTH, TILE_WIDTH, 1)]
void main_cs(
    uint3 dtid : SV_DispatchThreadID,
    uint3 gtid : SV_GroupThreadID,
    uint3 gid : SV_GroupID,
    uint gindex : SV_GroupIndex)
{
    const float3 view_origin = GetViewOriginFromInverseViewMatrix(cb_ngl_sceneview.cb_view_inv_mtx);
    const uint depth_width = uint(cb_instant_rdv.tex_main_view_depth_size.x);
    const uint pixel_id = dtid.y * depth_width + dtid.x;
    const float2 screen_pos_f = float2(dtid.xy) + float2(0.5, 0.5);
    const float2 screen_size_f = float2(cb_instant_rdv.tex_main_view_depth_size.xy);
    const float2 screen_uv = screen_pos_f / screen_size_f;

    // depth復元の有効スレッド判定.
    const bool is_valid_pixel = all(dtid.xy < cb_instant_rdv.tex_main_view_depth_size.xy);
    float3 pixel_pos_ws = 0.0.xxx;
    float3 surface_view_dir_ws = 0.0.xxx;
    bool has_valid_depth = false;
    bool has_surface_dir = false;
    if(is_valid_pixel)
    {
        const float d = TexHardwareDepth.Load(int3(dtid.xy, 0)).r;
        const float view_z = calc_view_z_from_ndc_z(d, cb_ngl_sceneview.cb_ndc_z_to_view_z_coef);
        if(65535.0 > abs(view_z))
        {
            has_valid_depth = true;
            const float3 to_pixel_ray_vs = CalcViewSpaceRay(screen_uv, cb_ngl_sceneview.cb_proj_mtx);
            pixel_pos_ws = mul(cb_ngl_sceneview.cb_view_inv_mtx, float4((to_pixel_ray_vs / abs(to_pixel_ray_vs.z)) * view_z, 1.0));
            const float3 to_surface_vec_ws = pixel_pos_ws - view_origin;
            const float to_surface_len_sq = dot(to_surface_vec_ws, to_surface_vec_ws);
            has_surface_dir = (to_surface_len_sq > 1e-6);
            surface_view_dir_ws = has_surface_dir ? (to_surface_vec_ws * rsqrt(to_surface_len_sq)) : 0.0.xxx;
        }
    }

    const uint cascade_count = FspCascadeCount();
    [unroll]
    for(uint cascade_index = 0; cascade_index < k_fsp_max_cascade_count; ++cascade_index)
    {
        if(cascade_index >= cascade_count)
        {
            break;
        }

        // グループ内ハッシュテーブル初期化.
        gs_cell_slot_index[gindex] = k_fsp_invalid_probe_index;
        gs_cell_slot_min_key[gindex] = k_fsp_depth_hint_metric_init;
        GroupMemoryBarrierWithGroupSync();

        const FspCascadeGridParam cascade = FspGetCascadeParam(cascade_index);
        const float half_cell_size = cascade.grid.cell_size * 0.5;
        const float hint_push_to_camera = half_cell_size * 0.08;
        const float3 hint_sample_pos_ws = has_surface_dir ? (pixel_pos_ws - surface_view_dir_ws * hint_push_to_camera) : pixel_pos_ws;

        uint global_cell_index = k_fsp_invalid_probe_index;
        const bool has_cell = has_valid_depth && FspTryGetGlobalCellIndexFromWorldPos(pixel_pos_ws, cascade_index, global_cell_index);
        if(has_cell)
        {
            const uint packed_key = FspBuildDepthHintPackedKey(cascade_index, global_cell_index, hint_sample_pos_ws, pixel_id, view_origin);
            const uint base_slot = (global_cell_index * 2654435761u) & (GROUP_SLOT_COUNT - 1u);
            bool merged_in_group = false;

            // 同一group内候補を同一cell単位で統合.
            [loop]
            for(uint probe_i = 0; probe_i < GROUP_HASH_PROBE_MAX; ++probe_i)
            {
                const uint slot = (base_slot + probe_i) & (GROUP_SLOT_COUNT - 1u);
                uint slot_prev_cell = k_fsp_invalid_probe_index;
                InterlockedCompareExchange(gs_cell_slot_index[slot], k_fsp_invalid_probe_index, global_cell_index, slot_prev_cell);
                if(slot_prev_cell == k_fsp_invalid_probe_index || slot_prev_cell == global_cell_index)
                {
                    InterlockedMin(gs_cell_slot_min_key[slot], packed_key);
                    merged_in_group = true;
                    break;
                }
            }

            // ハッシュ飽和時のみ直接globalへフォールバック.
            if(!merged_in_group)
            {
                uint prev_frame = 0;
                InterlockedExchange(RWFspCellVisibleFrameBuffer[global_cell_index], cb_instant_rdv.frame_count, prev_frame);
                uint prev_key = 0;
                InterlockedMin(RWFspCellDepthHintBuffer[global_cell_index], packed_key, prev_key);
            }
        }

        GroupMemoryBarrierWithGroupSync();

        // group内で統合済みスロットをglobalワークへ反映.
        if(gs_cell_slot_index[gindex] != k_fsp_invalid_probe_index)
        {
            const uint cell_index = gs_cell_slot_index[gindex];
            const uint packed_key = gs_cell_slot_min_key[gindex];
            uint prev_frame = 0;
            InterlockedExchange(RWFspCellVisibleFrameBuffer[cell_index], cb_instant_rdv.frame_count, prev_frame);
            uint prev_key = 0;
            InterlockedMin(RWFspCellDepthHintBuffer[cell_index], packed_key, prev_key);
        }

        GroupMemoryBarrierWithGroupSync();
    }
}
