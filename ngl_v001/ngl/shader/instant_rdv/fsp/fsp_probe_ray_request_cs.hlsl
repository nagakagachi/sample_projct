#if 0

fsp_probe_ray_request_cs.hlsl

FSP update multipass の Request パス。
active probe list を trace 用 request buffer に詰め替え、
固定レイ本数(= octmap 全セル数)から total ray count と DispatchIndirect 引数を作る。

#endif

#include "../instant_rdv_util.hlsli"

#define FSP_PROBE_RAY_COUNT_PER_PROBE (k_fsp_probe_octmap_width * k_fsp_probe_octmap_width)
#define FSP_RAY_LINEAR_THREAD_GROUP_SIZE 128u

[numthreads(FSP_RAY_LINEAR_THREAD_GROUP_SIZE, 1, 1)]
void main_cs(
    uint3 dtid : SV_DispatchThreadID,
    uint3 gtid : SV_GroupThreadID,
    uint3 gid : SV_GroupID,
    uint gindex : SV_GroupIndex)
{
    const uint active_probe_count = FspActiveProbeListCurr[0];

    // total ray count / trace indirect arg は1スレッドだけで確定する。
    if(dtid.x == 0)
    {
        const uint total_ray_count = active_probe_count * FSP_PROBE_RAY_COUNT_PER_PROBE;
        RWFspProbeTotalRayCountBuffer[0] = total_ray_count;

        const uint dispatch_group_count =
            (total_ray_count + (FSP_RAY_LINEAR_THREAD_GROUP_SIZE - 1u)) / FSP_RAY_LINEAR_THREAD_GROUP_SIZE;
        RWFspProbeTraceIndirectArg[0] = max(dispatch_group_count, 1u);
        RWFspProbeTraceIndirectArg[1] = 1u;
        RWFspProbeTraceIndirectArg[2] = 1u;
    }

    if(dtid.x >= active_probe_count)
    {
        return;
    }

    const uint probe_index = FspActiveProbeListCurr[dtid.x + 1];
    RWFspProbeRayRequestBuffer[dtid.x] =
        (probe_index < (uint)cb_instant_rdv.fsp_probe_pool_size) ? probe_index : k_fsp_invalid_probe_index;
}
