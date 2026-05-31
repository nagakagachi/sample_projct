
#if 0

bbv_removal_list_build_cs.hlsl

ハードウェア深度バッファより手前にある中空になったBbvCellを除去するための情報を生成する.

#endif

#define TILE_WIDTH 16

#include "../srvs_util.hlsli"
// SceneView定数バッファ構造定義.
#include "../../include/scene_view_struct.hlsli"

ConstantBuffer<SceneViewInfo> cb_ngl_sceneview;

// Injection元のDepthDeputhBufferのView情報.
ConstantBuffer<BbvSurfaceInjectionViewInfo> cb_injection_src_view_info;

Texture2D			TexHardwareDepth;


// ThreadGroupタイル単位でスキップする最適化のグループタイル幅. 1より大きい数値で実行.
#define THREAD_GROUP_SKIP_OPTIMIZE_GROUP_TILE_WIDTH 4

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
    const float d = TexHardwareDepth.Load(int3(dtid.xy + cb_injection_src_view_info.cb_view_depth_buffer_offset_size.xy, 0)).r;
    // 無限遠ピクセル(0/1近傍)は除去対象の視線終端が定まらないため処理しない。
    if(!(0.0 < d && d < 1.0))
    {
        return;
    }
    const float view_z = min(65535.0, calc_view_z_from_ndc_z(d, cb_injection_src_view_info.cb_ndc_z_to_view_z_coef));

    bool has_removal = false;
    uint voxel_index = 0;
    uint bitmask_u32_offset = 0;
    uint bitmask_append = 0;
    
    const float2 screen_uv = (float2(dtid.xy) + float2(0.5, 0.5)) / float2(cb_injection_src_view_info.cb_view_depth_buffer_offset_size.zw);
    // Orthoも含めて対応するためPositionを直接復元.
    const float3 pixel_pos_ws = mul(cb_injection_src_view_info.cb_view_inv_mtx, float4(CalcViewSpacePosition(screen_uv, view_z, cb_injection_src_view_info.cb_proj_mtx), 1.0));

    const float near_plane_view_z = cb_injection_src_view_info.cb_near_plane_view_z;
    const float3 view_ray_origin = mul(cb_injection_src_view_info.cb_view_inv_mtx, float4(CalcViewSpacePosition(screen_uv, near_plane_view_z, cb_injection_src_view_info.cb_proj_mtx), 1.0));

    const float3 to_pixel_vec_ws = pixel_pos_ws - view_ray_origin;
    const float3 ray_dir_ws = normalize(to_pixel_vec_ws);
    // 深度バッファの手前までレイトレース.
    // Note:適当な固定値ではなく, DDA相当の計算で1セル分バックトレースしたい
    const float trace_distance = dot(ray_dir_ws, to_pixel_vec_ws) - cb_srvs.bbv.cell_size*k_bbv_per_voxel_resolution_inv*0.9;
    if(trace_distance <= 0.0)
    {
        return;
    }


    int hit_voxel_index = -1;
    float4 debug_ray_info;
// Trace最適化検証.
#if NGL_SRVS_TRACE_USE_HIBRICK_BBV_REMOVAL_LIST_BUILD
    float4 curr_ray_t_ws = trace_bbv_hibrick(
        hit_voxel_index, debug_ray_info,
        view_ray_origin, ray_dir_ws, trace_distance,
        cb_srvs.bbv.grid_min_pos, cb_srvs.bbv.cell_size, cb_srvs.bbv.grid_resolution,
        cb_srvs.bbv.grid_toroidal_offset, BitmaskBrickVoxel);
#else
    float4 curr_ray_t_ws = trace_bbv(
        hit_voxel_index, debug_ray_info,
        view_ray_origin, ray_dir_ws, trace_distance, 
        cb_srvs.bbv.grid_min_pos, cb_srvs.bbv.cell_size, cb_srvs.bbv.grid_resolution,
        cb_srvs.bbv.grid_toroidal_offset, BitmaskBrickVoxel);
#endif

    if(0.0 <= curr_ray_t_ws.x)
    {
        const float3 hit_pos_ws = view_ray_origin + ray_dir_ws * (curr_ray_t_ws.x + 0.001);// ヒット位置は表面なので除去したいVoxelに侵入するためオフセット.

        // PixelWorldPosition->VoxelCoord
        const float3 voxel_coordf = (hit_pos_ws - cb_srvs.bbv.grid_min_pos) * cb_srvs.bbv.cell_size_inv;
        const int3 voxel_coord = floor(voxel_coordf);
        if(all(voxel_coord >= 0) && all(voxel_coord < cb_srvs.bbv.grid_resolution))
        {
            int3 voxel_coord_toroidal = voxel_coord_toroidal_mapping(voxel_coord, cb_srvs.bbv.grid_toroidal_offset, cb_srvs.bbv.grid_resolution);
            voxel_index = voxel_coord_to_index(voxel_coord_toroidal, cb_srvs.bbv.grid_resolution);

            {
                // 占有ビットマスク.
                const float3 voxel_coord_frac = frac(voxel_coordf);
                const uint3 voxel_coord_bitmask_pos = uint3(voxel_coord_frac * k_bbv_per_voxel_resolution);

                uint bitcell_u32_bit_pos;
                calc_bbv_bitcell_info(bitmask_u32_offset, bitcell_u32_bit_pos, voxel_coord_bitmask_pos);
                bitmask_append = (1u << bitcell_u32_bit_pos);
                has_removal = true;
            }
        }
    }

    // Wave単位で同一 (voxel_index, bitmask_u32_offset) をまとめ、
    // RemoveVoxelList への登録と Atomic を削減する。
    uint4 pending_lanes = WaveActiveBallot(has_removal);
    while(ballot_any(pending_lanes))
    {
        const uint leader_lane = first_lane_from_ballot(pending_lanes);
        const uint leader_voxel = WaveReadLaneAt(voxel_index, leader_lane);
        const uint leader_u32_offset = WaveReadLaneAt(bitmask_u32_offset, leader_lane);

        const bool is_same_target = has_removal &&
            (voxel_index == leader_voxel) &&
            (bitmask_u32_offset == leader_u32_offset);
        const uint4 same_target_lanes = WaveActiveBallot(is_same_target);
        const uint merged_append = WaveActiveBitOr(is_same_target ? bitmask_append : 0u);

        if(WaveGetLaneIndex() == leader_lane)
        {
            int current_visible_count;
            InterlockedAdd(RWRemoveVoxelList[0], 1, current_visible_count);
            if(cb_srvs.bbv_hollow_voxel_buffer_size > current_visible_count)
            {
                // 登録位置はindex0のカウンタを除いた位置(+1).
                const int target_index = (current_visible_count + 1) * k_component_count_RemoveVoxelList;
                RWRemoveVoxelList[(target_index)] = leader_voxel;
                RWRemoveVoxelList[(target_index) + 1] = leader_u32_offset;
                RWRemoveVoxelList[(target_index) + 2] = merged_append;
                RWRemoveVoxelList[(target_index) + 3] = 0;// 予備.
            }
            else
            {
                // サイズオーバーの場合はカウンタを戻す.
                InterlockedAdd(RWRemoveVoxelList[0], -1);
            }
        }

        pending_lanes &= ~same_target_lanes;
    }
}
