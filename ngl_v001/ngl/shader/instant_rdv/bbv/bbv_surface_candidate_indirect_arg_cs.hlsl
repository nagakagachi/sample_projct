#if 0

bbv_surface_candidate_indirect_arg_cs.hlsl

Touched Brick ListのcounterからFineVoxel列挙用DispatchIndirect引数を生成する。

#endif

#include "../instant_rdv_util.hlsli"

RWBuffer<uint> RWBbvSurfaceCandidateIndirectArg;

[numthreads(1, 1, 1)]
void main_cs(uint3 dtid : SV_DispatchThreadID)
{
    const uint touched_count = min(BbvSurfaceTouchedFspCandidateList[0], bbv_brick_count());
    RWBbvSurfaceCandidateIndirectArg[0] = touched_count;
    RWBbvSurfaceCandidateIndirectArg[1] = 1u;
    RWBbvSurfaceCandidateIndirectArg[2] = 1u;
}
