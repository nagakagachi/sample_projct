
#if 0
bbv_depthtest_carving_indirect_arg_build_cs.hlsl

DepthTest Frustum ActiveList を元に Carving 用 DispatchIndirect 引数を生成する。
#endif

#include "../srvs_util.hlsli"

RWBuffer<uint> RWFrustumBrickIndirectArg;

[numthreads(1, 1, 1)]
void main_cs(
    uint3 dtid : SV_DispatchThreadID,
    uint3 gtid : SV_GroupThreadID,
    uint3 gid : SV_GroupID,
    uint gindex : SV_GroupIndex)
{
    const uint active_brick_count = FrustumBrickList[0];
    // bbv_depthtest_carving_cs.hlsl と同じ thread group size を使って引数を生成する。
    RWFrustumBrickIndirectArg[0] = (active_brick_count + (k_bbv_depthtest_carving_thread_group_size - 1)) / k_bbv_depthtest_carving_thread_group_size;
    RWFrustumBrickIndirectArg[1] = 1;
    RWFrustumBrickIndirectArg[2] = 1;
}
