#if 0

fsp_depth_hint_finalize_cs.hlsl

VisibleSurfacePass で確定した最小 depth metric を使い、
同値 metric 候補の中から global thread id 最小のサンプルを勝者にする.

#endif

#define TILE_WIDTH 16

#include "../srvs_util.hlsli"
#include "../../include/scene_view_struct.hlsli"

ConstantBuffer<SceneViewInfo> cb_ngl_sceneview;
Texture2D         TexHardwareDepth;

[numthreads(TILE_WIDTH, TILE_WIDTH, 1)]
void main_cs(uint3 dtid : SV_DispatchThreadID)
{
    if((dtid.x >= cb_srvs.tex_main_view_depth_size.x) || (dtid.y >= cb_srvs.tex_main_view_depth_size.y))
    {
        return;
    }

    float d = TexHardwareDepth.Load(int3(dtid.xy, 0)).r;
    float view_z = calc_view_z_from_ndc_z(d, cb_ngl_sceneview.cb_ndc_z_to_view_z_coef);
    if(65535.0 <= abs(view_z))
    {
        return;
    }

    const float3 view_origin = GetViewOriginFromInverseViewMatrix(cb_ngl_sceneview.cb_view_inv_mtx);
    const float2 screen_pos_f = float2(dtid.xy) + float2(0.5, 0.5);
    const float2 screen_size_f = float2(cb_srvs.tex_main_view_depth_size.xy);
    const float2 screen_uv = (screen_pos_f / screen_size_f);
    const float3 to_pixel_ray_vs = CalcViewSpaceRay(screen_uv, cb_ngl_sceneview.cb_proj_mtx);
    const float3 pixel_pos_ws = mul(cb_ngl_sceneview.cb_view_inv_mtx, float4((to_pixel_ray_vs / abs(to_pixel_ray_vs.z)) * view_z, 1.0));

    const float3 to_surface_vec_ws = pixel_pos_ws - view_origin;
    const float to_surface_len_sq = dot(to_surface_vec_ws, to_surface_vec_ws);
    if(to_surface_len_sq <= 1e-6)
    {
        return;
    }
    const uint depth_metric_u = asuint(to_surface_len_sq);
    const float3 surface_view_dir_ws = to_surface_vec_ws * rsqrt(to_surface_len_sq);
    const uint global_thread_linear_id = dtid.y * uint(cb_srvs.tex_main_view_depth_size.x) + dtid.x;

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

        if(RWFspCellStateBuffer[global_cell_index].probe_data_dummy != depth_metric_u)
        {
            continue;
        }

        const uint cascade_local_cell_index = global_cell_index - cb_srvs.fsp_cascade[cascade_index].cell_offset;
        const float3 probe_cell_center = FspCalcCellCenterWs(cascade_index, cascade_local_cell_index);
        const float3 clamped_offset_ws = clamp(hint_sample_pos_ws - probe_cell_center, -half_cell_size.xxx, half_cell_size.xxx);
        const uint encoded_hint_offset = encode_range1_vec3_to_uint(clamped_offset_ws / max(half_cell_size, 1e-6));

        uint prev_thread_id = 0xffffffffu;
        InterlockedMin(RWFspCellStateBuffer[global_cell_index].depth_hint_thread_id, global_thread_linear_id, prev_thread_id);
        if(global_thread_linear_id < prev_thread_id)
        {
            RWFspCellStateBuffer[global_cell_index].probe_offset_v3 = encoded_hint_offset;
        }
    }
}
