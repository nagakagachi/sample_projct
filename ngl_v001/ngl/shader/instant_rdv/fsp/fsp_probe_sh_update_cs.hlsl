#if 0

fsp_probe_sh_update_cs.hlsl

ファイル説明:
 FrustumSurfaceProbe の OctMap atlas から SkyVisibility + Radiance の L1 SH を作り、
 cascaded dense IrradianceVolume へ global cell index 直結で書き込む。
 coeff order:
   0 = Y00
   1 = Y1_{-1}(y)
   2 = Y1_0(z)
   3 = Y1_{+1}(x)
 packed RGBA:
   R = SkyVisibility coeff
   G = Radiance R coeff
   B = Radiance G coeff
   A = Radiance B coeff

#endif

#include "../instant_rdv_util.hlsli"

[numthreads(PROBE_UPDATE_THREAD_GROUP_SIZE, 1, 1)]
void main_cs(
    uint3 dtid : SV_DispatchThreadID,
    uint3 gtid : SV_GroupThreadID,
    uint3 gid : SV_GroupID,
    uint gindex : SV_GroupIndex)
{
    const uint active_probe_count = FspActiveProbeListCurr[0];
    if(dtid.x >= active_probe_count)
    {
        return;
    }

    const uint probe_index = FspActiveProbeListCurr[dtid.x + 1];
    if(probe_index >= cb_instant_rdv.fsp_probe_pool_size)
    {
        return;
    }

    const FspProbePoolData probe_pool_data = FspProbePoolBuffer[probe_index];
    if(probe_pool_data.owner_cell_index == k_fsp_invalid_probe_index)
    {
        return;
    }

    float4 packed_sh_coeff0 = 0.0.xxxx;
    float4 packed_sh_coeff1 = 0.0.xxxx;
    float4 packed_sh_coeff2 = 0.0.xxxx;
    float4 packed_sh_coeff3 = 0.0.xxxx;

    [unroll]
    for(int oy = 0; oy < k_fsp_probe_octmap_width; ++oy)
    {
        [unroll]
        for(int ox = 0; ox < k_fsp_probe_octmap_width; ++ox)
        {
            const uint2 atlas_texel_pos = FspProbeAtlasTexelCoord(probe_index, uint2(ox, oy));
            const float4 fsp_probe_value = FspProbeAtlasTex.Load(int3(atlas_texel_pos, 0));
            const float4 packed_sample = float4(fsp_probe_value.a, fsp_probe_value.rgb);

            const float2 oct_uv = (float2(float(ox), float(oy)) + 0.5.xx) / float(k_fsp_probe_octmap_width);
            const float3 dir_ws = OctDecode(oct_uv);
            const float4 sh_basis = EvaluateL1ShBasis(dir_ws);

            packed_sh_coeff0 += packed_sample * sh_basis.x;
            packed_sh_coeff1 += packed_sample * sh_basis.y;
            packed_sh_coeff2 += packed_sample * sh_basis.z;
            packed_sh_coeff3 += packed_sample * sh_basis.w;
        }
    }

    const float texel_solid_angle = (4.0 * 3.14159265359) / float(k_fsp_probe_octmap_width * k_fsp_probe_octmap_width);
    const uint global_cell_index = probe_pool_data.owner_cell_index;
    // Probe atlas はRT resolve用の中間履歴で、最終シェーディング用SHはowner cellのdense volumeへ集約する。
    RWFspIrradianceVolumeSHBuffer[FspIrradianceVolumeSHAddress(global_cell_index, 0)] = packed_sh_coeff0 * texel_solid_angle;
    RWFspIrradianceVolumeSHBuffer[FspIrradianceVolumeSHAddress(global_cell_index, 1)] = packed_sh_coeff1 * texel_solid_angle;
    RWFspIrradianceVolumeSHBuffer[FspIrradianceVolumeSHAddress(global_cell_index, 2)] = packed_sh_coeff2 * texel_solid_angle;
    RWFspIrradianceVolumeSHBuffer[FspIrradianceVolumeSHAddress(global_cell_index, 3)] = packed_sh_coeff3 * texel_solid_angle;
}
