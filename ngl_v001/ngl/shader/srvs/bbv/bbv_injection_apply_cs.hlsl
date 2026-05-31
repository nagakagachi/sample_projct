
#if 0

bbv_injection_apply_cs.hlsl

深度バッファをもとに可視表面のVoxel情報をBbvに注入する.
また, フレームでの可視Voxel処理用リストの生成.

ViewとしてはPerspectiveなMainViewに加えてShadowMapViewも同一シェーダでInjectionしたい.

#endif

#define TILE_WIDTH 16

#include "../srvs_util.hlsli"
// SceneView定数バッファ構造定義.
#include "../../include/scene_view_struct.hlsli"

// MainViewの情報.
ConstantBuffer<SceneViewInfo> cb_ngl_sceneview;

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
        float3 pixel_pos_ws = mul(cb_injection_src_view_info.cb_view_inv_mtx, float4(CalcViewSpacePosition(screen_uv, view_z, cb_injection_src_view_info.cb_proj_mtx), 1.0));

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

    // Wave内で同一 (voxel_index, bitmask_u32_offset) 宛ての更新を統合し、
    // 代表laneのみ AtomicOr を発行して衝突を抑える。
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
            // Brick / HiBrick count は後段の集計パスで再構築するため、ここでは bitmask 更新だけ行う。
            InterlockedOr(RWBitmaskBrickVoxel[bbv_addr + leader_u32_offset], merged_append);
        }

        pending_lanes &= ~same_target_lanes;
    }
}
