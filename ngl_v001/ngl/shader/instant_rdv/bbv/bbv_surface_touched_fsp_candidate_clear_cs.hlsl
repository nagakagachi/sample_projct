#if 0

bbv_surface_touched_fsp_candidate_clear_cs.hlsl

Touched Brickから生成する診断用FSP候補リストを初期化する。

#endif

#include "../instant_rdv_util.hlsli"

[numthreads(1, 1, 1)]
void main_cs(uint3 dtid : SV_DispatchThreadID)
{
    RWBbvSurfaceTouchedFspCandidateList[0] = 0u;
}
