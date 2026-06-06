
#if 0

fsp_pre_update_cs.hlsl

可視SurfaceProbeリストを元に、probe 割り当てと配置調整を行う.

#endif


#include "../srvs_util.hlsli"
#include "../../include/scene_view_struct.hlsli"


#define RAY_SAMPLE_COUNT_PER_VOXEL 8
#define PROBE_UPDATE_TEMPORAL_RATE (0.025)
static const uint k_fsp_depth_hint_metric_init = 0xffffffffu;
static const uint k_fsp_depth_hint_pixel_bits = 24u;
// packed key lower bits から pixel_id を取り出すためのマスク。
static const uint k_fsp_depth_hint_pixel_mask = (1u << k_fsp_depth_hint_pixel_bits) - 1u;

ConstantBuffer<SceneViewInfo> cb_ngl_sceneview;
Texture2D         TexHardwareDepth;

// free stack から 1 probe index を pop する。
uint FspPopFreeProbeIndex()
{
    for(;;)
    {
        const uint observed_count = RWFspProbeFreeStack[0];
        if(observed_count == 0)
        {
            return k_fsp_invalid_probe_index;
        }

        uint cas_old_value = 0;
        InterlockedCompareExchange(RWFspProbeFreeStack[0], observed_count, observed_count - 1, cas_old_value);
        if(cas_old_value == observed_count)
        {
            return RWFspProbeFreeStack[observed_count];
        }
    }
}

// 現フレーム active list へ probe を追加する。
void FspPushCurrActiveProbeIndex(uint probe_index)
{
    uint active_list_index = 0;
    InterlockedAdd(RWFspActiveProbeListCurr[0], 1, active_list_index);
    if(active_list_index < cb_srvs.fsp_active_probe_buffer_size)
    {
        RWFspActiveProbeListCurr[active_list_index + 1] = probe_index;
    }
}

// probe atlas 1 tile を明示的にゼロ初期化する。
void FspClearProbeAtlas(uint probe_index)
{
    const uint2 probe_2d_map_pos = FspProbeAtlasMapPos(probe_index);
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

// 既存 probe の atlas 内容を新規 probe へコピーする。
void FspCopyProbeAtlas(uint dst_probe_index, uint src_probe_index)
{
    const uint2 dst_probe_2d_map_pos = FspProbeAtlasMapPos(dst_probe_index);
    const uint2 src_probe_2d_map_pos = FspProbeAtlasMapPos(src_probe_index);
    [unroll]
    for(int oct_j = 0; oct_j < k_fsp_probe_octmap_width; ++oct_j)
    {
        [unroll]
        for(int oct_i = 0; oct_i < k_fsp_probe_octmap_width; ++oct_i)
        {
            RWFspProbeAtlasTex[dst_probe_2d_map_pos * k_fsp_probe_octmap_width + uint2(oct_i, oct_j)] =
                RWFspProbeAtlasTex[src_probe_2d_map_pos * k_fsp_probe_octmap_width + uint2(oct_i, oct_j)];
        }
    }
}

bool FspTrySeedProbeFromUpperCascade(out FspProbePoolData src_probe_pool_data, float3 sample_pos_ws, uint dst_cascade_index, uint dst_probe_index)
{
    src_probe_pool_data = (FspProbePoolData)0;

    // 新規 probe の黒初期値を避けるため、同じ world pos を含む上位 cascade の
    // 安定済み probe atlas をそのまま seed として使う。
    const uint cascade_count = FspCascadeCount();
    [loop]
    for(uint src_cascade_index = dst_cascade_index + 1; src_cascade_index < cascade_count; ++src_cascade_index)
    {
        uint src_global_cell_index = k_fsp_invalid_probe_index;
        if(!FspTryGetGlobalCellIndexFromWorldPos(sample_pos_ws, src_cascade_index, src_global_cell_index))
        {
            continue;
        }

        const uint src_probe_index = RWFspCellProbeIndexBuffer[src_global_cell_index];
        if(src_probe_index == k_fsp_invalid_probe_index || src_probe_index >= (uint)cb_srvs.fsp_probe_pool_size)
        {
            continue;
        }

        const FspProbePoolData curr_src_probe_pool_data = RWFspProbePoolBuffer[src_probe_index];
        if(0 == (curr_src_probe_pool_data.flags & k_fsp_probe_flag_allocated) || curr_src_probe_pool_data.owner_cell_index != src_global_cell_index)
        {
            continue;
        }
        // 同フレームに新規割り当てされた source は atlas 初期化中の可能性があるため使わない。
        if(curr_src_probe_pool_data.last_update_frame == 0)
        {
            continue;
        }

        FspCopyProbeAtlas(dst_probe_index, src_probe_index);
        src_probe_pool_data = curr_src_probe_pool_data;
        return true;
    }

    return false;
}

[numthreads(PROBE_UPDATE_THREAD_GROUP_SIZE, 1, 1)]
void main_cs(
	uint3 dtid	: SV_DispatchThreadID,
	uint3 gtid : SV_GroupThreadID,
	uint3 gid : SV_GroupID,
	uint gindex : SV_GroupIndex
)
{
    // SurfaceProbeCellListを利用するバージョン.
    const uint visible_voxel_count = SurfaceProbeCellList[0]; // 0番目にアトミックカウンタが入っている.
    const uint update_element_index = dtid.x;
    
    if(visible_voxel_count <= update_element_index)
        return;

    const uint global_cell_index = SurfaceProbeCellList[update_element_index+1]; // 1番目以降に有効Cellインデックスが入っている.
    uint cascade_index = 0;
    uint local_cell_index = 0;
    if(!FspDecodeGlobalCellIndex(global_cell_index, cascade_index, local_cell_index))
    {
        return;
    }

    const FspCascadeGridParam cascade = FspGetCascadeParam(cascade_index);
    uint probe_index = RWFspCellProbeIndexBuffer[global_cell_index];
    const bool is_probe_lifecycle_enabled = (0 != cb_srvs.fsp_probe_lifecycle_enable);
    bool is_new_probe = false;
    if(k_fsp_invalid_probe_index == probe_index)
    {
        if(!is_probe_lifecycle_enabled)
        {
            return;
        }
        probe_index = FspPopFreeProbeIndex();
        if(k_fsp_invalid_probe_index == probe_index)
        {
            return;
        }
        RWFspCellProbeIndexBuffer[global_cell_index] = probe_index;
        FspPushCurrActiveProbeIndex(probe_index);
        is_new_probe = true;
    }

    FspProbePoolData probe_pool_data = RWFspProbePoolBuffer[probe_index];
    if(!is_probe_lifecycle_enabled)
    {
        probe_pool_data.last_seen_frame = cb_srvs.frame_count;
        probe_pool_data.debug_last_observed_frame = cb_srvs.frame_count;
        RWFspProbePoolBuffer[probe_index] = probe_pool_data;

        const uint owner_cell_index = probe_pool_data.owner_cell_index;
        if(owner_cell_index != k_fsp_invalid_probe_index)
        {
            RWFspCellStateBuffer[owner_cell_index].probe_offset_v3 = probe_pool_data.probe_offset_v3;
            RWFspCellStateBuffer[owner_cell_index].avg_sky_visibility = probe_pool_data.avg_sky_visibility;
        }
        return;
    }
    if(is_new_probe)
    {
        probe_pool_data.probe_offset_v3 = 0;
        probe_pool_data.avg_sky_visibility = 0.0;
        probe_pool_data.last_update_frame = 0;
    }
    probe_pool_data.owner_cell_index = global_cell_index;
    probe_pool_data.last_seen_frame = cb_srvs.frame_count;
    probe_pool_data.debug_last_observed_frame = cb_srvs.frame_count;
    probe_pool_data.flags |= k_fsp_probe_flag_allocated;

    // Cell中心.
    const float3 probe_cell_center = FspCalcCellCenterWs(cascade_index, local_cell_index);
    #if 1
        // Probe埋まり回避.
        const float half_cell_size = cascade.grid.cell_size * 0.5;
        const float3 prev_probe_offset = decode_uint_to_range1_vec3(probe_pool_data.probe_offset_v3) * half_cell_size;
        // ScreenSpacePass では packed key のみ保持し、ここで実 offset を再構成する。
        // 1) probe_data_dummy から pixel_id を取り出す
        // 2) pixel_id -> (x,y) へ復元して depth を再取得
        // 3) depth から world pos を復元し、hint offset を計算
        const uint depth_hint_packed = RWFspCellStateBuffer[global_cell_index].probe_data_dummy;
        bool has_depth_hint = false;
        float3 depth_hint_offset = 0.0.xxx;
        if(depth_hint_packed != k_fsp_depth_hint_metric_init)
        {
            const uint pixel_id = depth_hint_packed & k_fsp_depth_hint_pixel_mask;
            const uint depth_width = (uint)cb_srvs.tex_main_view_depth_size.x;
            const uint depth_height = (uint)cb_srvs.tex_main_view_depth_size.y;
            // 防御的に width=0 を避ける（通常は発生しない）。
            const uint pixel_x = pixel_id % max(depth_width, 1u);
            const uint pixel_y = pixel_id / max(depth_width, 1u);
            if(pixel_y < depth_height)
            {
                const float d = TexHardwareDepth.Load(int3(pixel_x, pixel_y, 0)).r;
                const float view_z = calc_view_z_from_ndc_z(d, cb_ngl_sceneview.cb_ndc_z_to_view_z_coef);
                if(65535.0 > abs(view_z))
                {
                    const float2 screen_pos_f = float2(float(pixel_x), float(pixel_y)) + float2(0.5, 0.5);
                    const float2 screen_uv = screen_pos_f / float2(cb_srvs.tex_main_view_depth_size.xy);
                    const float3 to_pixel_ray_vs = CalcViewSpaceRay(screen_uv, cb_ngl_sceneview.cb_proj_mtx);
                    const float3 pixel_pos_ws = mul(cb_ngl_sceneview.cb_view_inv_mtx, float4((to_pixel_ray_vs / abs(to_pixel_ray_vs.z)) * view_z, 1.0));
                    const float3 view_origin = GetViewOriginFromInverseViewMatrix(cb_ngl_sceneview.cb_view_inv_mtx);
                    const float3 to_surface_vec_ws = pixel_pos_ws - view_origin;
                    const float to_surface_len_sq = dot(to_surface_vec_ws, to_surface_vec_ws);
                    const bool has_surface_dir = (to_surface_len_sq > 1e-6);
                    const float3 surface_view_dir_ws = has_surface_dir ? (to_surface_vec_ws * rsqrt(to_surface_len_sq)) : 0.0.xxx;
                    const float hint_push_to_camera = half_cell_size * 0.08;
                    const float3 hint_sample_pos_ws = has_surface_dir ? (pixel_pos_ws - surface_view_dir_ws * hint_push_to_camera) : pixel_pos_ws;
                    // seed offset として使うため、セル内範囲へクランプして安定化。
                    depth_hint_offset = clamp(hint_sample_pos_ws - probe_cell_center, -half_cell_size.xxx, half_cell_size.xxx);
                    has_depth_hint = true;
                }
            }
        }

        // 深度ヒントが得られたセルはそれを優先し、無いセルのみ前フレームoffsetを継続利用。
        float3 seed_probe_offset = has_depth_hint ? depth_hint_offset : prev_probe_offset;

        float3 probe_sample_pos_ws = probe_cell_center + seed_probe_offset;

        {
            const int fallback_relocation_count = 4;
            bool found_non_solid = false;

            if(read_bbv_voxel_from_world_pos(BitmaskBrickVoxel, cb_srvs.bbv.grid_resolution, cb_srvs.bbv.grid_toroidal_offset, cb_srvs.bbv.grid_min_pos, cb_srvs.bbv.cell_size_inv, probe_sample_pos_ws) != 0)
            {
                // 深度ヒント方向を優先して、セル手前側から空き位置を探す.
                const float seed_len_sq = dot(seed_probe_offset, seed_probe_offset);
                const float3 preferred_dir = (seed_len_sq > 1e-6) ? (seed_probe_offset * rsqrt(seed_len_sq)) : 0.0.xxx;
                if(any(preferred_dir != 0.0.xxx))
                {
                    const float front_bias_samples[4] = {0.95, 0.75, 0.55, 0.35};
                    [unroll]
                    for(int fi = 0; fi < 4; ++fi)
                    {
                        probe_sample_pos_ws = probe_cell_center + preferred_dir * (half_cell_size * front_bias_samples[fi]);
                        if(read_bbv_voxel_from_world_pos(BitmaskBrickVoxel, cb_srvs.bbv.grid_resolution, cb_srvs.bbv.grid_toroidal_offset, cb_srvs.bbv.grid_min_pos, cb_srvs.bbv.cell_size_inv, probe_sample_pos_ws) == 0)
                        {
                            found_non_solid = true;
                            break;
                        }
                    }
                }

                // 方向優先で見つからないときだけランダム探索へフォールバック.
                // シードはセルID固定にして、静止時のフレーム間ゆらぎを抑える。
                if(!found_non_solid)
                {
                    for(int ri = 0; ri < fallback_relocation_count; ++ri)
                    {
                        const uint seed_0 = (global_cell_index * 1664525u + (ri + 1u) * 1013904223u);
                        // update_element_index(=visible list順序) 依存を避け、セルIDベースで安定化.
                        const uint seed_1 = seed_0 ^ (global_cell_index * 2246822519u + 3266489917u);
                        const uint seed_2 = seed_0 ^ ((global_cell_index + 1u) * 668265263u + 374761393u);
                        const float3 random_offset = float3(
                            noise_float_to_float(float2(global_cell_index, seed_0)),
                            noise_float_to_float(float2(seed_1, global_cell_index)),
                            noise_float_to_float(float2(seed_2, seed_0))) - 0.5;
                        probe_sample_pos_ws = probe_cell_center + random_offset * (cascade.grid.cell_size * 0.4);// レンジはセルを超えない程度.

                        if(read_bbv_voxel_from_world_pos(BitmaskBrickVoxel, cb_srvs.bbv.grid_resolution, cb_srvs.bbv.grid_toroidal_offset, cb_srvs.bbv.grid_min_pos, cb_srvs.bbv.cell_size_inv, probe_sample_pos_ws) == 0)
                        {
                            found_non_solid = true;
                            break;
                        }
                    }
                }
            }
        }
        const uint encoded_probe_offset = encode_range1_vec3_to_uint(((probe_sample_pos_ws - probe_cell_center) / (cascade.grid.cell_size * 0.5)));
        // Probe位置更新. Cellサイズの半分で正規化.
        probe_pool_data.probe_offset_v3 = encoded_probe_offset;
    #else
        // 埋まり回避なし
        float3 probe_sample_pos_ws = probe_cell_center;
    #endif

    if(is_new_probe)
    {
        FspProbePoolData src_probe_pool_data = (FspProbePoolData)0;
        if(FspTrySeedProbeFromUpperCascade(src_probe_pool_data, probe_sample_pos_ws, cascade_index, probe_index))
        {
            probe_pool_data.avg_sky_visibility = src_probe_pool_data.avg_sky_visibility;
        }
        else
        {
            // source が見つからない場合だけ新規割り当て時に明示的に初期化する。
            FspClearProbeAtlas(probe_index);
            probe_pool_data.avg_sky_visibility = 0.0;
        }
    }

    // 既存 debug / scratch path 互換のため、セル側 legacy buffer にも最低限ミラーしておく。
    RWFspCellStateBuffer[global_cell_index].probe_offset_v3 = probe_pool_data.probe_offset_v3;
    RWFspCellStateBuffer[global_cell_index].avg_sky_visibility = probe_pool_data.avg_sky_visibility;
    RWFspProbePoolBuffer[probe_index] = probe_pool_data;

    #if 0
        // プローブデータの整理のため一旦ここでの書き込みは無効化. 全域更新のほうで検証中.
        /*
        // Probeレイサンプル.
        {
            for(int sample_index = 0; sample_index < RAY_SAMPLE_COUNT_PER_VOXEL; ++sample_index)
            {
                // 全域Probe更新.
                #if 1
                    // 球面Fibonacciシーケンス分布上をフルでトレースする.
                    // 同時更新されるProbeのレイ方向がほとんど同じになるためか, Probe毎に乱数でサンプルするよりも数倍速くなる模様.
                    const int num_fibonacci_point_max = 128;
                    float3 sample_ray_dir = fibonacci_sphere_point((cb_srvs.frame_count*RAY_SAMPLE_COUNT_PER_VOXEL + sample_index)%num_fibonacci_point_max, num_fibonacci_point_max);
                #else
                    // Probe毎にランダムな方向をサンプリングする.
                    float3 sample_ray_dir = random_unit_vector3( float2( cb_srvs.frame_count + sample_index, update_element_index + sample_index * 37 ) );
                #endif

                const float3 sample_ray_origin = probe_sample_pos_ws;            
                    
                // SkyVisibility raycast.
                const float trace_distance = k_fsp_probe_distance_max;
                int hit_voxel_index = -1;
                    float4 debug_ray_info;
                // リファクタリング版.
#if NGL_SRVS_TRACE_USE_HIBRICK_FSP_VISIBLE_SURFACE_ELEMENT_UPDATE
                float4 curr_ray_t_ws = trace_bbv_hibrick(
                    hit_voxel_index, debug_ray_info,
                    sample_ray_origin, sample_ray_dir, trace_distance, 
                    cb_srvs.bbv.grid_min_pos, cb_srvs.bbv.cell_size, cb_srvs.bbv.grid_resolution,
                    cb_srvs.bbv.grid_toroidal_offset, BitmaskBrickVoxel);
#else
                float4 curr_ray_t_ws = trace_bbv(
                    hit_voxel_index, debug_ray_info,
                    sample_ray_origin, sample_ray_dir, trace_distance, 
                    cb_srvs.bbv.grid_min_pos, cb_srvs.bbv.cell_size, cb_srvs.bbv.grid_resolution,
                    cb_srvs.bbv.grid_toroidal_offset, BitmaskBrickVoxel);
#endif

                // SkyVisibilityの方向平均を更新.
                const float distance_probe_value = (0.0 > curr_ray_t_ws.x) ? 1.0 : 0.0;

                    // ProbeOctMapの更新.
                const float2 octmap_uv = OctEncode(sample_ray_dir);
                const uint2 probe_2d_map_pos = FspProbeAtlasMapPos(probe_index);

                // 境界部込のテクセル位置.
                const uint2 octmap_atlas_texel_pos = probe_2d_map_pos * k_fsp_probe_octmap_width_with_border + 1 + clamp(uint2(octmap_uv * k_fsp_probe_octmap_width), 0, (k_fsp_probe_octmap_width - 1));
                RWFspProbeAtlasTex[octmap_atlas_texel_pos] = lerp(RWFspProbeAtlasTex[octmap_atlas_texel_pos], distance_probe_value, PROBE_UPDATE_TEMPORAL_RATE);
            }
        }
        */
    #endif
}
