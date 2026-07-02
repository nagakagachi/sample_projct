#if 0
fsp_tile_slice_cell_mark_cs.hlsl

FSP可視サーフェイス抽出用のtile slice cell markパス。
tileのdepth slice min/max rangeから保守的なworld AABBを作り、交差しうるFSP cellをmarkする。
#endif

#define FSP_DEPTH_TILE_SIZE 8
#define FSP_DEPTH_SLICE_COUNT 32
#define FSP_TILE_MARK_THREAD_GROUP_SIZE 64

#include "../instant_rdv_util.hlsli"
#include "../../include/scene_view_struct.hlsli"

ConstantBuffer<SceneViewInfo> cb_ngl_sceneview;

static const uint k_fsp_depth_hint_visible_flag = 0u;

float FspDecodeDepthLogQDistance(uint depth_q)
{
    const float log_min = log2(0.05);
    const float log_max = log2(65535.0);
    const float depth_t = saturate(float(depth_q) / 65534.0);
    return exp2(lerp(log_min, log_max, depth_t));
}

float3 FspTileSliceCornerWs(float2 uv, float view_z)
{
    const float3 pos_vs = CalcViewSpacePosition(uv, view_z, cb_ngl_sceneview.cb_proj_mtx);
    return mul(cb_ngl_sceneview.cb_view_inv_mtx, float4(pos_vs, 1.0)).xyz;
}

void FspMarkCellRange(uint cascade_index, float3 world_min, float3 world_max)
{
    const FspCascadeGridParam cascade = FspGetCascadeParam(cascade_index);
    const float3 grid_coord_min_f = (world_min - cascade.grid.grid_min_pos) * cascade.grid.cell_size_inv;
    const float3 grid_coord_max_f = (world_max - cascade.grid.grid_min_pos) * cascade.grid.cell_size_inv;
    const int3 coord_min = max((int3)floor(min(grid_coord_min_f, grid_coord_max_f)), int3(0, 0, 0));
    const int3 coord_max = min((int3)floor(max(grid_coord_min_f, grid_coord_max_f)), cascade.grid.grid_resolution - 1);

    if(any(coord_min > coord_max))
    {
        return;
    }

    // 保守的AABB内のcellを全てmarkする。過剰markは許容し、surfaceの取り逃しを避ける。
    [loop]
    for(int z = coord_min.z; z <= coord_max.z; ++z)
    {
        [loop]
        for(int y = coord_min.y; y <= coord_max.y; ++y)
        {
            [loop]
            for(int x = coord_min.x; x <= coord_max.x; ++x)
            {
                const int3 voxel_coord_toroidal = voxel_coord_toroidal_mapping(int3(x, y, z), cascade.grid.grid_toroidal_offset, cascade.grid.grid_resolution);
                const uint local_cell_index = voxel_coord_to_index(voxel_coord_toroidal, cascade.grid.grid_resolution);
                const uint global_cell_index = cascade.cell_offset + local_cell_index;
                RWFspCellVisibleFrameBuffer[global_cell_index] = cb_instant_rdv.frame_count;
                RWFspCellDepthHintBuffer[global_cell_index] = k_fsp_depth_hint_visible_flag;
            }
        }
    }
}

[numthreads(FSP_TILE_MARK_THREAD_GROUP_SIZE, 1, 1)]
void main_cs(
    uint3 dtid : SV_DispatchThreadID,
    uint3 gtid : SV_GroupThreadID,
    uint3 gid : SV_GroupID,
    uint gindex : SV_GroupIndex)
{
    const uint2 depth_size = uint2(cb_instant_rdv.tex_main_view_depth_size.xy);
    const uint tile_count_x = (depth_size.x + FSP_DEPTH_TILE_SIZE - 1u) / FSP_DEPTH_TILE_SIZE;
    const uint tile_count_y = (depth_size.y + FSP_DEPTH_TILE_SIZE - 1u) / FSP_DEPTH_TILE_SIZE;
    const uint tile_count = tile_count_x * tile_count_y;
    const uint tile_index = dtid.x;
    if(tile_index >= tile_count)
    {
        return;
    }

    // tileのscreen boundsを使い、各depth slice内の実min/max範囲だけをworld AABBへ変換する。
    const uint tile_x = tile_index % tile_count_x;
    const uint tile_y = tile_index / tile_count_x;
    const uint2 pixel_min = uint2(tile_x, tile_y) * FSP_DEPTH_TILE_SIZE;
    const uint2 pixel_max = min(pixel_min + FSP_DEPTH_TILE_SIZE, depth_size);
    const float2 uv_min = float2(pixel_min) / float2(depth_size);
    const float2 uv_max = float2(pixel_max) / float2(depth_size);

    [loop]
    for(uint slice_index = 0u; slice_index < FSP_DEPTH_SLICE_COUNT; ++slice_index)
    {
        const uint slice_range_packed = FspDepthTileSliceMaskBuffer[tile_index * FSP_DEPTH_SLICE_COUNT + slice_index];
        if(slice_range_packed == 0xffffffffu)
        {
            continue;
        }

        // bitmaskだけでは遠景sliceが巨大な奥行き区間になり過剰markするため、実depth rangeを復元する。
        const uint min_depth_q = slice_range_packed >> 16;
        const uint max_depth_q = slice_range_packed & 0xffffu;
        const uint padded_min_depth_q = (min_depth_q > 0u) ? (min_depth_q - 1u) : 0u;
        const uint padded_max_depth_q = min(max_depth_q + 1u, 65534u);
        const float view_z0 = FspDecodeDepthLogQDistance(padded_min_depth_q);
        const float view_z1 = FspDecodeDepthLogQDistance(padded_max_depth_q);

        float3 world_min = float3( 3.402823466e+38,  3.402823466e+38,  3.402823466e+38);
        float3 world_max = float3(-3.402823466e+38, -3.402823466e+38, -3.402823466e+38);

        const float2 uvs[4] =
        {
            float2(uv_min.x, uv_min.y),
            float2(uv_max.x, uv_min.y),
            float2(uv_min.x, uv_max.y),
            float2(uv_max.x, uv_max.y)
        };

        [unroll]
        for(uint corner_index = 0u; corner_index < 4u; ++corner_index)
        {
            const float3 ws0 = FspTileSliceCornerWs(uvs[corner_index], view_z0);
            const float3 ws1 = FspTileSliceCornerWs(uvs[corner_index], view_z1);
            world_min = min(world_min, min(ws0, ws1));
            world_max = max(world_max, max(ws0, ws1));
        }

        const uint cascade_count = FspCascadeCount();
        [unroll]
        for(uint cascade_index = 0u; cascade_index < k_fsp_max_cascade_count; ++cascade_index)
        {
            if(cascade_index >= cascade_count)
            {
                break;
            }
            FspMarkCellRange(cascade_index, world_min, world_max);
        }
    }
}
