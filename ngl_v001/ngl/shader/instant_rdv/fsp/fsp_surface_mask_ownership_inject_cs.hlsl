#if 0

fsp_surface_mask_ownership_inject_cs.hlsl

FSP SurfaceMaskのOwnership注入パス。
Depth pixelを全cascadeへ書かず、最も細かい所有cascadeへだけ注入する。
Lightingがcoarseを選択し得る境界帯では、欠損防止のため隣接2cascadeへ注入する。

#endif

#define FSP_SURFACE_MASK_OWNERSHIP_TILE_WIDTH 8

#include "../instant_rdv_util.hlsli"
#include "../../include/scene_view_struct.hlsli"

ConstantBuffer<SceneViewInfo> cb_ngl_sceneview;
Texture2D TexHardwareDepth;

// wave内で同じuint wordを指すbitをまとめ、global AtomicOrを代表laneだけに限定する。
void FspInjectOwnershipCellMaskWave(bool has_cell, uint global_cell_index)
{
    // global_cell_index is the shared FSP X-major address used by ActiveProbe and IrradianceVolume.
    const uint word_index = has_cell ? (global_cell_index >> 5u) : 0u;
    const uint bit_mask = has_cell ? (1u << (global_cell_index & 31u)) : 0u;

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

[numthreads(
    FSP_SURFACE_MASK_OWNERSHIP_TILE_WIDTH,
    FSP_SURFACE_MASK_OWNERSHIP_TILE_WIDTH,
    1)]
void main_cs(
    uint3 dtid : SV_DispatchThreadID,
    uint3 gtid : SV_GroupThreadID,
    uint3 gid : SV_GroupID,
    uint gindex : SV_GroupIndex)
{
    const bool is_valid_pixel = all(dtid.xy < cb_instant_rdv.tex_main_view_depth_size.xy);
    float3 pixel_pos_ws = 0.0.xxx;
    bool has_surface = false;

    if(is_valid_pixel)
    {
        const float depth = TexHardwareDepth.Load(int3(dtid.xy, 0)).r;
        const float view_z = calc_view_z_from_ndc_z(
            depth,
            cb_ngl_sceneview.cb_ndc_z_to_view_z_coef);
        has_surface = abs(view_z) < 65535.0;
        if(has_surface)
        {
            const float2 screen_pos = float2(dtid.xy) + 0.5.xx;
            const float2 screen_size = float2(cb_instant_rdv.tex_main_view_depth_size.xy);
            const float2 screen_uv = screen_pos / screen_size;
            const float3 to_pixel_ray_vs = CalcViewSpaceRay(
                screen_uv,
                cb_ngl_sceneview.cb_proj_mtx);
            pixel_pos_ws = mul(
                cb_ngl_sceneview.cb_view_inv_mtx,
                float4((to_pixel_ray_vs / abs(to_pixel_ray_vs.z)) * view_z, 1.0));
        }
    }

    uint2 owner_cell_indices = k_fsp_invalid_probe_index.xx;
    const uint owner_cell_count = has_surface
        ? FspGetSurfaceOwnerCells(pixel_pos_ws, owner_cell_indices)
        : 0u;

    // wave intrinsicを全laneが同じ回数実行するため、最大2セルを固定回数で処理する。
    [unroll]
    for(uint owner_index = 0u; owner_index < 2u; ++owner_index)
    {
        const bool has_owner_cell = owner_index < owner_cell_count;
        FspInjectOwnershipCellMaskWave(
            has_owner_cell,
            has_owner_cell ? owner_cell_indices[owner_index] : 0u);
    }
}
