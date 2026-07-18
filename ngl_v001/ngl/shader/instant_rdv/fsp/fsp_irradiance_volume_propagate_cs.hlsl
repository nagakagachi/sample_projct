#if 0

fsp_irradiance_volume_propagate_cs.hlsl
ファイル説明:
 ActiveProbeで直接更新されなかったFSP IrradianceVolumeセルへ、
 同一cascade内の6近傍からSHをcheckerboard伝播する。

#endif

#include "../instant_rdv_util.hlsli"

bool FspIsCellCenterOccupied(uint cascade_index, uint local_cell_index)
{
    const float3 cell_center_ws = FspCalcCellCenterWs(cascade_index, local_cell_index);
    return read_bbv_voxel_from_world_pos(
        BitmaskBrickVoxel,
        cb_instant_rdv.bbv.grid_resolution,
        cb_instant_rdv.bbv.grid_toroidal_offset,
        cb_instant_rdv.bbv.grid_min_pos,
        cb_instant_rdv.bbv.cell_size_inv,
        cell_center_ws) != 0u;
}

uint FspLinearCoordToLocalCellIndex(int3 linear_coord, FspCascadeGridParam cascade)
{
    const int3 toroidal_coord = voxel_coord_toroidal_mapping(
        linear_coord,
        cascade.grid.grid_toroidal_offset,
        cascade.grid.grid_resolution);
    return voxel_coord_to_index(toroidal_coord, cascade.grid.grid_resolution);
}

bool FspTryLoadNeighborSH(
    out float4 out_coeff0,
    out float4 out_coeff1,
    out float4 out_coeff2,
    out float4 out_coeff3,
    uint cascade_index,
    int3 neighbor_linear_coord)
{
    out_coeff0 = 0.0.xxxx;
    out_coeff1 = 0.0.xxxx;
    out_coeff2 = 0.0.xxxx;
    out_coeff3 = 0.0.xxxx;

    const FspCascadeGridParam cascade = FspGetCascadeParam(cascade_index);
    if(any(neighbor_linear_coord < 0) || any(neighbor_linear_coord >= cascade.grid.grid_resolution))
    {
        return false;
    }

    const uint neighbor_local_cell_index = FspLinearCoordToLocalCellIndex(neighbor_linear_coord, cascade);
    const uint neighbor_global_cell_index = cascade.cell_offset + neighbor_local_cell_index;
    out_coeff0 = RWFspIrradianceVolumeSHBuffer[FspIrradianceVolumeSHAddress(neighbor_global_cell_index, 0)];
    out_coeff1 = RWFspIrradianceVolumeSHBuffer[FspIrradianceVolumeSHAddress(neighbor_global_cell_index, 1)];
    out_coeff2 = RWFspIrradianceVolumeSHBuffer[FspIrradianceVolumeSHAddress(neighbor_global_cell_index, 2)];
    out_coeff3 = RWFspIrradianceVolumeSHBuffer[FspIrradianceVolumeSHAddress(neighbor_global_cell_index, 3)];
    return FspIrradianceVolumeHasValidSHCoeff(out_coeff0, out_coeff1, out_coeff2, out_coeff3);
}

[numthreads(PROBE_UPDATE_THREAD_GROUP_SIZE, 1, 1)]
void main_cs(
    uint3 dtid : SV_DispatchThreadID,
    uint3 gtid : SV_GroupThreadID,
    uint3 gid : SV_GroupID,
    uint gindex : SV_GroupIndex)
{
    const uint global_cell_index = dtid.x;
    if(global_cell_index >= (uint)cb_instant_rdv.fsp_total_cell_count)
    {
        return;
    }

    if(FspIsActiveProbeOwnedCell(global_cell_index))
    {
        // ActiveProbeのRT結果が最優先。伝播は未Activeの空間セルを埋めるだけで、観測セルは上書きしない。
        return;
    }

    uint cascade_index = 0u;
    uint local_cell_index = 0u;
    if(!FspDecodeGlobalCellIndex(global_cell_index, cascade_index, local_cell_index))
    {
        return;
    }

    const FspCascadeGridParam cascade = FspGetCascadeParam(cascade_index);
    const int3 linear_coord = FspLocalCellIndexToLinearCoord(local_cell_index, cascade.grid);
    const uint cell_parity = uint((linear_coord.x + linear_coord.y + linear_coord.z) & 1);
    // parityをフレームごとに切り替え、6近傍読みと同時書きの衝突を避ける。
    if(cell_parity != (cb_instant_rdv.frame_count & 1u))
    {
        return;
    }

    if(FspIsCellCenterOccupied(cascade_index, local_cell_index))
    {
        // 不透明セル内部は注入点ではないため、近傍伝播でSHを作らない。
        return;
    }

    const int3 neighbor_offsets[6] =
    {
        int3( 1,  0,  0),
        int3(-1,  0,  0),
        int3( 0,  1,  0),
        int3( 0, -1,  0),
        int3( 0,  0,  1),
        int3( 0,  0, -1),
    };

    float4 accum_coeff0 = 0.0.xxxx;
    float4 accum_coeff1 = 0.0.xxxx;
    float4 accum_coeff2 = 0.0.xxxx;
    float4 accum_coeff3 = 0.0.xxxx;
    uint valid_neighbor_count = 0u;

    [unroll]
    for(uint neighbor_index = 0u; neighbor_index < 6u; ++neighbor_index)
    {
        float4 coeff0 = 0.0.xxxx;
        float4 coeff1 = 0.0.xxxx;
        float4 coeff2 = 0.0.xxxx;
        float4 coeff3 = 0.0.xxxx;
        if(!FspTryLoadNeighborSH(
            coeff0,
            coeff1,
            coeff2,
            coeff3,
            cascade_index,
            linear_coord + neighbor_offsets[neighbor_index]))
        {
            continue;
        }

        accum_coeff0 += coeff0;
        accum_coeff1 += coeff1;
        accum_coeff2 += coeff2;
        accum_coeff3 += coeff3;
        ++valid_neighbor_count;
    }

    if(valid_neighbor_count == 0u)
    {
        return;
    }

    const float inv_count = rcp(float(valid_neighbor_count));
    RWFspIrradianceVolumeSHBuffer[FspIrradianceVolumeSHAddress(global_cell_index, 0)] = accum_coeff0 * inv_count;
    RWFspIrradianceVolumeSHBuffer[FspIrradianceVolumeSHAddress(global_cell_index, 1)] = accum_coeff1 * inv_count;
    RWFspIrradianceVolumeSHBuffer[FspIrradianceVolumeSHAddress(global_cell_index, 2)] = accum_coeff2 * inv_count;
    RWFspIrradianceVolumeSHBuffer[FspIrradianceVolumeSHAddress(global_cell_index, 3)] = accum_coeff3 * inv_count;
}
