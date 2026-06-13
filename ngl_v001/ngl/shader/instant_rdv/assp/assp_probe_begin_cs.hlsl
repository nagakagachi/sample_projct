#if 0

assp_probe_begin_cs.hlsl

ASSP frame begin。
可変ray数カウンタをフレーム先頭で初期化する。

#endif

#include "assp_probe_common.hlsli"

[numthreads(1, 1, 1)]
void main_cs(uint3 dtid : SV_DispatchThreadID)
{
    RWAsspProbeTotalRayCountBuffer[0] = 0u;
}
