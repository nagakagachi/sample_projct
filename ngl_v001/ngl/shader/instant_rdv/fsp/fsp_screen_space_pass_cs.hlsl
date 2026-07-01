
#if 0

fsp_screen_space_pass_cs.hlsl

ハードウェア深度バッファをもとにFrustumSurfaceProbeの処理をする.
可視サーフェイス上にあるFsp要素リストの生成.

#endif

#define TILE_WIDTH 16

#include "../instant_rdv_util.hlsli"
// SceneView定数バッファ構造定義.
#include "../../include/scene_view_struct.hlsli"

ConstantBuffer<SceneViewInfo> cb_ngl_sceneview;
Texture2D			TexHardwareDepth;


// ThreadGroupタイル単位でスキップする最適化のグループタイル幅. 1より大きい数値で実行.
#define THREAD_GROUP_SKIP_OPTIMIZE_GROUP_TILE_WIDTH 1

static const uint k_fsp_depth_hint_metric_init = 0xffffffffu;
// packed depth-hint key layout (32bit):
// [31:24] metric_q  (0..254 を使用。255 は sentinel衝突回避)
// [23:00] pixel_id  (x + y * width, 最大 2^24-1)
static const uint k_fsp_depth_hint_pixel_bits = 24u;
static const uint k_fsp_depth_hint_metric_shift = k_fsp_depth_hint_pixel_bits;
static const uint k_fsp_depth_hint_pixel_mask = (1u << k_fsp_depth_hint_pixel_bits) - 1u;
static const float k_fsp_depth_hint_max_metric_q = 254.0; // 255 は sentinel(0xffffffff)衝突回避のため未使用.
// depth hint metric:
// - 上位bitにセル内 frontness の量子化 metric、下位bitに pixel id を詰める。
// - InterlockedMin 1回で、最小metric + 同値時の最小pixel id を同時確定する。

uint FspBuildDepthHintPackedKey(
    uint cascade_index,
    uint global_cell_index,
    float3 hint_sample_pos_ws,
    uint pixel_id,
    float3 view_origin)
{
    // カスケードごとのセルサイズ/セル中心で正規化するため、
    // カスケード間で metric の意味が崩れない。
    const uint cascade_local_cell_index = global_cell_index - cb_instant_rdv.fsp_cascade[cascade_index].cell_offset;
    const float3 probe_cell_center = FspCalcCellCenterWs(cascade_index, cascade_local_cell_index);
    const float half_cell_size = cb_instant_rdv.fsp_cascade[cascade_index].grid.cell_size * 0.5;
    const float3 clamped_offset_ws = clamp(hint_sample_pos_ws - probe_cell_center, -half_cell_size.xxx, half_cell_size.xxx);

    const float3 to_cell = probe_cell_center - view_origin;
    const float to_cell_len_sq = dot(to_cell, to_cell);
    const float3 to_cell_dir = (to_cell_len_sq > 1e-6) ? (to_cell * rsqrt(to_cell_len_sq)) : float3(0.0, 0.0, 1.0);
    const float half_diag = half_cell_size * 1.73205080757; // half * sqrt(3)
    const float frontness = dot(clamped_offset_ws, to_cell_dir);
    // frontness をセル内レンジ[-half_diag,+half_diag]へ正規化し、8bit量子化して比較キー化。
    // InterlockedMin の比較単調性を保つため、手前ほど小さい値になるように定義する。
    const float metric_norm = saturate((frontness + half_diag) / max(2.0 * half_diag, 1e-6));
    const uint metric_q = min((uint)(metric_norm * k_fsp_depth_hint_max_metric_q + 0.5), (uint)k_fsp_depth_hint_max_metric_q);
    const uint packed_pixel_id = pixel_id & k_fsp_depth_hint_pixel_mask;
    return (metric_q << k_fsp_depth_hint_metric_shift) | packed_pixel_id;
}

// 同一フレーム内の重複を atomic_work で潰しながら visible cell list へ積む。
void FspRegisterVisibleCell(uint global_cell_index)
{
    // Visible判定フレーム番号を書き込み.
    uint old_atomic_work = 0;
    InterlockedExchange(RWFspCellStateBuffer[global_cell_index].atomic_work, cb_instant_rdv.frame_count, old_atomic_work);

    // 交換前の値でVisible判定フレーム番号が現在フレームと異なるならリストへ登録. 別スレッドで同じVoxelを処理している場合の重複を除去する.
    if(cb_instant_rdv.frame_count != old_atomic_work)
    {
        // 3-pass実装と同じ可視スタンプを残し、デバッグ切替時の整合を保つ。
        RWFspCellVisibleFrameBuffer[global_cell_index] = cb_instant_rdv.frame_count;

        // 深度ヒント集約の比較値をフレーム先頭で初期化.
        RWFspCellStateBuffer[global_cell_index].depth_hint_packed_key = k_fsp_depth_hint_metric_init;
        // 3-pass側ワークと同値にして、後段参照先を統一できるようにする。
        RWFspCellDepthHintBuffer[global_cell_index] = k_fsp_depth_hint_metric_init;

        int current_visible_count = 0;
        InterlockedAdd(RWSurfaceProbeCellList[0], 1, current_visible_count);
        if(cb_instant_rdv.fsp_visible_voxel_buffer_size > current_visible_count)
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

void FspUpdateVisibleCellDepthHint(uint global_cell_index, uint packed_depth_hint_key_u)
{
    if(global_cell_index == k_fsp_invalid_probe_index)
    {
        return;
    }
    // depth_hint_packed_key には packed key だけを保持する。
    // 実オフセットは PreUpdate で pixel_id -> depth 再構成して生成する。
    uint prev_packed = 0;
    InterlockedMin(RWFspCellStateBuffer[global_cell_index].depth_hint_packed_key, packed_depth_hint_key_u, prev_packed);
    // 1-pass/3-pass の両経路で同じ depth hint ワークを更新する。
    InterlockedMin(RWFspCellDepthHintBuffer[global_cell_index], packed_depth_hint_key_u, prev_packed);
}

void FspRegisterAndHintVisibleCellWave(
    uint cascade_index,
    uint global_cell_index,
    float3 hint_sample_pos_ws,
    uint pixel_id,
    float3 view_origin)
{
    // 1pixel=1セル方針で得た候補をWave内で統合してから書き込む。
    // - cell list 追加は leader lane のみ
    // - depth hint 更新は同一セル内の最小packed-key lane のみ
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
        // Wave内は同一セル候補だけで最小packed keyを先に求め、Atomic回数を1回へ抑える。
        const uint same_cell_packed_key = is_same_cell
            ? FspBuildDepthHintPackedKey(cascade_index, global_cell_index, hint_sample_pos_ws, pixel_id, view_origin)
            : k_fsp_depth_hint_metric_init;
        const uint leader_packed_key = WaveActiveMin(same_cell_packed_key);
        const bool is_depth_leader = is_same_cell && (same_cell_packed_key == leader_packed_key);
        const uint depth_leader_lane = first_lane_from_ballot(WaveActiveBallot(is_depth_leader));

        if(WaveGetLaneIndex() == leader_lane)
        {
            FspRegisterVisibleCell(leader_cell);
        }
        if(WaveGetLaneIndex() == depth_leader_lane)
        {
            FspUpdateVisibleCellDepthHint(global_cell_index, leader_packed_key);
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
    // Dispatch端数スレッドの無効アクセスを防ぐ。
    if(any(dtid.xy >= cb_instant_rdv.tex_main_view_depth_size.xy))
    {
        return;
    }

	const float3 view_origin = GetViewOriginFromInverseViewMatrix(cb_ngl_sceneview.cb_view_inv_mtx);

	const float2 screen_pos_f = float2(dtid.xy) + float2(0.5, 0.5);// ピクセル中心への半ピクセルオフセット考慮.
	const float2 screen_size_f = float2(cb_instant_rdv.tex_main_view_depth_size.xy);
	const float2 screen_uv = (screen_pos_f / screen_size_f);
	// pixel_id は packed key 下位24bitへ入る。
	// 24bitを超える解像度では下位bitへ丸められる点に注意（一般的な解像度では問題なし）。
	const uint pixel_id = dtid.y * uint(cb_instant_rdv.tex_main_view_depth_size.x) + dtid.x;

    #if 1 < THREAD_GROUP_SKIP_OPTIMIZE_GROUP_TILE_WIDTH
        // 適当なTile単位処理スキップ軽量化.
        const uint skip_tile_size = THREAD_GROUP_SKIP_OPTIMIZE_GROUP_TILE_WIDTH;// SxS個のタイルグループ毎に1Fに1タイルだけ処理するシンプル軽量化.
        const uint tile_skip_id_x = gid.x%skip_tile_size;
        const uint tile_skip_id_y = gid.y%skip_tile_size;

        const uint skip_frame_id = cb_instant_rdv.frame_count % (skip_tile_size*skip_tile_size);
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
        FspRegisterAndHintVisibleCellWave(cascade_index, global_cell_index, hint_sample_pos_ws, pixel_id, view_origin);
    }
}
