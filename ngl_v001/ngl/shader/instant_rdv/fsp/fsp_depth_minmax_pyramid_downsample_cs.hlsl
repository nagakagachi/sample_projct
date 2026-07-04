#if 0
fsp_depth_minmax_pyramid_downsample_cs.hlsl

Depth min/max pyramid の1段downsample。
srcの2x2範囲を集約してdstへmin/maxを伝搬する。
#endif

#define FSP_DEPTH_PYRAMID_DOWNSAMPLE_THREAD_GROUP_SIZE 8

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

    // dst 1texelは src 2x2 を代表する。
    const uint2 src_base = dtid.xy * 2u;
    float depth_min = 3.402823466e+38;
    float depth_max = -3.402823466e+38;
    bool has_valid_depth = false;

    // 有効rangeは min<=max のときのみ。seedで埋めた未使用range(min=+inf,max=-inf)は除外する。
    [unroll]
    for(uint oy = 0u; oy < 2u; ++oy)
    {
        [unroll]
        for(uint ox = 0u; ox < 2u; ++ox)
        {
            const uint2 src_pos = src_base + uint2(ox, oy);
            if(any(src_pos >= src_size))
            {
                continue;
            }

            const float2 depth_range = RWFspDepthMinMaxPyramidSrcTex.Load(int3(src_pos, 0));
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
