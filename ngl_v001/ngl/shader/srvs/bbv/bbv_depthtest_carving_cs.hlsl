#if 0
bbv_depthtest_carving_cs.hlsl

DepthTestCarving ベース更新向けの Carving。
Frustum 候補 Brick に対して bitcell 単位の深度テストを行い、
手前側の fine voxel だけを削る。
#endif

#include "../srvs_util.hlsli"
// calc_view_z_from_ndc_z 定義.
#include "../../include/scene_view_struct.hlsli"

ConstantBuffer<BbvSurfaceInjectionViewInfo> cb_injection_src_view_info;
Texture2D TexHardwareDepth;

[numthreads(k_bbv_depthtest_carving_thread_group_size, 1, 1)]
void main_cs(uint3 dtid : SV_DispatchThreadID)
{
    const uint active_brick_count = FrustumBrickList[0];
    if(dtid.x >= active_brick_count)
    {
        return;
    }

    // ActiveList形式: 0番はcounter、1番以降は有効 voxel index。
    const uint voxel_index = FrustumBrickList[dtid.x + 1];
    const int3 voxel_coord_toroidal = index_to_voxel_coord(voxel_index, cb_srvs.bbv.grid_resolution);
    const int3 voxel_coord_linear = voxel_coord_toroidal_mapping(
        voxel_coord_toroidal,
        cb_srvs.bbv.grid_resolution - cb_srvs.bbv.grid_toroidal_offset,
        cb_srvs.bbv.grid_resolution);
    const float3 brick_origin_ws = (float3(voxel_coord_linear) * cb_srvs.bbv.cell_size) + cb_srvs.bbv.grid_min_pos;
    const float3 bitcell_step_ws = cb_srvs.bbv.cell_size * k_bbv_per_voxel_resolution_inv;
    const int2 depth_size = cb_injection_src_view_info.cb_view_depth_buffer_offset_size.zw;
    const int2 depth_atlas_offset = cb_injection_src_view_info.cb_view_depth_buffer_offset_size.xy;

    const uint bbv_addr = bbv_voxel_bitmask_data_addr(voxel_index);
    [unroll]
    for(uint u32_offset = 0; u32_offset < k_bbv_per_voxel_bitmask_u32_count; ++u32_offset)
    {
        uint bit_block = RWBitmaskBrickVoxel[bbv_addr + u32_offset];
        // wave全体でこのu32ブロックに有効bitが1つも無ければ即skipする。
        // laneごとの分岐ばらつきを減らし、空ブロック処理をまとめて落とす。
        if(0 == WaveActiveCountBits(0 != bit_block))
        {
            continue;
        }

        uint remain_mask = bit_block;
        // 空bitをなめず、立っているbitのみ処理する。
        uint scan_mask = bit_block;
        [loop]
        while(0 != scan_mask)
        {
            const uint bit_in_u32 = firstbitlow(scan_mask);
            const uint bit_value = (1u << bit_in_u32);
            // continue経路でも必ず前進するよう、先に走査済みbitを落とす。
            scan_mask &= (scan_mask - 1);

            const uint bit_index = u32_offset * 32 + bit_in_u32;
            const uint3 bitcell_pos = calc_bbv_bitcell_pos_from_bit_index(bit_index);
            const float3 bitcell_center_ws =
                brick_origin_ws + (float3(bitcell_pos) + 0.5) * bitcell_step_ws;
            const float3 bitcell_center_vs = mul(cb_injection_src_view_info.cb_view_mtx, float4(bitcell_center_ws, 1.0));
            const float4 bitcell_center_cs = mul(cb_injection_src_view_info.cb_proj_mtx, float4(bitcell_center_vs, 1.0));
            if(abs(bitcell_center_cs.w) <= 1e-6)
            {
                continue;
            }

            const float3 ndc = bitcell_center_cs.xyz / bitcell_center_cs.w;
            // 各Viewの射影空間で前後範囲外のセルはCarving対象にしない。
            // Main/Shadowともに同一ロジックで「そのViewに映っていない前後」を保持する。
            if(ndc.z < 0.0 || ndc.z > 1.0)
            {
                continue;
            }
            const float2 uv = float2(ndc.x * 0.5 + 0.5, -ndc.y * 0.5 + 0.5);
            if(any(uv < 0.0) || any(uv > 1.0))
            {
                continue;
            }

            const int2 screen_pos = clamp(int2(uv * float2(depth_size)), int2(0, 0), depth_size - 1);
            const int2 atlas_pos = screen_pos + depth_atlas_offset;
            const float surface_depth = TexHardwareDepth.Load(int3(atlas_pos, 0)).r;
            if(!isValidDepth(surface_depth))
            {
                // no-depth時の挙動はView種別で分離する。
                // MainView: 空領域の残留occupancy掃除のため除去する。
                // ShadowView: atlas未カバー領域などでの過剰除去を避けるため除去しない。
                if(0 != cb_injection_src_view_info.cb_is_main_view)
                {
                    remain_mask &= ~bit_value;
                }
                continue;
            }

            const float surface_view_z = calc_view_z_from_ndc_z(surface_depth, cb_injection_src_view_info.cb_ndc_z_to_view_z_coef);
            const float bitcell_view_z = bitcell_center_vs.z;
            if(bitcell_view_z < surface_view_z)
            {
                remain_mask &= ~bit_value;
            }
        }

        RWBitmaskBrickVoxel[bbv_addr + u32_offset] = remain_mask;
    }
}
