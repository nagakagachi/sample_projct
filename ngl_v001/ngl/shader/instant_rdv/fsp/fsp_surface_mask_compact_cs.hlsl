#if 0
fsp_surface_mask_compact_cs.hlsl

FSP SurfacePass専用セルマスクのコンパクションパス。
立っているbitを SurfaceProbeCellList へ詰め、既存FSP更新パスへ合流させる。

mode4 SurfaceMask path の最終パス。
このパスは common finalize の代替であり、SurfaceProbeCellList を直接構築する。
同時に既存のFSP後段が参照する frame stamp / depth hint / cell state mirror も更新するため、
C++側では mode4 のとき fsp_screen_space_finalize_cs を実行しない。
#endif

#define FSP_SURFACE_MASK_COMPACT_THREAD_GROUP_SIZE 128

#include "../instant_rdv_util.hlsli"

static const uint k_fsp_depth_hint_visible_flag = 0u;

[numthreads(FSP_SURFACE_MASK_COMPACT_THREAD_GROUP_SIZE, 1, 1)]
void main_cs(
    uint3 dtid : SV_DispatchThreadID,
    uint3 gtid : SV_GroupThreadID,
    uint3 gid : SV_GroupID,
    uint gindex : SV_GroupIndex)
{
    const uint total_cell_count = (uint)max(cb_instant_rdv.fsp_total_cell_count, 0);
    const uint mask_word_count = (total_cell_count + 31u) / 32u;
    const uint mask_word_index = dtid.x;
    if(mask_word_index >= mask_word_count)
    {
        return;
    }

    const uint mask_bits = FspSurfaceCellMaskBuffer[mask_word_index];
    if(mask_bits == 0u)
    {
        return;
    }

    // Reserve contiguous output slots once per non-zero word. This keeps append
    // atomics proportional to non-empty mask words rather than set bits or pixels.
    const uint bit_count = countbits(mask_bits);
    uint append_base_index = 0u;
    InterlockedAdd(RWSurfaceProbeCellList[0], bit_count, append_base_index);

    // Match the existing visible surface list contract:
    // element 0 is a counter and elements [1, max_visible_cell_count] are cells.
    // If the reservation overflowed, undo only the unwritable part so later passes
    // see a bounded count instead of reading past the allocated list.
    const uint max_visible_cell_count = (uint)max(cb_instant_rdv.fsp_visible_voxel_buffer_size, 0);
    uint writable_count = 0u;
    if(append_base_index < max_visible_cell_count)
    {
        writable_count = min(bit_count, max_visible_cell_count - append_base_index);
    }
    const uint overflow_count = bit_count - writable_count;
    if(overflow_count > 0u)
    {
        InterlockedAdd(RWSurfaceProbeCellList[0], -(int)overflow_count);
    }

    uint remaining_bits = mask_bits;
    uint local_rank = 0u;
    while(remaining_bits != 0u)
    {
        // Iterate only set bits. firstbitlow + bit clear avoids scanning all 32
        // bit positions when a word contains only a few visible cells.
        const uint bit_index = firstbitlow(remaining_bits);
        remaining_bits &= (remaining_bits - 1u);

        const uint global_cell_index = mask_word_index * 32u + bit_index;
        if(global_cell_index >= total_cell_count)
        {
            continue;
        }

        // 既存 finalize 経路と同等の反映:
        // - 可視frame stamp
        // - depth hint（現状は可視フラグ運用。将来的な削除候補だが互換性のため維持）
        // - cell state mirror
        RWFspCellVisibleFrameBuffer[global_cell_index] = cb_instant_rdv.frame_count;
        RWFspCellDepthHintBuffer[global_cell_index] = k_fsp_depth_hint_visible_flag;
        RWFspCellStateBuffer[global_cell_index].atomic_work = cb_instant_rdv.frame_count;
        RWFspCellStateBuffer[global_cell_index].depth_hint_packed_key = k_fsp_depth_hint_visible_flag;

        if(local_rank < writable_count)
        {
            RWSurfaceProbeCellList[append_base_index + local_rank + 1u] = global_cell_index;
        }
        ++local_rank;
    }
}
