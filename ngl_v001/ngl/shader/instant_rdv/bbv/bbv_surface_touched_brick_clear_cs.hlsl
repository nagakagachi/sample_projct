#if 0

bbv_surface_touched_brick_clear_cs.hlsl

Generation marker scan用のBrick候補リストを初期化する。

#endif

#include "../instant_rdv_util.hlsli"

[numthreads(1, 1, 1)]
void main_cs(uint3 dtid : SV_DispatchThreadID)
{
    RWBbvSurfaceTouchedBrickList[0] = 0u;
}
