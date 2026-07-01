
#if 0

fsp_clear_voxel_cs.hlsl

#endif


#include "../instant_rdv_util.hlsli"


static const uint k_fsp_depth_hint_metric_init = 0xffffffffu;

// DepthBufferに対してDispatch.
[numthreads(96, 1, 1)]
void main_cs(
	uint3 dtid	: SV_DispatchThreadID,
	uint3 gtid : SV_GroupThreadID,
	uint3 gid : SV_GroupID,
	uint gindex : SV_GroupIndex
)
{
    // 全Voxelをクリア.
    const uint cell_count = cb_instant_rdv.fsp_total_cell_count;
    const uint probe_pool_size = cb_instant_rdv.fsp_probe_pool_size;

    if(0 == dtid.x)
    {
        RWSurfaceProbeCellList[0] = 0;
        RWFspProbeFreeStack[0] = probe_pool_size;
        RWFspActiveProbeListPrev[0] = 0;
        RWFspActiveProbeListCurr[0] = 0;
    }

    if(dtid.x < cell_count)
    {
        RWFspCellStateBuffer[dtid.x] = (FspProbeData)0;
        RWFspCellProbeIndexBuffer[dtid.x] = k_fsp_invalid_probe_index;
        RWFspCellVisibleFrameBuffer[dtid.x] = 0;
        RWFspCellDepthHintBuffer[dtid.x] = k_fsp_depth_hint_metric_init;
    }

    if(dtid.x < probe_pool_size)
    {
        FspProbePoolData probe_data = (FspProbePoolData)0;
        probe_data.owner_cell_index = k_fsp_invalid_probe_index;
        RWFspProbePoolBuffer[dtid.x] = probe_data;
        RWFspProbeFreeStack[dtid.x + 1] = probe_pool_size - 1 - dtid.x;

        const uint2 probe_2d_map_pos = FspProbeAtlasMapPos(dtid.x);
        [unroll]
        for(int oct_j = 0; oct_j < k_fsp_probe_octmap_width; ++oct_j)
        {
            [unroll]
            for(int oct_i = 0; oct_i < k_fsp_probe_octmap_width; ++oct_i)
            {
                RWFspProbeAtlasTex[probe_2d_map_pos * k_fsp_probe_octmap_width + uint2(oct_i, oct_j)] = 0.0.xxxx;
            }
        }
    }
}
