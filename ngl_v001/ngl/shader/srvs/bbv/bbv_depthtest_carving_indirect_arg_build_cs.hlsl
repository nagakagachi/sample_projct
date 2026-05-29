
#if 0
bbv_depthtest_carving_indirect_arg_build_cs.hlsl

DepthTestCarving Frustum ActiveList を元に Carving 用 DispatchIndirect 引数を生成する。
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
    // bbv_depthtest_carving_cs.hlsl は 1 thread = 1 u32 block を処理する。
    // 1 Brick あたり k_bbv_per_voxel_bitmask_u32_count thread 必要なため、Dispatch数も同倍率で組む。
    // ここを active_brick_count 基準のままにすると最適化効果が出ないため注意。
    const uint total_u32_jobs = active_brick_count * k_bbv_per_voxel_bitmask_u32_count;
    RWFrustumBrickIndirectArg[0] = (total_u32_jobs + (k_bbv_depthtest_carving_thread_group_size - 1)) / k_bbv_depthtest_carving_thread_group_size;
    RWFrustumBrickIndirectArg[1] = 1;
    RWFrustumBrickIndirectArg[2] = 1;
}
