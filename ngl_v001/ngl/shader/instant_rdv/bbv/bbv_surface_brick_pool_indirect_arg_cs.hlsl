/*
    bbv_surface_brick_pool_indirect_arg_cs.hlsl
    SurfaceBrickPoolのunique countからDispatchIndirect引数を生成する。
*/

#include "../instant_rdv_util.hlsli"

Buffer<uint> BbvSurfaceBrickPoolBuffer;
RWBuffer<uint> RWBbvSurfaceBrickPoolIndirectArg;

[numthreads(1, 1, 1)]
void main_cs(uint3 dtid : SV_DispatchThreadID)
{
    const uint count = min(
        BbvSurfaceBrickPoolBuffer[0],
        (uint)k_bbv_surface_brick_pool_capacity);

    // Inject shaderは [numthreads(64, 1, 1)] だが、SV_GroupID.xを
    // Pool entry indexとして使う。したがって1 group = 1 Brickであり、
    // X dispatch数はcountそのもの（count / 64ではない）。
    RWBbvSurfaceBrickPoolIndirectArg[0] = count;
    RWBbvSurfaceBrickPoolIndirectArg[1] = 1u;
    RWBbvSurfaceBrickPoolIndirectArg[2] = 1u;
}
