/*
    bbv_surface_brick_pool_candidate_indirect_arg_cs.hlsl
    Tile Candidate数からAllocate/Merge用DispatchIndirect引数を生成する。
*/

Buffer<uint> BbvSurfaceBrickPoolCandidateBuffer;
RWBuffer<uint> RWBbvSurfaceBrickPoolCandidateIndirectArg;

[numthreads(1, 1, 1)]
void main_cs(uint3 dtid : SV_DispatchThreadID)
{
    uint element_count = 0u;
    BbvSurfaceBrickPoolCandidateBuffer.GetDimensions(element_count);
    const uint capacity = (element_count - 1u) >> 1u;
    const uint candidate_count = min(
        BbvSurfaceBrickPoolCandidateBuffer[0],
        capacity);

    // Allocate/Merge shaderは [numthreads(64, 1, 1)] でcandidateを
    // SV_DispatchThreadID.xから1件ずつ処理する。X dispatch数は
    // candidate_countを64スレッド単位のgroup数へ変換する。
    RWBbvSurfaceBrickPoolCandidateIndirectArg[0] =
        (candidate_count + 63u) / 64u;
    RWBbvSurfaceBrickPoolCandidateIndirectArg[1] = 1u;
    RWBbvSurfaceBrickPoolCandidateIndirectArg[2] = 1u;
}
