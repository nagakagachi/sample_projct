#if 0

bbv_surface_touched_brick_scan_cs.hlsl

Injection時に記録したraw Touched Brick候補だけを走査し、
Removal/Carving後も占有が残るBrickを最終リストへ詰める。

#endif

#include "../instant_rdv_util.hlsli"

[numthreads(PROBE_UPDATE_THREAD_GROUP_SIZE, 1, 1)]
void main_cs(uint3 dtid : SV_DispatchThreadID)
{
    const uint raw_index = dtid.x;
    const uint brick_count = bbv_brick_count();
    const uint raw_count = min(
        BbvSurfaceTouchedFspCandidateList[0],
        brick_count);
    if(raw_index >= raw_count)
    {
        return;
    }

    const uint brick_index = BbvSurfaceTouchedFspCandidateList[raw_index + 1u];
    if(brick_index >= brick_count)
    {
        return;
    }

    // Removal/Carving後に空になったBrickはSurface候補へ流さない。
    if(BitmaskBrickVoxel[bbv_voxel_coarse_occupancy_info_addr(brick_index)] == 0u)
    {
        return;
    }

    uint append_index = 0u;
    InterlockedAdd(RWBbvSurfaceTouchedBrickList[0], 1u, append_index);
    if(append_index < brick_count)
    {
        RWBbvSurfaceTouchedBrickList[append_index + 1u] = brick_index;
    }
}
