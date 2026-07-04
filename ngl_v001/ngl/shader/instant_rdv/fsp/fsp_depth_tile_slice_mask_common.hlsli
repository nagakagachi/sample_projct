#ifndef FSP_DEPTH_TILE_SLICE_MASK_COMMON_HLSLI
#define FSP_DEPTH_TILE_SLICE_MASK_COMMON_HLSLI
/*
手法のまとめ（記録用）
FSPの可視サーフェイス抽出を、画面全体を tile に分割して各 log-depth slice の実depth範囲（min/max）を集約する Depth Tile-Slice方式 で最適化.
後段は tile×slice の深度範囲から保守的AABBを復元してセルをmarkし、近景のみ最小限のpaddingで欠落を防ぎつつ、中遠景の過剰アクティブ化を抑制する

レガシーシングルパスが0.2msだったところがDepthTileSlice版マルチパスで0.1ms程度まで改善
*/

// Depth Tile-Sliceデータ構造の共通定義。
// レイアウト: tileごとに FSP_DEPTH_SLICE_COUNT 個のuintを連続配置し、
// 各要素は high16=min depth_q / low16=max depth_q を保持する。
// 使用されていないsliceは k_fsp_depth_tile_slice_range_unused を格納する。

// 1tileのピクセル辺長。mask生成パスの[numthreads]とcell mark側のtile分解で共通利用する。
#define FSP_DEPTH_TILE_SIZE 8
// 1tileあたりのlog-depth分割数。増やすほどdepth解像度は上がるが、メモリ/更新コストも増える。
#define FSP_DEPTH_SLICE_COUNT 32

// depth_q は 0..65534 を有効範囲として運用し、65535 は未使用にしてsentinelとの衝突を避ける。
static const uint k_fsp_depth_q_max = 65534u;
static const uint k_fsp_depth_tile_slice_range_unused = 0xffffffffu;
static const float k_fsp_depth_log_min_distance = 0.05;
static const float k_fsp_depth_log_max_distance = 65535.0;

uint FspEncodeDepthLogQ(float view_z)
{
    // 幅広い視距離を安定して扱うためlog2距離を16bit量子化する。
    const float depth_abs = clamp(abs(view_z), k_fsp_depth_log_min_distance, k_fsp_depth_log_max_distance);
    const float log_min = log2(k_fsp_depth_log_min_distance);
    const float log_max = log2(k_fsp_depth_log_max_distance);
    const float depth_q_f = saturate((log2(depth_abs) - log_min) / max(log_max - log_min, 1e-6));
    return min((uint)(depth_q_f * float(k_fsp_depth_q_max) + 0.5), k_fsp_depth_q_max);
}

float FspDecodeDepthLogQDistance(uint depth_q)
{
    const float log_min = log2(k_fsp_depth_log_min_distance);
    const float log_max = log2(k_fsp_depth_log_max_distance);
    const float depth_t = saturate(float(depth_q) / float(k_fsp_depth_q_max));
    return exp2(lerp(log_min, log_max, depth_t));
}

uint FspDepthSliceFromLogQ(uint depth_q)
{
    return min(depth_q / (65536u / FSP_DEPTH_SLICE_COUNT), FSP_DEPTH_SLICE_COUNT - 1u);
}

uint FspDepthTileSliceLinearIndex(uint tile_index, uint slice_index)
{
    return tile_index * FSP_DEPTH_SLICE_COUNT + slice_index;
}

uint FspPackDepthTileSliceRange(uint min_depth_q, uint max_depth_q)
{
    return (min_depth_q << 16) | (max_depth_q & 0xffffu);
}

void FspUnpackDepthTileSliceRange(uint packed_range, out uint min_depth_q, out uint max_depth_q)
{
    min_depth_q = packed_range >> 16;
    max_depth_q = packed_range & 0xffffu;
}

#endif
