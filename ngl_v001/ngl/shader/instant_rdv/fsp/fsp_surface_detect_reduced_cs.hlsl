/*
    fsp_surface_detect_reduced_cs.hlsl

    ReducedSurfaceBufferからFSP SurfaceCellを検出する。
    Cellを初めて検出したthreadが、Cell indexと検出元texelを同じslotへ出力する。
*/

#define FSP_SURFACE_DETECT_TILE_WIDTH 8

#include "../instant_rdv_util.hlsli"
#include "../../include/scene_view_struct.hlsli"

ConstantBuffer<SceneViewInfo> cb_ngl_sceneview;
Texture2D<float4> TexReducedSurfaceBuffer;
RWBuffer<uint> RWSurfaceProbeSourceTexelList;

bool FspReserveSurfaceListSlot(out uint list_index)
{
    list_index = 0u;
    const uint capacity =
        (uint)max(cb_instant_rdv.fsp_visible_voxel_buffer_size, 0);
    for(;;)
    {
        const uint observed_count = RWSurfaceProbeCellList[0];
        if(observed_count >= capacity)
        {
            return false;
        }

        uint original_count = 0u;
        InterlockedCompareExchange(
            RWSurfaceProbeCellList[0],
            observed_count,
            observed_count + 1u,
            original_count);
        if(original_count == observed_count)
        {
            list_index = observed_count;
            return true;
        }
    }
}

void FspDetectSurfaceCell(
    uint global_cell_index,
    uint reduced_surface_texel_index)
{
    uint mask_word_index = 0u;
    uint mask_bit = 0u;
    if(!FspGetSurfaceMaskAddress(
        global_cell_index,
        mask_word_index,
        mask_bit))
    {
        return;
    }

    uint original_mask = 0u;
    InterlockedOr(
        RWFspSurfaceCellMaskBuffer[mask_word_index],
        mask_bit,
        original_mask);
    if((original_mask & mask_bit) != 0u)
    {
        return;
    }

    uint list_index = 0u;
    if(!FspReserveSurfaceListSlot(list_index))
    {
        return;
    }

    RWSurfaceProbeCellList[list_index + 1u] = global_cell_index;
    RWSurfaceProbeSourceTexelList[list_index + 1u] =
        reduced_surface_texel_index;
}

[numthreads(
    FSP_SURFACE_DETECT_TILE_WIDTH,
    FSP_SURFACE_DETECT_TILE_WIDTH,
    1)]
void main_cs(uint3 dtid : SV_DispatchThreadID)
{
    uint reduced_width = 0u;
    uint reduced_height = 0u;
    TexReducedSurfaceBuffer.GetDimensions(
        reduced_width,
        reduced_height);
    if(any(dtid.xy >= uint2(reduced_width, reduced_height)))
    {
        return;
    }

    const float4 surface_sample =
        TexReducedSurfaceBuffer.Load(int3(dtid.xy, 0));
    if(surface_sample.x <= 0.0)
    {
        return;
    }

    const uint2 source_resolution =
        uint2(cb_instant_rdv.tex_main_view_depth_size);
    const uint2 source_texel = ReducedSurfaceBufferSourceTexel(
        dtid.xy,
        source_resolution,
        cb_instant_rdv.frame_count);
    const float2 source_uv =
        (float2(source_texel) + 0.5.xx) /
        float2(source_resolution);
    const float3 surface_pos_vs = CalcViewSpacePosition(
        source_uv,
        surface_sample.x,
        cb_ngl_sceneview.cb_proj_mtx);
    const float3 surface_pos_ws = mul(
        cb_ngl_sceneview.cb_view_inv_mtx,
        float4(surface_pos_vs, 1.0));

    uint2 owner_cell_indices = k_fsp_invalid_probe_index.xx;
    const uint owner_count = FspGetSurfaceOwnerCells(
        surface_pos_ws,
        owner_cell_indices);
    const uint reduced_surface_texel_index =
        dtid.x + dtid.y * reduced_width;
    if(owner_count > 0u)
    {
        FspDetectSurfaceCell(
            owner_cell_indices.x,
            reduced_surface_texel_index);
    }
    if(owner_count > 1u)
    {
        FspDetectSurfaceCell(
            owner_cell_indices.y,
            reduced_surface_texel_index);
    }
}
