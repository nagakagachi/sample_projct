#if 0
bbv_removal_carving_cs.hlsl

BBV Removal用のCarving。
Frustum 候補 Brick に対して bitcell 単位の深度テストを行い、
手前側の fine voxel だけを削る。
#endif

#include "../instant_rdv_util.hlsli"
// calc_view_z_from_ndc_z 定義.
#include "../../include/scene_view_struct.hlsli"

ConstantBuffer<BbvSurfaceInjectionViewInfo> cb_injection_src_view_info;
Texture2D TexHardwareDepth;

[numthreads(k_bbv_removal_carving_thread_group_size, 1, 1)]
void main_cs(uint3 dtid : SV_DispatchThreadID)
{
    const uint active_brick_count = FrustumBrickList[0];
    // 最適化メモ:
    // 旧方式(1thread=1brick)は、1 Brick内16u32をthread内で直列処理していた。
    // 現方式は 1thread=1u32 に分解し、同一Brickをwave内で協調して処理する。
    // これにより、MainViewの実測でCarving処理時間が大幅に短縮した。
    // 1 Brick は 16 u32 (8x8x8=512bit) なので、wave32 なら 2 Brick、wave64 なら 4 Brick を同時に扱える。
    const uint u32_per_brick = k_bbv_per_voxel_bitmask_u32_count;
    const uint total_u32_jobs = active_brick_count * u32_per_brick;
    if(dtid.x >= total_u32_jobs)
    {
        return;
    }

    // ActiveList形式: 0番はcounter、1番以降は有効 voxel index。
    // dtid.x は「Brick番号」ではなく「(Brick, u32_offset) のジョブ番号」として解釈する。
    const uint brick_list_index = dtid.x / u32_per_brick;
    const uint u32_offset = dtid.x - brick_list_index * u32_per_brick;
    const uint lane_voxel_index = FrustumBrickList[brick_list_index + 1];

    // wave内で同一Brick(16 lane単位)は同じ voxel_index を共有する。
    // lane_index / 16 で「このlaneがwave内の何番目のBrick担当か」を求め、
    // そのBrick担当の先頭lane(0 or 16 or 32 or 48)から共通値を受け取る。
    // 例: wave32 では [0..15]=BrickA, [16..31]=BrickB。
    const uint lane_index = WaveGetLaneIndex();
    const uint slot_first_lane = (lane_index / u32_per_brick) * u32_per_brick;
    // 同一Brick担当slotの代表laneが持つ voxel_index を取得する。
    // 各laneの lane_voxel_index は同じ値のはずだが、明示的に代表lane値へ揃えて
    // 以降のアドレス計算を同一基準で行う。
    const uint voxel_index = WaveReadLaneAt(lane_voxel_index, slot_first_lane);

    int3 voxel_coord_linear = int3(0, 0, 0);
    // 座標変換は同一Brick内で共通なので、代表laneのみ計算して共有する。
    // 全laneが同じ結果を使うことで、無駄なindex_to_voxel_coordを削減する。
    if(lane_index == slot_first_lane)
    {
        const int3 voxel_coord_toroidal =
            BbvMortonIndexToPhysicalVoxelCoord(voxel_index, cb_instant_rdv.bbv.grid_resolution);
        voxel_coord_linear = voxel_coord_toroidal_mapping(
            voxel_coord_toroidal,
            cb_instant_rdv.bbv.grid_resolution - cb_instant_rdv.bbv.grid_toroidal_offset,
            cb_instant_rdv.bbv.grid_resolution);
    }
    // 代表laneで計算した voxel_coord_linear を同一Brick担当laneへ配布する。
    // x/y/z を個別に読むことで、同じ座標を共有しつつ再計算を避ける。
    voxel_coord_linear.x = WaveReadLaneAt(voxel_coord_linear.x, slot_first_lane);
    voxel_coord_linear.y = WaveReadLaneAt(voxel_coord_linear.y, slot_first_lane);
    voxel_coord_linear.z = WaveReadLaneAt(voxel_coord_linear.z, slot_first_lane);

    const float3 brick_origin_ws = (float3(voxel_coord_linear) * cb_instant_rdv.bbv.cell_size) + cb_instant_rdv.bbv.grid_min_pos;
    const float3 bitcell_step_ws = cb_instant_rdv.bbv.cell_size * k_bbv_per_voxel_resolution_inv;
    const int2 depth_size = cb_injection_src_view_info.cb_view_depth_buffer_offset_size.zw;
    const int2 depth_atlas_offset = cb_injection_src_view_info.cb_view_depth_buffer_offset_size.xy;

    const uint bbv_addr = bbv_voxel_bitmask_data_addr(voxel_index);
    uint bit_block = RWBitmaskBrickVoxel[bbv_addr + u32_offset];
    // このwaveで「処理すべきu32(bit_block!=0)」が1つも無い場合は、
    // 以降の分岐・座標計算・深度参照を丸ごとスキップする。
    // 全lane同じ分岐(Uniform branch)になるため発散を抑えられる。
    if(0 == WaveActiveCountBits(0 != bit_block))
    {
        return;
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
        // セル中心1点ではなく、セル外接球の「カメラ側最近点」で前後判定する。
        // これにより中心が奥側でも手前側へはみ出した占有を削りやすくする。
        const float center_len_sq = dot(bitcell_center_vs, bitcell_center_vs);
        const float3 center_dir = (center_len_sq > 1e-10) ? (bitcell_center_vs * rsqrt(center_len_sq)) : float3(0.0, 0.0, 1.0);
        const float bitcell_sphere_radius = 0.5 * length(bitcell_step_ws);
        const float3 bitcell_nearest_vs = bitcell_center_vs - center_dir * bitcell_sphere_radius;
        const float bitcell_nearest_view_z = bitcell_nearest_vs.z;
        if(bitcell_nearest_view_z < surface_view_z)
        {
            remain_mask &= ~bit_value;
        }
    }

    RWBitmaskBrickVoxel[bbv_addr + u32_offset] = remain_mask;
}
