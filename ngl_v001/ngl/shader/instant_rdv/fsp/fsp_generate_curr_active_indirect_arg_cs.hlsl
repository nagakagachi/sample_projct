/*
    fsp_generate_curr_active_indirect_arg_cs.hlsl

    現フレームActiveProbeListの世代交代counterからRay Requestおよび
    SH Update用のDispatchIndirect引数を生成する。

    ActiveProbeListは先頭2ワードを交互counterとして使用するため、
    SurfaceProbeCellList等の通常の[0] counter形式とは分けて扱う。
*/

#include "../instant_rdv_util.hlsli"

Buffer<uint> ProbeIndexList;
RWBuffer<uint> RWFspIndirectArg;

[numthreads(1, 1, 1)]
void main_cs(uint3 dtid : SV_DispatchThreadID)
{
    const uint list_count =
        ProbeIndexList[FspActiveProbeCurrentCounterSlot()];
    const uint dispatch_group_count =
        (list_count + (cb_instant_rdv.fsp_indirect_cs_thread_group_size.x - 1u)) /
        cb_instant_rdv.fsp_indirect_cs_thread_group_size.x;

    RWFspIndirectArg[0] = max(dispatch_group_count, 1u);
    RWFspIndirectArg[1] = 1u;
    RWFspIndirectArg[2] = 1u;
}
