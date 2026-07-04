#if 0
fsp_cell_depth_pyramid_mark_cs.hlsl

Cell-driven可視セル抽出。
各FSP cellをscreenへ投影し、depth min/max pyramidと交差したセルのみをactive markする。
#endif

#define FSP_CELL_DEPTH_PYRAMID_MARK_THREAD_GROUP_SIZE 64

#include "../instant_rdv_util.hlsli"
#include "../../include/scene_view_struct.hlsli"

ConstantBuffer<SceneViewInfo> cb_ngl_sceneview;
Texture2D<float2> FspDepthMinMaxPyramidTex;

static const uint k_fsp_depth_hint_visible_flag = 0u;

float3 FspCellCornerWs(float3 cell_min_ws, float3 cell_max_ws, uint corner_index)
{
    // 0..7bitからAABB cornerを復元。分岐せずlerpで展開してALUコストを安定化する。
    const float3 corner_lerp = float3((corner_index & 1u) ? 1.0 : 0.0, (corner_index & 2u) ? 1.0 : 0.0, (corner_index & 4u) ? 1.0 : 0.0);
    return lerp(cell_min_ws, cell_max_ws, corner_lerp);
}

bool FspBuildCellScreenAabbAndDepthRange(
    float3 cell_min_ws,
    float3 cell_max_ws,
    out float2 out_uv_min,
    out float2 out_uv_max,
    out float out_depth_min,
    out float out_depth_max)
{
    out_uv_min = float2( 3.402823466e+38,  3.402823466e+38);
    out_uv_max = float2(-3.402823466e+38, -3.402823466e+38);
    out_depth_min = 3.402823466e+38;
    out_depth_max = -3.402823466e+38;
    bool has_valid_corner = false;

    [unroll]
    for(uint corner_index = 0u; corner_index < 8u; ++corner_index)
    {
        const float3 corner_ws = FspCellCornerWs(cell_min_ws, cell_max_ws, corner_index);
        const float3 corner_vs = mul(cb_ngl_sceneview.cb_view_mtx, float4(corner_ws, 1.0));
        // near plane手前にあるcornerはscreen AABBに寄与させない。
        if(corner_vs.z <= 1e-5)
        {
            continue;
        }

        const float4 corner_cs = mul(cb_ngl_sceneview.cb_proj_mtx, float4(corner_vs, 1.0));
        // w<=0近傍は射影不安定なので除外し、AABBの暴走を防ぐ。
        if(corner_cs.w <= 1e-6)
        {
            continue;
        }

        const float2 ndc_xy = corner_cs.xy / corner_cs.w;
        const float2 uv = ndc_xy * float2(0.5, -0.5) + 0.5;
        const float view_distance = abs(corner_vs.z);

        out_uv_min = min(out_uv_min, uv);
        out_uv_max = max(out_uv_max, uv);
        out_depth_min = min(out_depth_min, view_distance);
        out_depth_max = max(out_depth_max, view_distance);
        has_valid_corner = true;
    }

    if(!has_valid_corner)
    {
        return false;
    }

    const float2 uv_min_clamped = max(out_uv_min, 0.0.xx);
    const float2 uv_max_clamped = min(out_uv_max, 1.0.xx);
    if(any(uv_min_clamped >= uv_max_clamped))
    {
        // 画面外、または投影退化したcellはdepth交差判定を行わない。
        return false;
    }

    out_uv_min = uv_min_clamped;
    out_uv_max = uv_max_clamped;
    return true;
}

bool FspHasDepthIntersection(float2 uv_min, float2 uv_max, float cell_depth_min, float cell_depth_max)
{
    uint base_width = 1;
    uint base_height = 1;
    uint mip_count = 1;
    FspDepthMinMaxPyramidTex.GetDimensions(0, base_width, base_height, mip_count);

    float2 span_px = (uv_max - uv_min) * float2(base_width, base_height);
    float span_max = max(span_px.x, span_px.y);
    uint mip_index = 0u;
    // 1セルあたりのサンプル数が増えすぎないよう、AABBサイズに応じてmipを選択する。
    // 4px閾値は「広いAABBは粗いmipで保守判定、狭いAABBは細かいmipで過剰mark抑制」のバランス点。
    while((span_max > 4.0) && ((mip_index + 1u) < mip_count))
    {
        span_max *= 0.5;
        ++mip_index;
    }

    uint mip_width = 1;
    uint mip_height = 1;
    uint mip_level_count_unused = 0;
    FspDepthMinMaxPyramidTex.GetDimensions(mip_index, mip_width, mip_height, mip_level_count_unused);
    const float2 mip_size_f = float2(mip_width, mip_height);

    int2 sample_min = int2(floor(uv_min * mip_size_f));
    int2 sample_max = int2(ceil(uv_max * mip_size_f)) - 1;
    // ceil-1 で「AABBと1pxでも重なるタイル」を漏れなく含める conservative raster 相当の範囲化。
    sample_min = clamp(sample_min, int2(0, 0), int2(max((int)mip_width - 1, 0), max((int)mip_height - 1, 0)));
    sample_max = clamp(sample_max, int2(0, 0), int2(max((int)mip_width - 1, 0), max((int)mip_height - 1, 0)));

    if(any(sample_min > sample_max))
    {
        return false;
    }

    [loop]
    for(int y = sample_min.y; y <= sample_max.y; ++y)
    {
        [loop]
        for(int x = sample_min.x; x <= sample_max.x; ++x)
        {
            const float2 depth_range = FspDepthMinMaxPyramidTex.Load(int3(x, y, mip_index));
            if(depth_range.x > depth_range.y)
            {
                // 無効range(未使用depth)は交差判定から除外。
                continue;
            }

            if(!(cell_depth_max < depth_range.x || depth_range.y < cell_depth_min))
            {
                // [cell_min,cell_max] と [depth_min,depth_max] が1点でも重なれば可視候補として採用。
                return true;
            }
        }
    }

    return false;
}

[numthreads(FSP_CELL_DEPTH_PYRAMID_MARK_THREAD_GROUP_SIZE, 1, 1)]
void main_cs(
    uint3 dtid : SV_DispatchThreadID,
    uint3 gtid : SV_GroupThreadID,
    uint3 gid : SV_GroupID,
    uint gindex : SV_GroupIndex)
{
    const uint global_cell_index = dtid.x;
    if(global_cell_index >= (uint)cb_instant_rdv.fsp_total_cell_count)
    {
        return;
    }

    uint cascade_index = 0u;
    uint local_cell_index = 0u;
    if(!FspDecodeGlobalCellIndex(global_cell_index, cascade_index, local_cell_index))
    {
        return;
    }

    const FspCascadeGridParam cascade = FspGetCascadeParam(cascade_index);
    const int3 cell_coord = FspLocalCellIndexToLinearCoord(local_cell_index, cascade.grid);
    // toroidal空間上のlocal indexを線形座標へ戻してから、world AABBを再構成する。
    const float3 cell_min_ws = float3(cell_coord) * cascade.grid.cell_size + cascade.grid.grid_min_pos;
    const float3 cell_max_ws = cell_min_ws + cascade.grid.cell_size.xxx;

    float2 uv_min = 0.0.xx;
    float2 uv_max = 0.0.xx;
    float cell_depth_min = 0.0;
    float cell_depth_max = 0.0;
    if(!FspBuildCellScreenAabbAndDepthRange(cell_min_ws, cell_max_ws, uv_min, uv_max, cell_depth_min, cell_depth_max))
    {
        return;
    }

    if(!FspHasDepthIntersection(uv_min, uv_max, cell_depth_min, cell_depth_max))
    {
        return;
    }

    // finalize passでlist化できるよう、共通ワーク形式(frame stamp + depth hint flag)へ書き込む。
    RWFspCellVisibleFrameBuffer[global_cell_index] = cb_instant_rdv.frame_count;
    RWFspCellDepthHintBuffer[global_cell_index] = k_fsp_depth_hint_visible_flag;
}
