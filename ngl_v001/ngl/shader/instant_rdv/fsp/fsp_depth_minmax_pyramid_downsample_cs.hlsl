#if 0
fsp_depth_minmax_pyramid_downsample_cs.hlsl

Depth min/max pyramid 構築パス。
- dst_mip == 0: full-res hardware depth を 2x2 集約して half-res mip0 を作る
- dst_mip >= 1: 前段mipを 2x2 集約して下位mipへ伝搬する
#endif

#define FSP_DEPTH_PYRAMID_DOWNSAMPLE_THREAD_GROUP_SIZE 8

#include "../instant_rdv_util.hlsli"
#include "../../include/scene_view_struct.hlsli"

// srcはUAVで読み書きし、全resourceをUnorderedAccess layoutに統一する。
// first step(dst_mip==0)はTexHardwareDepthを使い、以降はRWFspDepthMinMaxPyramidSrcTexを使う。
Texture2D TexHardwareDepth;
ConstantBuffer<SceneViewInfo> cb_ngl_sceneview;
RWTexture2D<float2> RWFspDepthMinMaxPyramidSrcTex;
RWTexture2D<float2> RWFspDepthMinMaxPyramidDstTex;

[numthreads(FSP_DEPTH_PYRAMID_DOWNSAMPLE_THREAD_GROUP_SIZE, FSP_DEPTH_PYRAMID_DOWNSAMPLE_THREAD_GROUP_SIZE, 1)]
void main_cs(
    uint3 dtid : SV_DispatchThreadID,
    uint3 gtid : SV_GroupThreadID,
    uint3 gid : SV_GroupID,
    uint gindex : SV_GroupIndex)
{
    // Dispatchはdst解像度基準。端数pixelが出るmipでは早期returnで安全にガードする。
    uint2 dst_size = 0;
    RWFspDepthMinMaxPyramidDstTex.GetDimensions(dst_size.x, dst_size.y);
    if(any(dtid.xy >= dst_size))
    {
        return;
    }

    uint2 src_size = 0;
    RWFspDepthMinMaxPyramidSrcTex.GetDimensions(src_size.x, src_size.y);
    const uint2 depth_size = uint2(cb_instant_rdv.tex_main_view_depth_size.xy);
    // mip0生成判定は「dst解像度がfull-resの1/2か」で決める。
    // src/dst viewの一致判定に依存すると、バインド変更時に誤判定しやすいため明示条件へ変更する。
    const uint2 expected_mip0_size = uint2((depth_size.x + 1u) / 2u, (depth_size.y + 1u) / 2u);
    const bool is_building_mip0_from_full_res_depth = all(dst_size == expected_mip0_size);

    // dst 1texelは src 2x2 を代表する。
    const uint2 src_base = dtid.xy * 2u;
    float depth_min = 3.402823466e+38;
    float depth_max = -3.402823466e+38;
    bool has_valid_depth = false;

    // 有効rangeは min<=max のときのみ。未使用range(min=+inf,max=-inf)は除外する。
    [unroll]
    for(uint oy = 0u; oy < 2u; ++oy)
    {
        [unroll]
        for(uint ox = 0u; ox < 2u; ++ox)
        {
            const uint2 src_pos = src_base + uint2(ox, oy);
            float2 depth_range = float2(3.402823466e+38, -3.402823466e+38);
            if(is_building_mip0_from_full_res_depth)
            {
                if(any(src_pos >= depth_size))
                {
                    continue;
                }

                const float d = TexHardwareDepth.Load(int3(src_pos, 0)).r;
                const float view_z = calc_view_z_from_ndc_z(d, cb_ngl_sceneview.cb_ndc_z_to_view_z_coef);
                const float view_distance = abs(view_z);
                if(view_distance < 65535.0)
                {
                    depth_range = float2(view_distance, view_distance);
                }
            }
            else
            {
                if(any(src_pos >= src_size))
                {
                    continue;
                }
                depth_range = RWFspDepthMinMaxPyramidSrcTex.Load(int3(src_pos, 0));
            }

            if(depth_range.x <= depth_range.y)
            {
                depth_min = min(depth_min, depth_range.x);
                depth_max = max(depth_max, depth_range.y);
                has_valid_depth = true;
            }
        }
    }

    // 2x2すべてが無効depthなら、未使用rangeを維持して上位mipへ伝搬する。
    RWFspDepthMinMaxPyramidDstTex[dtid.xy] = has_valid_depth ? float2(depth_min, depth_max) : float2(3.402823466e+38, -3.402823466e+38);
}
