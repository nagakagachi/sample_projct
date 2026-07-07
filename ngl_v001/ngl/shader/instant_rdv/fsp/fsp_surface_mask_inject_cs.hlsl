#if 0
fsp_surface_mask_inject_cs.hlsl

FSP SurfacePass専用セルマスクの注入パス。
DepthBufferを8x8 dispatchで走査し、交差セルのbitをcascade別に立てる。

SurfaceMask path の中間パス。
BBV injection と同じ発想で「pixelごとにリストへappend」せず、
該当cellの1bitだけを立てる。複数pixelが同じcellを指すケースでは
同じbitへのORに収束するため、後段のcompactで一意なcellだけを処理できる。
BBV本体を汚染しないよう、FSP専用シェーダとして実装している。
#endif

#define TILE_WIDTH 8

#include "../instant_rdv_util.hlsli"
#include "../../include/scene_view_struct.hlsli"

ConstantBuffer<SceneViewInfo> cb_ngl_sceneview;
Texture2D TexHardwareDepth;

void FspInjectCellMaskWave(bool has_cell, uint global_cell_index)
{
    const uint word_index = has_cell ? (global_cell_index >> 5u) : 0u;
    const uint bit_mask = has_cell ? (1u << (global_cell_index & 31u)) : 0u;

    // Reduce UAV atomics by grouping active lanes that target the same uint word.
    // Without this, many neighboring pixels would issue independent InterlockedOr
    // operations to the same mask word. WaveActiveBitOr merges their bit masks,
    // and only the leader lane performs the final atomic OR for that word.
    uint4 pending_lanes = WaveActiveBallot(has_cell);
    while(ballot_any(pending_lanes))
    {
        const uint leader_lane = first_lane_from_ballot(pending_lanes);
        const uint leader_word_index = WaveReadLaneAt(word_index, leader_lane);
        const bool is_same_word = has_cell && (word_index == leader_word_index);
        const uint4 same_word_lanes = WaveActiveBallot(is_same_word);
        const uint merged_mask = WaveActiveBitOr(is_same_word ? bit_mask : 0u);

        if(WaveGetLaneIndex() == leader_lane)
        {
            InterlockedOr(RWFspSurfaceCellMaskBuffer[leader_word_index], merged_mask);
        }

        pending_lanes &= ~same_word_lanes;
    }
}

[numthreads(TILE_WIDTH, TILE_WIDTH, 1)]
void main_cs(
    uint3 dtid : SV_DispatchThreadID,
    uint3 gtid : SV_GroupThreadID,
    uint3 gid : SV_GroupID,
    uint gindex : SV_GroupIndex)
{
    if(any(dtid.xy >= cb_instant_rdv.tex_main_view_depth_size.xy))
    {
        return;
    }

    const float2 screen_pos_f = float2(dtid.xy) + 0.5.xx;
    const float2 screen_size_f = float2(cb_instant_rdv.tex_main_view_depth_size.xy);
    const float2 screen_uv = screen_pos_f / screen_size_f;

    const float d = TexHardwareDepth.Load(int3(dtid.xy, 0)).r;
    const float view_z = calc_view_z_from_ndc_z(d, cb_ngl_sceneview.cb_ndc_z_to_view_z_coef);
    if(65535.0 <= abs(view_z))
    {
        return;
    }

    const float3 to_pixel_ray_vs = CalcViewSpaceRay(screen_uv, cb_ngl_sceneview.cb_proj_mtx);
    const float3 pixel_pos_ws = mul(cb_ngl_sceneview.cb_view_inv_mtx, float4((to_pixel_ray_vs / abs(to_pixel_ray_vs.z)) * view_z, 1.0));

    // A single surface sample can belong to one cell per cascade. Mark all
    // cascades so the compact pass can later emit global cell indices in the
    // same address space used by the existing FSP update pipeline.
    const uint cascade_count = FspCascadeCount();
    [unroll]
    for(uint cascade_index = 0u; cascade_index < k_fsp_max_cascade_count; ++cascade_index)
    {
        if(cascade_index >= cascade_count)
        {
            break;
        }

        uint global_cell_index = 0u;
        const bool has_cell = FspTryGetGlobalCellIndexFromWorldPos(pixel_pos_ws, cascade_index, global_cell_index);
        FspInjectCellMaskWave(has_cell, global_cell_index);
    }
}
