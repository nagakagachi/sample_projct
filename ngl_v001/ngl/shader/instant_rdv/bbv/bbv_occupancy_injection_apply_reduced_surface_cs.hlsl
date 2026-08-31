/*
    bbv_occupancy_injection_apply_reduced_surface_cs.hlsl

    MainViewの縮小Surface SampleからBBV OccupancyとFSP SurfaceCellを同時生成する。

    Legacy (integrated_surface版) との違い:
    - 入力はフル解像度DepthではなくMainViewReducedSurfaceBufferBuildが生成した
      縮小テクスチャ (x=view Z, yz=world法線oct, w=法線信頼度)。
    - Dispatchも縮小解像度 (1/4 x 1/4 = 1/16スレッド数)。
    - 注入位置オフセットはView方向固定ではなく、法線信頼度で
      View方向と法線奥方向をブレンドする。
    - 時間方向のJitter巡回により16フレームで4x4全texelをカバーする。
*/

#define TILE_WIDTH 8

#include "../instant_rdv_util.hlsli"
#include "../../include/scene_view_struct.hlsli"

ConstantBuffer<BbvSurfaceInjectionViewInfo> cb_injection_src_view_info;
Texture2D<float4> TexReducedSurfaceBuffer;

[numthreads(TILE_WIDTH, TILE_WIDTH, 1)]
void main_cs(uint3 dtid : SV_DispatchThreadID)
{
    uint sample_width;
    uint sample_height;
    TexReducedSurfaceBuffer.GetDimensions(sample_width, sample_height);
    if(any(dtid.xy >= uint2(sample_width, sample_height)))
    {
        return;
    }

    // x=view Z, yz=world法線oct, w=法線信頼度。
    // view Z <= 0 は生成側が書いた無効Depth (スカイ等) のsentinel。
    const float4 surface_sample =
        TexReducedSurfaceBuffer.Load(int3(dtid.xy, 0));
    if(surface_sample.x <= 0.0)
    {
        return;
    }

    // 縮小バッファ生成時と同じJitter規則で元のフル解像度texelを逆算し、
    // 生成時のsample点と同一UVでサーフェイス位置を復元する。
    const uint2 source_resolution = uint2(
        cb_injection_src_view_info.cb_view_depth_buffer_offset_size.zw);
    const uint2 source_texel = ReducedSurfaceBufferSourceTexel(
        dtid.xy,
        source_resolution,
        cb_instant_rdv.frame_count);
    const float2 screen_uv =
        (float2(source_texel) + 0.5.xx) / float2(source_resolution);
    const float3 surface_pos_vs = CalcViewSpacePosition(
        screen_uv,
        surface_sample.x,
        cb_injection_src_view_info.cb_proj_mtx);
    const float3 surface_pos_ws = mul(
        cb_injection_src_view_info.cb_view_inv_mtx,
        float4(surface_pos_vs, 1.0));

    // 注入位置のオフセット方向を法線信頼度でブレンドする。
    //   信頼度1: 法線の奥方向 (-normal)。サーフェイス裏側のボクセルへ確実に入る。
    //   信頼度0: View奥方向へフォールバック (法線推定失敗時)。
    // Radiance注入 (reduced_surface版) も同じ方向・距離を使い、
    // OccupancyとRadianceが同一FineVoxelへ揃うことを保証する。
    const float surface_len_sq = dot(surface_pos_vs, surface_pos_vs);
    const float3 view_ray_vs = surface_len_sq > 1e-10
        ? surface_pos_vs * rsqrt(surface_len_sq)
        : float3(0.0, 0.0, 1.0);
    const float3 view_ray_ws = normalize(mul(
        cb_injection_src_view_info.cb_view_inv_mtx,
        float4(view_ray_vs, 0.0)));
    const float3 surface_normal_ws = normalize(
        OctDecode(surface_sample.yz));
    const float3 injection_dir_ws = normalize(lerp(
        view_ray_ws,
        -surface_normal_ws,
        saturate(surface_sample.w)));
    const float3 injection_pos_ws =
        surface_pos_ws +
        injection_dir_ws *
            cb_instant_rdv.bbv_occupancy_injection_world_offset;

    bool has_injection = false;
    uint voxel_index = 0u;
    uint bitmask_u32_offset = 0u;
    uint bitmask_append = 0u;
    const float3 voxel_coordf =
        (injection_pos_ws - cb_instant_rdv.bbv.grid_min_pos) *
        cb_instant_rdv.bbv.cell_size_inv;
    const int3 voxel_coord = floor(voxel_coordf);
    if(all(voxel_coord >= 0) &&
       all(voxel_coord < cb_instant_rdv.bbv.grid_resolution))
    {
        const int3 voxel_coord_toroidal = voxel_coord_toroidal_mapping(
            voxel_coord,
            cb_instant_rdv.bbv.grid_toroidal_offset,
            cb_instant_rdv.bbv.grid_resolution);
        voxel_index = BbvPhysicalVoxelCoordToMortonIndex(
            voxel_coord_toroidal,
            cb_instant_rdv.bbv.grid_resolution);
        const uint3 bitcell_coord = uint3(
            frac(voxel_coordf) * k_bbv_per_voxel_resolution);
        uint bitcell_bit_pos;
        calc_bbv_bitcell_info(
            bitmask_u32_offset,
            bitcell_bit_pos,
            bitcell_coord);
        bitmask_append = 1u << bitcell_bit_pos;
        has_injection = true;
    }

    // Wave内で同一 (voxel, maskワード) への書き込みを集約し、
    // 代表laneのみ InterlockedOr を実行してatomic回数を削減する。
    uint4 pending_lanes = WaveActiveBallot(has_injection);
    while(ballot_any(pending_lanes))
    {
        const uint leader_lane = first_lane_from_ballot(pending_lanes);
        const uint leader_voxel =
            WaveReadLaneAt(voxel_index, leader_lane);
        const uint leader_word =
            WaveReadLaneAt(bitmask_u32_offset, leader_lane);
        const bool is_same_target =
            has_injection &&
            voxel_index == leader_voxel &&
            bitmask_u32_offset == leader_word;
        const uint4 same_target_lanes =
            WaveActiveBallot(is_same_target);
        const uint merged_append = WaveActiveBitOr(
            is_same_target ? bitmask_append : 0u);
        if(WaveGetLaneIndex() == leader_lane)
        {
            InterlockedOr(
                RWBitmaskBrickVoxel[
                    bbv_voxel_bitmask_data_addr(leader_voxel) +
                    leader_word],
                merged_append);
        }
        pending_lanes &= ~same_target_lanes;
    }

    if(cb_instant_rdv.fsp_surface_mask_generation_enable == 0)
    {
        return;
    }

    // FSP SurfaceCellMask生成 (MainViewのみバインドされるため統合実行)。
    // FSPはオフセット前のDepthサーフェイス位置 (surface_pos_ws) を使う。
    // オフセット後の位置ではセルがサーフェイスからずれ、ActiveProbe集合が
    // Legacyと乖離するため。owner Cascade判定とboundary dither込みで
    // 最大2 Cascadeのmask addressへWave集約AtomicOrする。
    uint2 owner_mask_words = 0u.xx;
    uint2 owner_mask_bits = 0u.xx;
    const uint owner_count = FspGetSurfaceOwnerMaskAddresses(
        surface_pos_ws,
        owner_mask_words,
        owner_mask_bits);
    FspInjectCellMaskWave(
        owner_count > 0u,
        owner_mask_words.x,
        owner_mask_bits.x);
    FspInjectCellMaskWave(
        owner_count > 1u,
        owner_mask_words.y,
        owner_mask_bits.y);
}
