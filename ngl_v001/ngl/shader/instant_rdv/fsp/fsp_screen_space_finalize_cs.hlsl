#if 0
fsp_screen_space_finalize_cs.hlsl

FSPの画面空間収集確定パス。
セルワーク（visible frame/depth hint）から可視セルリストを構築し、
既存の FspCellStateBuffer へ互換反映する。
#endif

#include "../instant_rdv_util.hlsli"

[numthreads(96, 1, 1)]
void main_cs(
    uint3 dtid : SV_DispatchThreadID,
    uint3 gtid : SV_GroupThreadID,
    uint3 gid : SV_GroupID,
    uint gindex : SV_GroupIndex)
{
    // セル全域走査.
    const uint global_cell_index = dtid.x;
    if(global_cell_index >= cb_instant_rdv.fsp_total_cell_count)
    {
        return;
    }
    if(FspCellVisibleFrameBuffer[global_cell_index] != cb_instant_rdv.frame_count)
    {
        return;
    }

    // 既存パス互換のため cell state に mirror.
    RWFspCellStateBuffer[global_cell_index].atomic_work = cb_instant_rdv.frame_count;
    RWFspCellStateBuffer[global_cell_index].depth_hint_packed_key = FspCellDepthHintBuffer[global_cell_index];

    // 可視セルリストへ登録.
    int current_visible_count = 0;
    InterlockedAdd(RWSurfaceProbeCellList[0], 1, current_visible_count);
    if(cb_instant_rdv.fsp_visible_voxel_buffer_size > current_visible_count)
    {
        RWSurfaceProbeCellList[current_visible_count + 1] = global_cell_index;
    }
    else
    {
        InterlockedAdd(RWSurfaceProbeCellList[0], -1);
    }
}
