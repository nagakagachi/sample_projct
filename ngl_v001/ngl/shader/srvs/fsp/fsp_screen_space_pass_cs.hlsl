
#if 0

fsp_screen_space_pass_cs.hlsl

ハードウェア深度バッファをもとにFrustumSurfaceProbeの処理をする.
可視サーフェイス上にあるFsp要素リストの生成.

#endif

#define TILE_WIDTH 16

#include "../srvs_util.hlsli"
// SceneView定数バッファ構造定義.
#include "../../include/scene_view_struct.hlsli"

ConstantBuffer<SceneViewInfo> cb_ngl_sceneview;
Texture2D			TexHardwareDepth;


// ThreadGroupタイル単位でスキップする最適化のグループタイル幅. 1より大きい数値で実行.
#define THREAD_GROUP_SKIP_OPTIMIZE_GROUP_TILE_WIDTH 1

static const uint k_fsp_depth_hint_metric_init = 0x7f7fffff;
// depth hint metric:
// - 値は「視点からサーフェスまでの距離^2 (to_surface_len_sq)」を uint 化したもの。
// - 小さいほど手前サーフェスなので、InterlockedMin で「セル内で最も手前」を代表値として残す。

// 同一フレーム内の重複を atomic_work で潰しながら visible cell list へ積む。
void FspRegisterVisibleCell(uint global_cell_index)
{
    // Visible判定フレーム番号を書き込み.
    uint old_atomic_work = 0;
    InterlockedExchange(RWFspCellStateBuffer[global_cell_index].atomic_work, cb_srvs.frame_count, old_atomic_work);

    // 交換前の値でVisible判定フレーム番号が現在フレームと異なるならリストへ登録. 別スレッドで同じVoxelを処理している場合の重複を除去する.
    if(cb_srvs.frame_count != old_atomic_work)
    {
        // 深度ヒント集約の比較値をフレーム先頭で初期化.
        RWFspCellStateBuffer[global_cell_index].probe_data_dummy = k_fsp_depth_hint_metric_init;

        int current_visible_count = 0;
        InterlockedAdd(RWSurfaceProbeCellList[0], 1, current_visible_count);
        if(cb_srvs.fsp_visible_voxel_buffer_size > current_visible_count)
        {
            // 追加可能であれば登録. 登録位置はindex0のカウンタを除いた位置.
            RWSurfaceProbeCellList[current_visible_count + 1] = global_cell_index;
        }
        else
        {
            // サイズオーバーの場合はカウンタを戻す.
            InterlockedAdd(RWSurfaceProbeCellList[0], -1);
        }
    }
}

void FspUpdateVisibleCellDepthHint(uint cascade_index, uint global_cell_index, float3 hint_sample_pos_ws, uint depth_metric_u)
{
    if(global_cell_index == k_fsp_invalid_probe_index)
    {
        return;
    }

    const uint cascade_local_cell_index = global_cell_index - cb_srvs.fsp_cascade[cascade_index].cell_offset;
    const float3 probe_cell_center = FspCalcCellCenterWs(cascade_index, cascade_local_cell_index);
    const float half_cell_size = cb_srvs.fsp_cascade[cascade_index].grid.cell_size * 0.5;
    const float3 clamped_offset_ws = clamp(hint_sample_pos_ws - probe_cell_center, -half_cell_size.xxx, half_cell_size.xxx);
    const uint encoded_hint_offset = encode_range1_vec3_to_uint(clamped_offset_ws / max(half_cell_size, 1e-6));

    uint prev_metric = 0;
    // 同一セルに対応する複数ピクセルから、最小 depth metric (最手前) だけを採用する。
    // 勝者ピクセルの hint offset を probe_offset_v3 に保持し、PreUpdate の初期配置に使う。
    InterlockedMin(RWFspCellStateBuffer[global_cell_index].probe_data_dummy, depth_metric_u, prev_metric);
    if(depth_metric_u < prev_metric)
    {
        RWFspCellStateBuffer[global_cell_index].probe_offset_v3 = encoded_hint_offset;
    }
}

void FspRegisterAndHintVisibleCellWave(
    uint cascade_index,
    uint global_cell_index,
    float3 hint_sample_pos_ws,
    uint depth_metric_u)
{
    // 1pixel=1セル方針で得た候補をWave内で統合してから書き込む。
    // - cell list 追加は leader lane のみ
    // - depth hint 更新は同一セル内の最小depth_metric lane のみ
    // これにより、可視セル登録とhint更新のAtomic競合を抑える。
    const bool has_cell = (global_cell_index != k_fsp_invalid_probe_index);
    if(!has_cell)
    {
        return;
    }

    uint4 pending_lanes = WaveActiveBallot(has_cell);
    while(ballot_any(pending_lanes))
    {
        const uint leader_lane = first_lane_from_ballot(pending_lanes);
        const uint leader_cell = WaveReadLaneAt(global_cell_index, leader_lane);
        const bool is_same_cell = has_cell && (global_cell_index == leader_cell);
        const uint4 same_cell_lanes = WaveActiveBallot(is_same_cell);
        const uint same_cell_depth_metric_u = is_same_cell ? depth_metric_u : k_fsp_depth_hint_metric_init;
        const uint leader_depth_metric_u = WaveActiveMin(same_cell_depth_metric_u);
        const bool is_depth_leader = is_same_cell && (same_cell_depth_metric_u == leader_depth_metric_u);
        const uint depth_leader_lane = first_lane_from_ballot(WaveActiveBallot(is_depth_leader));

        if(WaveGetLaneIndex() == leader_lane)
        {
            FspRegisterVisibleCell(leader_cell);
        }
        if(WaveGetLaneIndex() == depth_leader_lane)
        {
            FspUpdateVisibleCellDepthHint(cascade_index, global_cell_index, hint_sample_pos_ws, depth_metric_u);
        }

        pending_lanes &= ~same_cell_lanes;
    }
}

// DepthBufferに対してDispatch.
[numthreads(TILE_WIDTH, TILE_WIDTH, 1)]
void main_cs(
	uint3 dtid	: SV_DispatchThreadID,
	uint3 gtid : SV_GroupThreadID,
	uint3 gid : SV_GroupID,
	uint gindex : SV_GroupIndex
)
{
	const float3 view_origin = GetViewOriginFromInverseViewMatrix(cb_ngl_sceneview.cb_view_inv_mtx);

	const float2 screen_pos_f = float2(dtid.xy) + float2(0.5, 0.5);// ピクセル中心への半ピクセルオフセット考慮.
	const float2 screen_size_f = float2(cb_srvs.tex_main_view_depth_size.xy);
	const float2 screen_uv = (screen_pos_f / screen_size_f);

    #if 1 < THREAD_GROUP_SKIP_OPTIMIZE_GROUP_TILE_WIDTH
        // 適当なTile単位処理スキップ軽量化.
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


    // Tile単位で処理やAtomic書き出しをまとめることで効率化可能なはず.

    float d = TexHardwareDepth.Load(int3(dtid.xy, 0)).r;
    float view_z = calc_view_z_from_ndc_z(d, cb_ngl_sceneview.cb_ndc_z_to_view_z_coef);

    // Skyチェック.
    if(65535.0 <= abs(view_z))
        return;

    // 深度->PixelWorldPosition
    const float3 to_pixel_ray_vs = CalcViewSpaceRay(screen_uv, cb_ngl_sceneview.cb_proj_mtx);
    const float3 pixel_pos_ws = mul(cb_ngl_sceneview.cb_view_inv_mtx, float4((to_pixel_ray_vs/abs(to_pixel_ray_vs.z)) * view_z, 1.0));

    const float3 to_surface_vec_ws = pixel_pos_ws - view_origin;
    const float to_surface_len_sq = dot(to_surface_vec_ws, to_surface_vec_ws);
    // depth metric を uint 比較用に前計算して使い回す（AtomicMin / WaveMin 用）。
    const uint to_surface_len_sq_u = asuint(to_surface_len_sq);
    const bool has_surface_dir = (to_surface_len_sq > 1e-6);
    const float3 surface_view_dir_ws = has_surface_dir ? (to_surface_vec_ws * rsqrt(to_surface_len_sq)) : 0.0.xxx;

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
        const float3 hint_sample_pos_ws = has_surface_dir ? (pixel_pos_ws - surface_view_dir_ws * hint_push_to_camera) : pixel_pos_ws;

        // 全ピクセル処理を前提に、深度位置を含むセル（mid）だけを列挙する。
        // near/far候補は使わず、抽出段は最小コストの列挙に限定。
        // サーフェイス手前寄せ/埋まり回避の品質は pre-update 側で担保する。
        uint global_cell_index = k_fsp_invalid_probe_index;
        FspTryGetGlobalCellIndexFromWorldPos(pixel_pos_ws, cascade_index, global_cell_index);
        FspRegisterAndHintVisibleCellWave(cascade_index, global_cell_index, hint_sample_pos_ws, to_surface_len_sq_u);
    }
}
