#include "../instant_rdv_util.hlsli"

RWBuffer<uint> RWFspDebugStats;

[numthreads(1, 1, 1)]
void main_cs(uint3 dtid : SV_DispatchThreadID)
{
    RWFspDebugStats[0] = SurfaceProbeCellList[0];
    RWFspDebugStats[1] = FspProbeFreeStack[0];
    RWFspDebugStats[2] = FspActiveProbeListCurr[0];
}
