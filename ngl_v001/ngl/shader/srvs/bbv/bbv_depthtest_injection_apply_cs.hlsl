
#if 0

bbv_depthtest_injection_apply_cs.hlsl

DepthTest ベース更新向けの Legacy 相当 Injection。
Legacy Injection と独立した専用シェーダとして維持し、
DepthTest 側の挙動調整が Legacy 側へ影響しないようにする。

#endif

#define TILE_WIDTH 8

#include "../srvs_util.hlsli"
// SceneView定数バッファ構造定義.
#include "../../include/scene_view_struct.hlsli"

// Injection元のDepthDeputhBufferのView情報.
ConstantBuffer<BbvSurfaceInjectionViewInfo> cb_injection_src_view_info;

Texture2D			TexHardwareDepth;


// ThreadGroupタイル単位でスキップする最適化のグループタイル幅. 1より大きい数値で実行.
#define THREAD_GROUP_SKIP_OPTIMIZE_GROUP_TILE_WIDTH 0

// DepthBufferに対してDispatch.
[numthreads(TILE_WIDTH, TILE_WIDTH, 1)]
void main_cs(
	uint3 dtid	: SV_DispatchThreadID,
	uint3 gtid : SV_GroupThreadID,
	uint3 gid : SV_GroupID,
	uint gindex : SV_GroupIndex
)
{
    // 範囲チェック.
    if(any(dtid.xy >= cb_injection_src_view_info.cb_view_depth_buffer_offset_size.zw))
    {
        return;
    }

    #if 1 < THREAD_GROUP_SKIP_OPTIMIZE_GROUP_TILE_WIDTH
        // Tile単位処理スキップ軽量化.
        const uint skip_tile_size = THREAD_GROUP_SKIP_OPTIMIZE_GROUP_TILE_WIDTH;// SxS個のタイルグループ毎に1Fに1タイルだけ処理するシンプル軽量化.
        const uint tile_skip_id_x = gid.x%skip_tile_size;
        const uint tile_skip_id_y = gid.y%skip_tile_size;
        const uint skip_frame_id = cb_srvs.frame_count % (skip_tile_size*skip_tile_size);
        const uint skip_frame_id_y = skip_frame_id / (skip_tile_size);
        const uint skip_frame_id_x = skip_frame_id % (skip_tile_size);
        if((tile_skip_id_x != skip_frame_id_x) || (tile_skip_id_y != skip_frame_id_y))
        {
            return;
        }
    #endif

    // ハードウェア深度取得. AtlasTexture対応のためオフセット考慮.
    float d = TexHardwareDepth.Load(int3(dtid.xy + cb_injection_src_view_info.cb_view_depth_buffer_offset_size.xy, 0)).r;

    bool has_injection = false;
    uint voxel_index = 0;
    uint bitmask_u32_offset = 0;
    uint bitmask_append = 0;
    
    // 無限遠ピクセルのチェック. ReverseZを考慮して0 < d < 1.
    if(0.0 < d && d < 1.0)
    {
        // DepthBufferに紐づいたView情報で復元.
        float view_z = calc_view_z_from_ndc_z(d, cb_injection_src_view_info.cb_ndc_z_to_view_z_coef);
        // 深度->PixelWorldPosition
        // DepthBufferに紐づいたView情報で復元.
        const float2 screen_uv = (float2(dtid.xy) + float2(0.5, 0.5)) / float2(cb_injection_src_view_info.cb_view_depth_buffer_offset_size.zw);
        // Orthoも含めて対応するためPositionを直接復元.
        float3 pixel_pos_vs = CalcViewSpacePosition(screen_uv, view_z, cb_injection_src_view_info.cb_proj_mtx);

        // near平面上の同一ピクセル点を始点にすると、
        // Perspectiveでは放射状、Orthoでは平行方向のレイが得られる。
        const float3 view_ray_origin_vs = CalcViewSpacePosition(screen_uv, cb_injection_src_view_info.cb_near_plane_view_z, cb_injection_src_view_info.cb_proj_mtx);
        const float3 to_pixel_vec_vs = pixel_pos_vs - view_ray_origin_vs;
        const float to_pixel_len_sq = dot(to_pixel_vec_vs, to_pixel_vec_vs);
        const float inv_to_pixel_len = rsqrt(max(to_pixel_len_sq, 1e-10));
        // 表面注入がRemovalで落ちやすいケース向けに、固定ワールド距離だけ視線奥へシフト.
        pixel_pos_vs += (to_pixel_vec_vs * inv_to_pixel_len) * cb_srvs.bbv_depthtest_injection_world_offset;

        const float3 pixel_pos_ws = mul(cb_injection_src_view_info.cb_view_inv_mtx, float4(pixel_pos_vs, 1.0));


        // PixelWorldPosition->VoxelCoord
        const float3 voxel_coordf = (pixel_pos_ws - cb_srvs.bbv.grid_min_pos) * cb_srvs.bbv.cell_size_inv;
        const int3 voxel_coord = floor(voxel_coordf);
        if(all(voxel_coord >= 0) && all(voxel_coord < cb_srvs.bbv.grid_resolution))
        {
            const int3 voxel_coord_toroidal = voxel_coord_toroidal_mapping(voxel_coord, cb_srvs.bbv.grid_toroidal_offset, cb_srvs.bbv.grid_resolution);
            voxel_index = voxel_coord_to_index(voxel_coord_toroidal, cb_srvs.bbv.grid_resolution);
            {
                // 占有ビットマスク.
                const float3 voxel_coord_frac = frac(voxel_coordf);
                const uint3 voxel_coord_bitmask_pos = uint3(voxel_coord_frac * k_bbv_per_voxel_resolution);
                uint bitcell_u32_bit_pos;
                calc_bbv_bitcell_info(bitmask_u32_offset, bitcell_u32_bit_pos, voxel_coord_bitmask_pos);
                bitmask_append = (1u << bitcell_u32_bit_pos);
                has_injection = true;
            }
        }
    }

    // Wave内で同一 (voxel_index, bitmask_u32_offset) への書き込みを統合し、
    // 代表laneのみ AtomicOr を発行して競合を抑える。
    uint4 pending_lanes = WaveActiveBallot(has_injection);
    while(ballot_any(pending_lanes))
    {
        const uint leader_lane = first_lane_from_ballot(pending_lanes);
        const uint leader_voxel = WaveReadLaneAt(voxel_index, leader_lane);
        const uint leader_u32_offset = WaveReadLaneAt(bitmask_u32_offset, leader_lane);

        const bool is_same_target = has_injection &&
            (voxel_index == leader_voxel) &&
            (bitmask_u32_offset == leader_u32_offset);
        const uint4 same_target_lanes = WaveActiveBallot(is_same_target);
        const uint merged_append = WaveActiveBitOr(is_same_target ? bitmask_append : 0u);

        if(WaveGetLaneIndex() == leader_lane)
        {
            const uint bbv_addr = bbv_voxel_bitmask_data_addr(leader_voxel);
            InterlockedOr(RWBitmaskBrickVoxel[bbv_addr + leader_u32_offset], merged_append);
        }

        pending_lanes &= ~same_target_lanes;
    }
}
