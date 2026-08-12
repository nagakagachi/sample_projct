/*
    bbv_depthtest_injection_apply_fsp_surface_cs.hlsl
    FSP表面セル検出を統合したBBV Depth Injection。

    MainViewのDepth pixelでBBV占有を書き込み、同じパスでFSP
    SurfaceCellMaskをマーキングする。ActiveProbe検出をLegacyの
    FSP Surface Passと同等に保つため、表面位置とオフセット後の
    Injection位置を分離し、BBV側にはLegacyと同じRemoval対策の
    オフセットを適用する。
*/

#define TILE_WIDTH 8

#include "../instant_rdv_util.hlsli"
#include "../../include/scene_view_struct.hlsli"

// BBVのDepth Injection元View情報。
// MainView判定とFSP Surface PassのCascade選択もこの定数バッファで受け取る。
ConstantBuffer<BbvSurfaceInjectionViewInfo> cb_injection_src_view_info;

// ShadowViewやDepth Atlasを含むInjection元のハードウェア深度。
Texture2D TexHardwareDepth;

// FSP SurfaceCellMaskは1bitが1つのGlobal Cellに対応する。
// 同一Wave内で同じuint wordを指すbitをまとめ、AtomicOrの回数を減らす。
void FspInjectCellMaskWave(bool has_cell, uint global_cell_index)
{
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

// BBV OccupancyへのInjectionと、MainView由来のFSP SurfaceCell検出を同時に行う。
// FSP側は表面位置、BBV側はRemoval対策オフセット後の位置を使用する。
[numthreads(TILE_WIDTH, TILE_WIDTH, 1)]
void main_cs(uint3 dtid : SV_DispatchThreadID)
{
    // Depth Atlasの対象範囲外は、BBVとFSPのどちらにも書き込まない。
    if(any(dtid.xy >= cb_injection_src_view_info.cb_view_depth_buffer_offset_size.zw))
    {
        return;
    }

    // DepthをView空間へ復元し、表面位置とBBV Injection位置の基準を作る。
    const float d = TexHardwareDepth.Load(int3(
        dtid.xy + cb_injection_src_view_info.cb_view_depth_buffer_offset_size.xy, 0)).r;
    const bool has_surface = 0.0 < d && d < 1.0;
    float3 surface_pos_ws = 0.0.xxx;
    float3 injection_pos_ws = 0.0.xxx;
    if(has_surface)
    {
        const float2 screen_uv =
            (float2(dtid.xy) + 0.5.xx) /
            float2(cb_injection_src_view_info.cb_view_depth_buffer_offset_size.zw);
        const float view_z = calc_view_z_from_ndc_z(
            d, cb_injection_src_view_info.cb_ndc_z_to_view_z_coef);
        float3 pixel_pos_vs = CalcViewSpacePosition(
            screen_uv, view_z, cb_injection_src_view_info.cb_proj_mtx);

        // FSPのSurfaceCell検出は、DepthBuffer上の実表面位置を使う。
        surface_pos_ws = mul(
            cb_injection_src_view_info.cb_view_inv_mtx,
            float4(pixel_pos_vs, 1.0));

        // BBV InjectionはLegacyと同じく、表面から視線奥へ固定距離をずらす。
        // このオフセットがないと、後続のMainView Depth Carvingで
        // Injection直後のVoxelが表面手前として削除される。
        const float3 view_ray_origin_vs = CalcViewSpacePosition(
            screen_uv,
            cb_injection_src_view_info.cb_near_plane_view_z,
            cb_injection_src_view_info.cb_proj_mtx);
        const float3 to_pixel_vec_vs = pixel_pos_vs - view_ray_origin_vs;
        const float inv_to_pixel_len = rsqrt(max(dot(to_pixel_vec_vs, to_pixel_vec_vs), 1e-10));
        pixel_pos_vs +=
            (to_pixel_vec_vs * inv_to_pixel_len) *
            cb_instant_rdv.bbv_depthtest_injection_world_offset;

        injection_pos_ws = mul(
            cb_injection_src_view_info.cb_view_inv_mtx,
            float4(pixel_pos_vs, 1.0));
    }

    // Injection位置をBBVのBrick/FineVoxel座標へ変換する。
    // 無効Depthではhas_injectionがfalseのため、BBVへは書き込まれない。
    bool has_injection = false;
    uint voxel_index = 0u;
    uint bitmask_u32_offset = 0u;
    uint bitmask_append = 0u;
    const float3 voxel_coordf =
        (injection_pos_ws - cb_instant_rdv.bbv.grid_min_pos) *
        cb_instant_rdv.bbv.cell_size_inv;
    const int3 voxel_coord = floor(voxel_coordf);
    if(has_surface && all(voxel_coord >= 0) && all(voxel_coord < cb_instant_rdv.bbv.grid_resolution))
    {
        const int3 voxel_coord_toroidal = voxel_coord_toroidal_mapping(
            voxel_coord,
            cb_instant_rdv.bbv.grid_toroidal_offset,
            cb_instant_rdv.bbv.grid_resolution);
        voxel_index = BbvPhysicalVoxelCoordToMortonIndex(
            voxel_coord_toroidal, cb_instant_rdv.bbv.grid_resolution);
        const float3 voxel_coord_frac = frac(voxel_coordf);
        const uint3 voxel_coord_bitmask_pos =
            uint3(voxel_coord_frac * k_bbv_per_voxel_resolution);
        uint bitcell_u32_bit_pos;
        calc_bbv_bitcell_info(bitmask_u32_offset, bitcell_u32_bit_pos, voxel_coord_bitmask_pos);
        bitmask_append = 1u << bitcell_u32_bit_pos;
        has_injection = true;
    }

    // 同一Wave内で同じBBV bitmask wordを共有するpixelを集約する。
    // BBV Occupancyの意味はLegacy Injectionと同じまま、AtomicOrだけを削減する。
    uint4 pending_lanes = WaveActiveBallot(has_injection);
    while(ballot_any(pending_lanes))
    {
        const uint leader_lane = first_lane_from_ballot(pending_lanes);
        const uint leader_voxel = WaveReadLaneAt(voxel_index, leader_lane);
        const uint leader_u32_offset = WaveReadLaneAt(bitmask_u32_offset, leader_lane);
        const bool is_same_target = has_injection &&
            voxel_index == leader_voxel &&
            bitmask_u32_offset == leader_u32_offset;
        const uint4 same_target_lanes = WaveActiveBallot(is_same_target);
        const uint merged_append = WaveActiveBitOr(is_same_target ? bitmask_append : 0u);
        if(WaveGetLaneIndex() == leader_lane)
        {
            const uint bbv_addr = bbv_voxel_bitmask_data_addr(leader_voxel);
            InterlockedOr(RWBitmaskBrickVoxel[bbv_addr + leader_u32_offset], merged_append);
        }
        pending_lanes &= ~same_target_lanes;
    }

    // FSP Surface候補はMainViewだけから生成する。
    // Shadow/Sub ViewのDepthはBBV Geometryには使えても、
    // 通常のカメラ可視表面としてActiveProbeへ混ぜない。
    if(cb_injection_src_view_info.cb_is_main_view == 0)
    {
        return;
    }

    if(!has_surface)
    {
        return;
    }

    // Cascade選択はLegacyのSurface Passと同じ設定を使用する。
    // Ownershipでは最細Cascadeと境界帯の隣接Cascadeだけを対象にする。
    if(cb_injection_src_view_info.cb_fsp_surface_pass_mode != 0)
    {
        uint2 owner_cells = k_fsp_invalid_probe_index.xx;
        const uint owner_count = FspGetSurfaceOwnerCells(surface_pos_ws, owner_cells);
        FspInjectCellMaskWave(owner_count > 0u, owner_cells.x);
        FspInjectCellMaskWave(owner_count > 1u, owner_cells.y);
    }
    else
    {
        const uint cascade_count = FspCascadeCount();
        [unroll]
        for(uint cascade_index = 0u; cascade_index < k_fsp_max_cascade_count; ++cascade_index)
        {
            if(cascade_index >= cascade_count)
            {
                break;
            }
            uint global_cell_index = 0u;
            const bool has_cell = FspTryGetGlobalCellIndexFromWorldPos(
                surface_pos_ws, cascade_index, global_cell_index);
            FspInjectCellMaskWave(has_cell, global_cell_index);
        }
    }
}
