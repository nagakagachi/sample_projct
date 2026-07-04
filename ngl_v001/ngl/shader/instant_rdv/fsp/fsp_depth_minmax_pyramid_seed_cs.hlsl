#if 0
fsp_depth_minmax_pyramid_seed_cs.hlsl

Cell-driven可視セル判定の前段。
full-res depthをview距離へ変換し、depth min/max pyramid の mip0 を初期化する。
#endif

#define FSP_DEPTH_PYRAMID_SEED_THREAD_GROUP_SIZE 8

#include "../instant_rdv_util.hlsli"
#include "../../include/scene_view_struct.hlsli"

ConstantBuffer<SceneViewInfo> cb_ngl_sceneview;
Texture2D TexHardwareDepth;
RWTexture2D<float2> RWFspDepthMinMaxPyramidMip0Tex;

[numthreads(FSP_DEPTH_PYRAMID_SEED_THREAD_GROUP_SIZE, FSP_DEPTH_PYRAMID_SEED_THREAD_GROUP_SIZE, 1)]
void main_cs(
    uint3 dtid : SV_DispatchThreadID,
    uint3 gtid : SV_GroupThreadID,
    uint3 gid : SV_GroupID,
    uint gindex : SV_GroupIndex)
{
    // mip0はfull-res depthと1:1対応。以降のmipはdownsample passで構築する。
    const uint2 depth_size = uint2(cb_instant_rdv.tex_main_view_depth_size.xy);
    if(any(dtid.xy >= depth_size))
    {
        return;
    }

    // hardware depth -> view距離へ正規化しておくことで、
    // Reverse-Z / projection差異を後段交差判定から切り離す。
    const float d = TexHardwareDepth.Load(int3(dtid.xy, 0)).r;
    const float view_z = calc_view_z_from_ndc_z(d, cb_ngl_sceneview.cb_ndc_z_to_view_z_coef);
    const float view_distance = abs(view_z);

    // sky/無効depthは未使用値として格納し、後段の交差判定から除外する。
    if(view_distance >= 65535.0)
    {
        RWFspDepthMinMaxPyramidMip0Tex[dtid.xy] = float2(3.402823466e+38, -3.402823466e+38);
    }
    else
    {
        RWFspDepthMinMaxPyramidMip0Tex[dtid.xy] = float2(view_distance, view_distance);
    }
}
