#if 0

fsp_visible_anchor_finalize_cs.hlsl

DepthHint winner key からセルごとの可視アンカー情報を確定する。
Pass1(screen-space): winner key only
Pass2(this pass): winner payload only

#endif

#define TILE_WIDTH 16

#include "../srvs_util.hlsli"
#include "../../include/scene_view_struct.hlsli"

ConstantBuffer<SceneViewInfo> cb_ngl_sceneview;
Texture2D         TexHardwareDepth;

static const uint k_fsp_depth_hint_metric_init = 0xffffffffu;
static const uint k_fsp_depth_hint_pixel_bits = 24u;
static const uint k_fsp_depth_hint_metric_shift = k_fsp_depth_hint_pixel_bits;
static const uint k_fsp_depth_hint_pixel_mask = (1u << k_fsp_depth_hint_pixel_bits) - 1u;
static const float k_fsp_depth_hint_max_metric_q = 254.0;

uint FspBuildDepthHintPackedKey(
    uint cascade_index,
    uint global_cell_index,
    float3 hint_sample_pos_ws,
    uint pixel_id,
    float3 view_origin)
{
    const uint cascade_local_cell_index = global_cell_index - cb_srvs.fsp_cascade[cascade_index].cell_offset;
    const float3 probe_cell_center = FspCalcCellCenterWs(cascade_index, cascade_local_cell_index);
    const float half_cell_size = cb_srvs.fsp_cascade[cascade_index].grid.cell_size * 0.5;
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
	uint3 dtid	: SV_DispatchThreadID,
	uint3 gtid : SV_GroupThreadID,
	uint3 gid : SV_GroupID,
	uint gindex : SV_GroupIndex
)
{
    const float3 view_origin = GetViewOriginFromInverseViewMatrix(cb_ngl_sceneview.cb_view_inv_mtx);

    const float2 screen_pos_f = float2(dtid.xy) + float2(0.5, 0.5);
    const float2 screen_size_f = float2(cb_srvs.tex_main_view_depth_size.xy);
    const float2 screen_uv = (screen_pos_f / screen_size_f);
    const uint pixel_id = dtid.y * uint(cb_srvs.tex_main_view_depth_size.x) + dtid.x;

    const float depth = TexHardwareDepth.Load(int3(dtid.xy, 0)).r;
    const float view_z = calc_view_z_from_ndc_z(depth, cb_ngl_sceneview.cb_ndc_z_to_view_z_coef);
    if(65535.0 <= abs(view_z))
    {
        return;
    }

    const float3 to_pixel_ray_vs = CalcViewSpaceRay(screen_uv, cb_ngl_sceneview.cb_proj_mtx);
    const float3 pixel_pos_ws = mul(cb_ngl_sceneview.cb_view_inv_mtx, float4((to_pixel_ray_vs/abs(to_pixel_ray_vs.z)) * view_z, 1.0));

    const float3 to_surface_vec_ws = pixel_pos_ws - view_origin;
    const float to_surface_len_sq = dot(to_surface_vec_ws, to_surface_vec_ws);
    if(to_surface_len_sq <= 1e-6)
    {
        return;
    }
    const float3 surface_view_dir_ws = to_surface_vec_ws * rsqrt(to_surface_len_sq);

    const uint cascade_count = FspCascadeCount();
    [unroll]
    for(uint cascade_index = 0; cascade_index < k_fsp_max_cascade_count; ++cascade_index)
    {
        if(cascade_index >= cascade_count)
        {
            break;
        }

        const FspCascadeGridParam cascade = FspGetCascadeParam(cascade_index);
        const float half_cell_size = cascade.grid.cell_size * 0.5;
        const float hint_push_to_camera = half_cell_size * 0.08;
        const float3 hint_sample_pos_ws = pixel_pos_ws - surface_view_dir_ws * hint_push_to_camera;

        uint global_cell_index = k_fsp_invalid_probe_index;
        FspTryGetGlobalCellIndexFromWorldPos(pixel_pos_ws, cascade_index, global_cell_index);
        if(global_cell_index == k_fsp_invalid_probe_index)
        {
            continue;
        }

        const uint winner_key = FspCellStateBuffer[global_cell_index].depth_hint_packed_key;
        if(winner_key == k_fsp_depth_hint_metric_init)
        {
            continue;
        }

        const uint candidate_key = FspBuildDepthHintPackedKey(cascade_index, global_cell_index, hint_sample_pos_ws, pixel_id, view_origin);
        if(candidate_key != winner_key)
        {
            continue;
        }

        uint old_frame = 0;
        InterlockedExchange(RWFspVisibleAnchorBuffer[global_cell_index].atomic_frame, cb_srvs.frame_count, old_frame);
        if(old_frame == cb_srvs.frame_count)
        {
            continue;
        }

        FspVisibleAnchorData anchor = (FspVisibleAnchorData)0;
        anchor.surface_pos_ws = pixel_pos_ws;
        anchor.surface_view_z = view_z;
        anchor.surface_view_dir_ws = surface_view_dir_ws;
        anchor.winner_key = winner_key;
        anchor.valid = 1u;
        anchor.atomic_frame = cb_srvs.frame_count;
        RWFspVisibleAnchorBuffer[global_cell_index] = anchor;
    }
}
