/*
    bbv_surface_brick_pool_build_cs.hlsl
    MainView Depthを1回走査し、Screen Tile内でSurface候補を集約する。
*/

#define SURFACE_BRICK_POOL_BUILD_TILE_WIDTH 16
#define SURFACE_BRICK_POOL_BUILD_TILE_HEIGHT 8
#define SURFACE_BRICK_POOL_BUILD_THREAD_COUNT 128
#define SURFACE_BRICK_POOL_TILE_HASH_SIZE 256
#define SURFACE_BRICK_POOL_TILE_HASH_EMPTY 0xffffffffu

#include "../instant_rdv_util.hlsli"
#include "../../include/scene_view_struct.hlsli"

ConstantBuffer<BbvSurfaceInjectionViewInfo> cb_injection_src_view_info;
Texture2D TexHardwareDepth;

RWBuffer<uint> RWBbvSurfaceBrickPoolCandidate;
RWBuffer<uint> RWBbvSurfaceBrickPoolDebug;

groupshared uint g_surface_keys[SURFACE_BRICK_POOL_TILE_HASH_SIZE];
groupshared uint g_surface_masks[SURFACE_BRICK_POOL_TILE_HASH_SIZE];
groupshared uint g_emit_count;
groupshared uint g_emit_base;

void InsertTileSurfaceCandidate(uint packed_key, uint merged_mask)
{
    uint hash_slot = (packed_key * 2654435761u) &
        (SURFACE_BRICK_POOL_TILE_HASH_SIZE - 1u);

    [loop]
    for(uint probe = 0u; probe < SURFACE_BRICK_POOL_TILE_HASH_SIZE; ++probe)
    {
        uint observed_key = 0u;
        InterlockedCompareExchange(
            g_surface_keys[hash_slot],
            SURFACE_BRICK_POOL_TILE_HASH_EMPTY,
            packed_key,
            observed_key);
        if(observed_key == SURFACE_BRICK_POOL_TILE_HASH_EMPTY ||
           observed_key == packed_key)
        {
            InterlockedOr(g_surface_masks[hash_slot], merged_mask);
            return;
        }
        hash_slot = (hash_slot + 1u) &
            (SURFACE_BRICK_POOL_TILE_HASH_SIZE - 1u);
    }

    uint ignored_previous = 0u;
    InterlockedAdd(RWBbvSurfaceBrickPoolDebug[0], 1u, ignored_previous);
}

[numthreads(
    SURFACE_BRICK_POOL_BUILD_TILE_WIDTH,
    SURFACE_BRICK_POOL_BUILD_TILE_HEIGHT,
    1)]
void main_cs(
    uint3 dtid : SV_DispatchThreadID,
    uint gindex : SV_GroupIndex)
{
    g_surface_keys[gindex] = SURFACE_BRICK_POOL_TILE_HASH_EMPTY;
    g_surface_keys[gindex + SURFACE_BRICK_POOL_BUILD_THREAD_COUNT] =
        SURFACE_BRICK_POOL_TILE_HASH_EMPTY;
    g_surface_masks[gindex] = 0u;
    g_surface_masks[gindex + SURFACE_BRICK_POOL_BUILD_THREAD_COUNT] = 0u;
    if(gindex == 0u)
    {
        g_emit_count = 0u;
        g_emit_base = 0u;
    }
    GroupMemoryBarrierWithGroupSync();

    bool has_surface = false;
    uint packed_key = 0u;
    uint bit_mask = 0u;

    if(!any(dtid.xy >= cb_injection_src_view_info.cb_view_depth_buffer_offset_size.zw))
    {
        const float depth = TexHardwareDepth.Load(int3(
            dtid.xy + cb_injection_src_view_info.cb_view_depth_buffer_offset_size.xy,
            0)).r;
        if(depth > 0.0 && depth < 1.0)
        {
            const float2 screen_uv =
                (float2(dtid.xy) + 0.5.xx) /
                float2(cb_injection_src_view_info.cb_view_depth_buffer_offset_size.zw);
            const float surface_view_z = calc_view_z_from_ndc_z(
                depth,
                cb_injection_src_view_info.cb_ndc_z_to_view_z_coef);
            const float3 surface_pos_vs = CalcViewSpacePosition(
                screen_uv,
                surface_view_z,
                cb_injection_src_view_info.cb_proj_mtx);
            const float3 surface_pos_ws = mul(
                cb_injection_src_view_info.cb_view_inv_mtx,
                float4(surface_pos_vs, 1.0)).xyz;
            const float3 voxel_coordf =
                (surface_pos_ws - cb_instant_rdv.bbv.grid_min_pos) *
                cb_instant_rdv.bbv.cell_size_inv;
            const int3 voxel_coord = int3(floor(voxel_coordf));

            if(!any(voxel_coord < 0) &&
               !any(voxel_coord >= cb_instant_rdv.bbv.grid_resolution))
            {
                const int3 toroidal_coord = voxel_coord_toroidal_mapping(
                    voxel_coord,
                    cb_instant_rdv.bbv.grid_toroidal_offset,
                    cb_instant_rdv.bbv.grid_resolution);
                const uint brick_index = BbvPhysicalVoxelCoordToMortonIndex(
                    toroidal_coord,
                    cb_instant_rdv.bbv.grid_resolution);
                const uint3 fine_coord = uint3(
                    frac(voxel_coordf) * k_bbv_per_voxel_resolution);
                uint mask_word = 0u;
                uint mask_bit = 0u;
                calc_bbv_bitcell_info(mask_word, mask_bit, fine_coord);
                packed_key = (brick_index << 4u) | mask_word;
                bit_mask = 1u << mask_bit;
                has_surface = true;
            }
        }
    }

    // Wave内の同一Brick/mask wordを先に統合し、shared atomicを代表laneだけにする。
    uint4 pending_lanes = WaveActiveBallot(has_surface);
    while(ballot_any(pending_lanes))
    {
        const uint leader_lane = first_lane_from_ballot(pending_lanes);
        const uint leader_key = WaveReadLaneAt(packed_key, leader_lane);
        const bool is_same_key = has_surface && packed_key == leader_key;
        const uint4 same_key_lanes = WaveActiveBallot(is_same_key);
        const uint merged_mask =
            WaveActiveBitOr(is_same_key ? bit_mask : 0u);

        if(WaveGetLaneIndex() == leader_lane)
        {
            InsertTileSurfaceCandidate(leader_key, merged_mask);
        }
        pending_lanes &= ~same_key_lanes;
    }

    GroupMemoryBarrierWithGroupSync();

    uint emit_index0 = 0xffffffffu;
    uint emit_index1 = 0xffffffffu;
    if(g_surface_keys[gindex] != SURFACE_BRICK_POOL_TILE_HASH_EMPTY)
    {
        InterlockedAdd(g_emit_count, 1u, emit_index0);
    }
    if(g_surface_keys[gindex + SURFACE_BRICK_POOL_BUILD_THREAD_COUNT] !=
       SURFACE_BRICK_POOL_TILE_HASH_EMPTY)
    {
        InterlockedAdd(g_emit_count, 1u, emit_index1);
    }

    GroupMemoryBarrierWithGroupSync();

    uint candidate_element_count = 0u;
    RWBbvSurfaceBrickPoolCandidate.GetDimensions(candidate_element_count);
    const uint candidate_capacity = (candidate_element_count - 1u) >> 1u;
    if(gindex == 0u && g_emit_count > 0u)
    {
        InterlockedAdd(
            RWBbvSurfaceBrickPoolCandidate[0],
            g_emit_count,
            g_emit_base);
        const uint writable_count = g_emit_base < candidate_capacity
            ? min(g_emit_count, candidate_capacity - g_emit_base)
            : 0u;
        if(writable_count < g_emit_count)
        {
            uint ignored_previous = 0u;
            InterlockedAdd(
                RWBbvSurfaceBrickPoolDebug[0],
                g_emit_count - writable_count,
                ignored_previous);
        }
    }

    GroupMemoryBarrierWithGroupSync();

    if(emit_index0 != 0xffffffffu)
    {
        const uint candidate_index = g_emit_base + emit_index0;
        if(candidate_index < candidate_capacity)
        {
            const uint candidate_address = 1u + candidate_index * 2u;
            RWBbvSurfaceBrickPoolCandidate[candidate_address] =
                g_surface_keys[gindex];
            RWBbvSurfaceBrickPoolCandidate[candidate_address + 1u] =
                g_surface_masks[gindex];
        }
    }
    if(emit_index1 != 0xffffffffu)
    {
        const uint candidate_index = g_emit_base + emit_index1;
        if(candidate_index < candidate_capacity)
        {
            const uint candidate_address = 1u + candidate_index * 2u;
            RWBbvSurfaceBrickPoolCandidate[candidate_address] =
                g_surface_keys[
                    gindex + SURFACE_BRICK_POOL_BUILD_THREAD_COUNT];
            RWBbvSurfaceBrickPoolCandidate[candidate_address + 1u] =
                g_surface_masks[
                    gindex + SURFACE_BRICK_POOL_BUILD_THREAD_COUNT];
        }
    }
}
