#if 0
fsp_surface_mask_compact_cs.hlsl

FSP SurfacePass専用セルマスクのコンパクションパス。
立っているbitを SurfaceProbeCellList へ詰め、既存FSP更新パスへ合流させる。

SurfaceMask path の最終パス。
このパスで SurfaceProbeCellList を直接構築し、FspCellStateBuffer に可視frameと
depth hint を書く。旧 common finalize 用の中間バッファは使わない。
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

        // SurfaceMask正式採用後は中間の visible/depth-hint buffer を使わず、
        // 後段が読む cell state に直接可視frameとdepth hintを反映する。
        RWFspCellStateBuffer[global_cell_index].atomic_work = cb_instant_rdv.frame_count;
        RWFspCellStateBuffer[global_cell_index].depth_hint_packed_key = k_fsp_depth_hint_visible_flag;

        if(local_rank < writable_count)
        {
            RWSurfaceProbeCellList[append_base_index + local_rank + 1u] = global_cell_index;
        }
        ++local_rank;
    }
}
