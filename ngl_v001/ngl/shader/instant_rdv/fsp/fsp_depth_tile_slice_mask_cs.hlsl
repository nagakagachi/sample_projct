#if 0
fsp_depth_tile_slice_mask_cs.hlsl

FSP可視サーフェイス抽出用のDepth tile slice range生成パス。
全pixelのdepthを読み、tile x log depth sliceごとの実min/max depthへ保守的に集約する。
#endif

#define FSP_DEPTH_TILE_SIZE 8
#define FSP_DEPTH_SLICE_COUNT 32

#include "../instant_rdv_util.hlsli"
#include "../../include/scene_view_struct.hlsli"

ConstantBuffer<SceneViewInfo> cb_ngl_sceneview;
Texture2D TexHardwareDepth;

groupshared uint gs_tile_slice_mask;
groupshared uint gs_slice_min_q[FSP_DEPTH_SLICE_COUNT];
groupshared uint gs_slice_max_q[FSP_DEPTH_SLICE_COUNT];

uint FspEncodeDepthLogQ(float view_z)
{
    // 幅広い視距離を安定して扱うためlog2距離を16bit量子化する。
    const float depth_abs = clamp(abs(view_z), 0.05, 65535.0);
    const float log_min = log2(0.05);
    const float log_max = log2(65535.0);
    const float depth_q_f = saturate((log2(depth_abs) - log_min) / max(log_max - log_min, 1e-6));
    return min((uint)(depth_q_f * 65534.0 + 0.5), 65534u);
}

uint FspDepthSliceFromLogQ(uint depth_q)
{
    return min(depth_q / (65536u / FSP_DEPTH_SLICE_COUNT), FSP_DEPTH_SLICE_COUNT - 1u);
}

[numthreads(FSP_DEPTH_TILE_SIZE, FSP_DEPTH_TILE_SIZE, 1)]
void main_cs(
    uint3 dtid : SV_DispatchThreadID,
    uint3 gtid : SV_GroupThreadID,
    uint3 gid : SV_GroupID,
    uint gindex : SV_GroupIndex)
{
    // tile内全pixelの実depth幅をsliceごとに集約し、遠景sliceの過剰な奥行き区間markを避ける。
    if(gindex == 0)
    {
        gs_tile_slice_mask = 0u;
    }
    if(gindex < FSP_DEPTH_SLICE_COUNT)
    {
        gs_slice_min_q[gindex] = 0xffffu;
        gs_slice_max_q[gindex] = 0u;
    }
    GroupMemoryBarrierWithGroupSync();

    if(all(dtid.xy < cb_instant_rdv.tex_main_view_depth_size.xy))
    {
        const float d = TexHardwareDepth.Load(int3(dtid.xy, 0)).r;
        const float view_z = calc_view_z_from_ndc_z(d, cb_ngl_sceneview.cb_ndc_z_to_view_z_coef);
        if(65535.0 > abs(view_z))
        {
            const uint depth_q = FspEncodeDepthLogQ(view_z);
            const uint slice_index = FspDepthSliceFromLogQ(depth_q);
            InterlockedOr(gs_tile_slice_mask, 1u << slice_index);
            InterlockedMin(gs_slice_min_q[slice_index], depth_q);
            InterlockedMax(gs_slice_max_q[slice_index], depth_q);
        }
    }

    GroupMemoryBarrierWithGroupSync();

    if(gindex == 0)
    {
        const uint tile_count_x = (uint(cb_instant_rdv.tex_main_view_depth_size.x) + FSP_DEPTH_TILE_SIZE - 1u) / FSP_DEPTH_TILE_SIZE;
        const uint tile_index = gid.x + gid.y * tile_count_x;
        [unroll]
        for(uint slice_index = 0u; slice_index < FSP_DEPTH_SLICE_COUNT; ++slice_index)
        {
            const uint write_index = tile_index * FSP_DEPTH_SLICE_COUNT + slice_index;
            if(0u != (gs_tile_slice_mask & (1u << slice_index)))
            {
                // high16=min log depth, low16=max log depth. sentinelは未使用slice。
                RWFspDepthTileSliceMaskBuffer[write_index] = (gs_slice_min_q[slice_index] << 16) | gs_slice_max_q[slice_index];
            }
            else
            {
                RWFspDepthTileSliceMaskBuffer[write_index] = 0xffffffffu;
            }
        }
    }
}
