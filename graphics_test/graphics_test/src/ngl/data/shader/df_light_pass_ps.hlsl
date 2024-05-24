
/*

	df_light_pass_ps.hlsl
 
 */


struct VS_OUTPUT
{
	float4 pos	:	SV_POSITION;
	float2 uv	:	TEXCOORD0;
};


#include "include/scene_view_struct.hlsli"
ConstantBuffer<SceneViewInfo> ngl_cb_sceneview;
ConstantBuffer<SceneDirectionalShadowInfo> ngl_cb_shadowview;


Texture2D tex_lineardepth;// Linear View Depth.
Texture2D tex_gbuffer0;
Texture2D tex_gbuffer1;
Texture2D tex_gbuffer2;
Texture2D tex_gbuffer3;
Texture2D tex_prev_light;
Texture2D tex_shadowmap;
SamplerState samp;



#define ngl_PI 3.14159265358979323846
#define ngl_EPSILON 1e-6

// GGXの知覚的Roughnessの下限制限をしてハイライトを残すための値.
// https://google.github.io/filament/Filament.html
#define ngl_MIN_PERCEPTUAL_ROUGHNESS 0.045

// reflectance : 誘電体の場合の正規化反射率[0,1]
float3 compute_F0(const float3 base_color, float metallic, float dielectric_reflectance) {
	return (base_color * metallic) + (0.16 * dielectric_reflectance * dielectric_reflectance * (1.0 - metallic));
}
// 誘電体の正規化反射率(reflectance)に一般的な値な値を採用して計算.
float3 compute_F0_default(const float3 base_color, float metallic) {
	const float k_dielectric_reflectance = 0.5;// compute_F0()の metallic==0 で一般的な誘電体のスペキュラF0である0.04になるような値.
	return compute_F0(base_color, metallic, k_dielectric_reflectance);
}


float3 brdf_schlick_F(float3 F0, float3 N, float3 V, float3 L)
{
	const float3 h = normalize(V + L);
	const float v_o_h = saturate(dot(V, h));
	const float tmp = (1.0 - v_o_h);
	const float3 F = F0 + (1.0 - F0) * (tmp*tmp*tmp*tmp*tmp);
	return F;
}
float brdf_trowbridge_reitz_D(float perceptual_roughness, float3 N, float3 V, float3 L)
{
	const float limited_perceptual_roughness = clamp(perceptual_roughness, ngl_MIN_PERCEPTUAL_ROUGHNESS, 1.0);
	const float a = limited_perceptual_roughness*limited_perceptual_roughness;
	const float a2 = a*a;
	
	const float3 h = normalize(V + L);
	const float n_o_h = dot(N, h);
	const float tmp = (1.0 + n_o_h*n_o_h * (a2 - 1.0));
	const float D = (a2) / (ngl_PI * tmp*tmp);
	return D;
}
// Height-Correlated Smith Masking-Shadowing
// マイクロファセットBRDFの分母 4*NoV*NoL の項を含み単純化されたもの.
float brdf_smith_ggx_correlated_G(float perceptual_roughness, float3 N, float3 V, float3 L)
{
	const float limited_perceptual_roughness = clamp(perceptual_roughness, ngl_MIN_PERCEPTUAL_ROUGHNESS, 1.0);
	const float a = limited_perceptual_roughness*limited_perceptual_roughness;
	const float a2 = a*a;
	
	const float ui = saturate(dot(N,L));
	const float uo = saturate(dot(N,V));
	
	const float d0 = uo * sqrt(a2 + ui * (ui - a2 * ui));
	const float d1 = ui * sqrt(a2 + uo * (uo - a2 * uo));
	const float G = 0.5 / (d0 + d1 + ngl_EPSILON);// zero divide対策.
	return G;
}
float3 brdf_standard_ggx(float3 base_color, float perceptual_roughness, float metalness, float3 N, float3 V, float3 L)
{
	const float3 F0 =  compute_F0_default(base_color, metalness);
	
	const float3 brdf_F = brdf_schlick_F(F0, N, V, L);
	const float brdf_D = brdf_trowbridge_reitz_D(perceptual_roughness, N, V, L);
	// マイクロファセットBRDFの分母の 4*NoV*NoL 項は式の単純化のためG項に含まれる.[Lagarde].
	const float brdf_G = brdf_smith_ggx_correlated_G(perceptual_roughness, N, V, L);

	return brdf_D * brdf_F * brdf_G;
}

float3 brdf_lambert(float3 base_color, float perceptual_roughness, float metalness, float3 N, float3 V, float3 L)
{
	const float lambert = 1.0 / ngl_PI;

	const float3 diffuse = lerp(base_color, 0.0, metalness) * lambert;
	return diffuse;
}

float4 main_ps(VS_OUTPUT input) : SV_TARGET
{
	// リニアView深度.
	float ld = tex_lineardepth.Load(int3(input.pos.xy, 0)).r;// LightingBufferとGBufferが同じ解像度前提でLoad.
	if(1e7 <= ld)
	{
		// 天球扱い.
		return float4(0.0, 0.0, 0.5, 0.0);
	}
	float depth_visualize = pow(saturate(ld / 200.0), 1.0/0.8);
	
	// LightingBufferとGBufferが同じ解像度前提でLoad.
	float4 gb0 = tex_gbuffer0.Load(int3(input.pos.xy, 0));
	float4 gb1 = tex_gbuffer1.Load(int3(input.pos.xy, 0));
	float4 gb2 = tex_gbuffer2.Load(int3(input.pos.xy, 0));
	float4 gb3 = tex_gbuffer3.Load(int3(input.pos.xy, 0));

	// GBuffer Decode.
	float3 gb_base_color = gb0.xyz;
	float gb_occlusion = gb0.w;
	float3 gb_normal_ws = gb1.xyz * 2.0 - 1.0;// gbufferからWorldNormalデコード.
	float gb_rounghness = gb2.x;
	float gb_metalness = gb2.y;
	float gb_surface_option = gb2.z;
	float gb_material_id = gb2.w;
	float3 gb_emissive = gb3.xyz;

	

	// ピクセルへのワールド空間レイを計算.
	const float3 to_pixel_ray_vs = CalcViewSpaceRay(input.uv, ngl_cb_sceneview.cb_proj_mtx);

#if 1
	// 
	const float3 pixel_pos_ws = mul(ngl_cb_sceneview.cb_view_inv_mtx, float4((to_pixel_ray_vs/abs(to_pixel_ray_vs.z)) * ld, 1.0));
	const float3 to_pixel_ray_ws = normalize(pixel_pos_ws - ngl_cb_sceneview.cb_view_inv_mtx._m03_m13_m23);
#else
	float3 to_pixel_ray_ws = mul(ngl_cb_sceneview.cb_view_inv_mtx, float4(to_pixel_ray_vs/abs(to_pixel_ray_vs.z), 0.0));// View空間Rayのzを1に補正して後段でViewZ乗算する.
	const float3 pixel_pos_ws = to_pixel_ray_ws * ld + ngl_cb_sceneview.cb_view_inv_mtx._m03_m13_m23;
#endif
	
	const float k_pi = 3.141592;
	const float3 lit_intensity = float3(1.0, 1.0, 1.0) * k_pi;
	//const float3 lit_dir = normalize(float3(-0.5, -1.0, -0.4));
	const float3 lit_dir = normalize(ngl_cb_shadowview.cb_shadow_view_inv_mtx._m02_m12_m22);// ShadowViewの向きを利用.

	
	const float shadow_sample_bias_ws = 0.001;// 基準バイアス.
	const float shadow_sample_slope_bias_ws = 5.0 / (saturate(dot(-lit_dir, gb_normal_ws)) + 0.001);// スロープバイアス.
	const float3 pixel_pos_shadow_vs = mul(ngl_cb_shadowview.cb_shadow_view_mtx, float4(pixel_pos_ws + (-lit_dir * ((1.0 + shadow_sample_slope_bias_ws)*shadow_sample_bias_ws)), 1.0));
	const float4 pixel_pos_shadow_cs = mul(ngl_cb_shadowview.cb_shadow_proj_mtx, float4(pixel_pos_shadow_vs, 1.0));
	const float3 pixel_pos_shadow_cs_pd = pixel_pos_shadow_cs.xyz / pixel_pos_shadow_cs.w;
	float2 pixel_pos_shadow_uv = pixel_pos_shadow_cs_pd.xy * 0.5 + 0.5;
	pixel_pos_shadow_uv.y = 1.0 - pixel_pos_shadow_uv.y;// Y反転.

	// Shadowmap Sample.
	float light_visibility = 1.0;
	if(true)
	{
		// 範囲外はとりあえず日向.
		if(all(0.0 < pixel_pos_shadow_uv) && all(1.0 > pixel_pos_shadow_uv))
		{
			const float shadow_sample = tex_shadowmap.SampleLevel(samp, pixel_pos_shadow_uv, 0).x;
			light_visibility = (shadow_sample <= pixel_pos_shadow_cs_pd.z)? 1.0 : 0.0;
		}
	}

	
	const float3 V = -to_pixel_ray_ws;
	const float3 L = -lit_dir;
	
	const float3 specular_term = brdf_standard_ggx(gb_base_color, gb_rounghness, gb_metalness, gb_normal_ws, V, L);
	const float3 diffuse_term = brdf_lambert(gb_base_color, gb_rounghness, gb_metalness, gb_normal_ws, V, L);
	
	const float3 brdf = specular_term + (diffuse_term);
	
	const float cos_term = saturate(dot(gb_normal_ws, -lit_dir));
	float3 lit_color = cos_term * brdf * lit_intensity * light_visibility;
	
	// ambient term.
	if(1)
	{
		const float3 k_ambient_rate = float3(0.7, 0.7, 1.0) * 0.15;
		lit_color += gb_base_color * (1.0/k_pi) * (abs(dot(gb_normal_ws, lit_dir)) * 0.5 + 0.5) * lit_intensity * k_ambient_rate;
	}

	
	// GBufferデバッグ.
	if(true)
	{
		// 前回フレームのバッファ.
		if(true)
		{
			float3 prev_light = tex_prev_light.Load(int3(input.pos.xy, 0)).xyz;
		
			const float2 dist_from_center = (input.uv - 0.5);
			const float length_from_center = length(dist_from_center);
			// 画面端でテスト用のフィードバックブラー.
			float prev_blend_rate = (0.49 < length_from_center)? 1.0 : 0.0;
			lit_color = lerp(lit_color, prev_light, prev_blend_rate * 0.9);
		}

		const float debug_area_w = 0.1;
		if(true)
		{
			const float2 k_lt = float2(1.0 - debug_area_w, 0.0);
			const float2 k_size = float2(debug_area_w, 0.5);

			const float2 area_rate = (input.uv - k_lt)/(k_size);
			if(all(0.0 < area_rate) && all(1.0 > area_rate))
				lit_color = gb_base_color;
		}
		if(true)
		{
			const float2 k_lt = float2(1.0 - debug_area_w, 0.5);
			const float2 k_size = float2(debug_area_w, 0.5);

			const float2 area_rate = (input.uv - k_lt)/(k_size);
			if(all(0.0 < area_rate) && all(1.0 > area_rate))
				lit_color = gb_normal_ws * 0.5 + 0.5;// 可視化は [0,1] 範囲
		}
		if(true)
		{
			const float2 k_lt = float2(0.0, 0.0);
			const float2 k_size = float2(debug_area_w, 0.4);

			const float2 area_rate = (input.uv - k_lt)/(k_size);
			if(all(0.0 < area_rate) && all(1.0 > area_rate))
				lit_color = gb_rounghness;
		}
		if(true)
		{
			const float2 k_lt = float2(0.0, 0.4);
			const float2 k_size = float2(debug_area_w, 0.8);

			const float2 area_rate = (input.uv - k_lt)/(k_size);
			if(all(0.0 < area_rate) && all(1.0 > area_rate))
				lit_color = gb_metalness;
		}
		if(true)
		{
			const float2 k_lt = float2(0.0, 0.8);
			const float2 k_size = float2(debug_area_w, 0.2);

			const float2 area_rate = (input.uv - k_lt)/(k_size);
			if(all(0.0 < area_rate) && all(1.0 > area_rate))
				lit_color = gb_occlusion;
		}
	}
	
	
	return float4(lit_color, 1.0);
}
