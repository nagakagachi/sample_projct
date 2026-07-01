#if 0
fsp_screen_space_prepare_cs.hlsl

FSPの画面空間収集前準備パス。
可視セル候補の depth hint ワークバッファを初期化する。
#endif

#include "../instant_rdv_util.hlsli"

static const uint k_fsp_depth_hint_metric_init = 0xffffffffu;

[numthreads(96, 1, 1)]
void main_cs(
    uint3 dtid : SV_DispatchThreadID,
    uint3 gtid : SV_GroupThreadID,
    uint3 gid : SV_GroupID,
    uint gindex : SV_GroupIndex)
{
    // セル全域の depth hint ワークを sentinel 値へ初期化.
    if(dtid.x >= cb_instant_rdv.fsp_total_cell_count)
    {
        return;
    }

    RWFspCellDepthHintBuffer[dtid.x] = k_fsp_depth_hint_metric_init;
}
