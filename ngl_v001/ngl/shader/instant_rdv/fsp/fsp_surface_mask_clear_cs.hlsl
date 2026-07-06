#if 0
fsp_surface_mask_clear_cs.hlsl

FSP SurfacePass専用セルマスクのクリアパス。
1bit = 1 global cell index のビット配列を毎フレーム0初期化する。

mode4 SurfaceMask path の先頭パス。
検出マスクは前フレームのbitを再利用できないため毎フレームclearする。
Buffer clear APIではなくcomputeで実行しているのは、既存のFSP dispatch/marker列に
揃えて可視化しやすくし、将来必要なら部分clearへ拡張しやすくするため。
#endif

#define FSP_SURFACE_MASK_CLEAR_THREAD_GROUP_SIZE 256

#include "../instant_rdv_util.hlsli"

[numthreads(FSP_SURFACE_MASK_CLEAR_THREAD_GROUP_SIZE, 1, 1)]
void main_cs(
    uint3 dtid : SV_DispatchThreadID,
    uint3 gtid : SV_GroupThreadID,
    uint3 gid : SV_GroupID,
    uint gindex : SV_GroupIndex)
{
    const uint total_cell_count = (uint)max(cb_instant_rdv.fsp_total_cell_count, 0);
    const uint mask_word_count = (total_cell_count + 31u) / 32u;
    if(dtid.x >= mask_word_count)
    {
        return;
    }

    // 1 thread clears 1 uint word, which covers 32 global FSP cells.
    // The buffer is compact enough that this cost is expected to be much smaller
    // than the depth-driven injection and later probe update work.
    RWFspSurfaceCellMaskBuffer[dtid.x] = 0u;
}
