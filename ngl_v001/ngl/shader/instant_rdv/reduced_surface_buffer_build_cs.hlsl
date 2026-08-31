/*
    reduced_surface_buffer_build_cs.hlsl

    MainView Depthを4x4タイルごとにJitter sampleし、view Zと近似法線を生成する。
    出力: x=view Z, yz=world normal oct, w=normal confidence。
*/

#define TILE_WIDTH 8

#include "instant_rdv_util.hlsli"
#include "../include/scene_view_struct.hlsli"

ConstantBuffer<BbvSurfaceInjectionViewInfo> cb_injection_src_view_info;
Texture2D TexHardwareDepth;
RWTexture2D<float4> RWReducedSurfaceBuffer;

bool LoadViewPosition(int2 texel, int2 source_resolution, out float3 position_vs)
{
    texel = clamp(texel, int2(0, 0), source_resolution - 1);
    const float depth = TexHardwareDepth.Load(int3(
        texel + cb_injection_src_view_info.cb_view_depth_buffer_offset_size.xy,
        0)).r;
    if(!isValidDepth(depth))
    {
        position_vs = 0.0.xxx;
        return false;
    }

    const float2 uv =
        (float2(texel) + 0.5.xx) / float2(source_resolution);
    const float view_z = calc_view_z_from_ndc_z(
        depth,
        cb_injection_src_view_info.cb_ndc_z_to_view_z_coef);
    position_vs = CalcViewSpacePosition(
        uv,
        view_z,
        cb_injection_src_view_info.cb_proj_mtx);
    return true;
}

bool SelectStableTangent(
    float3 center_vs,
    bool has_negative,
    float3 negative_vs,
    bool has_positive,
    float3 positive_vs,
    out float3 tangent_vs)
{
    const float negative_delta =
        has_negative ? abs(center_vs.z - negative_vs.z) : 1e30;
    const float positive_delta =
        has_positive ? abs(positive_vs.z - center_vs.z) : 1e30;
    const bool use_positive = positive_delta <= negative_delta;
    const float selected_delta = min(negative_delta, positive_delta);
    const float discontinuity_limit = max(0.25, abs(center_vs.z) * 0.02);
    if(selected_delta > discontinuity_limit)
    {
        tangent_vs = 0.0.xxx;
        return false;
    }

    tangent_vs = use_positive
        ? positive_vs - center_vs
        : center_vs - negative_vs;
    return dot(tangent_vs, tangent_vs) > 1e-10;
}

[numthreads(TILE_WIDTH, TILE_WIDTH, 1)]
void main_cs(uint3 dtid : SV_DispatchThreadID)
{
    uint output_width;
    uint output_height;
    RWReducedSurfaceBuffer.GetDimensions(output_width, output_height);
    if(any(dtid.xy >= uint2(output_width, output_height)))
    {
        return;
    }

    const int2 source_resolution =
        cb_injection_src_view_info.cb_view_depth_buffer_offset_size.zw;
    const uint2 source_texel = ReducedSurfaceBufferSourceTexel(
        dtid.xy,
        uint2(source_resolution),
        cb_instant_rdv.frame_count);

    float3 center_vs;
    if(!LoadViewPosition(int2(source_texel), source_resolution, center_vs))
    {
        RWReducedSurfaceBuffer[dtid.xy] = 0.0.xxxx;
        return;
    }

    float3 negative_x_vs;
    float3 positive_x_vs;
    float3 negative_y_vs;
    float3 positive_y_vs;
    const bool has_negative_x = LoadViewPosition(
        int2(source_texel) + int2(-1, 0),
        source_resolution,
        negative_x_vs);
    const bool has_positive_x = LoadViewPosition(
        int2(source_texel) + int2(1, 0),
        source_resolution,
        positive_x_vs);
    const bool has_negative_y = LoadViewPosition(
        int2(source_texel) + int2(0, -1),
        source_resolution,
        negative_y_vs);
    const bool has_positive_y = LoadViewPosition(
        int2(source_texel) + int2(0, 1),
        source_resolution,
        positive_y_vs);

    float3 tangent_x_vs;
    float3 tangent_y_vs;
    const bool has_tangent_x = SelectStableTangent(
        center_vs,
        has_negative_x,
        negative_x_vs,
        has_positive_x,
        positive_x_vs,
        tangent_x_vs);
    const bool has_tangent_y = SelectStableTangent(
        center_vs,
        has_negative_y,
        negative_y_vs,
        has_positive_y,
        positive_y_vs,
        tangent_y_vs);

    const float center_len_sq = dot(center_vs, center_vs);
    const float3 view_ray_vs = center_len_sq > 1e-10
        ? center_vs * rsqrt(center_len_sq)
        : float3(0.0, 0.0, 1.0);
    float3 normal_vs = -view_ray_vs;
    float normal_confidence = 0.0;
    if(has_tangent_x && has_tangent_y)
    {
        const float3 candidate_normal_vs = cross(tangent_x_vs, tangent_y_vs);
        const float candidate_len_sq =
            dot(candidate_normal_vs, candidate_normal_vs);
        if(candidate_len_sq > 1e-10)
        {
            normal_vs = candidate_normal_vs * rsqrt(candidate_len_sq);
            if(dot(normal_vs, -view_ray_vs) < 0.0)
            {
                normal_vs = -normal_vs;
            }
            normal_confidence = 1.0;
        }
    }

    const float3 normal_ws = normalize(mul(
        cb_injection_src_view_info.cb_view_inv_mtx,
        float4(normal_vs, 0.0)));
    RWReducedSurfaceBuffer[dtid.xy] = float4(
        center_vs.z,
        OctEncode(normal_ws),
        normal_confidence);
}
