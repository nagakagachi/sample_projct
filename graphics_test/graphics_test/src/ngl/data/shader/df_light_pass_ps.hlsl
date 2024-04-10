
/*

	df_light_pass_ps.hlsl
 
 */


struct VS_OUTPUT
{
	float4 pos	:	SV_POSITION;
	float2 uv	:	TEXCOORD0;
};


#include "include/scene_view_struct.hlsli"
ConstantBuffer<SceneViewInfo> cb_sceneview;


Texture2D tex_lineardepth;
Texture2D tex_gbuffer0;
Texture2D tex_gbuffer1;
Texture2D tex_gbuffer2;
Texture2D tex_gbuffer3;
Texture2D tex_prev_light;
SamplerState samp;

float4 main_ps(VS_OUTPUT input) : SV_TARGET
{
	// リニア深度.
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
	float3 gb_albedo = gb0.xyz;
	float gb_occlusion = gb0.w;
	float3 gb_normal_ws = gb1.xyz * 2.0 - 1.0;// gbufferからWorldNormalデコード.
	float gb_rounghness = gb2.x;
	float gb_metallic = gb2.y;
	float gb_surface_option = gb2.z;
	float gb_material_id = gb2.w;
	float3 gb_emissive = gb3.xyz;

	

	// ピクセルへのワールド空間レイを計算.
	float3 to_pixel_ray_ws;
	{
		const float3 to_pixel_ray_vs = CalcViewSpaceRay(input.uv, cb_sceneview.cb_proj_mtx);
		to_pixel_ray_ws = mul(cb_sceneview.cb_view_inv_mtx, float4(to_pixel_ray_vs, 0.0));
	}

	const float3 lit_intensity = float3(1.0, 1.0, 1.0) * 2.0;
	const float3 lit_dir = normalize(float3(-0.5, -1.0, -0.4));

	
	float3 diffuse_term = (1.0/3.141592);
	float3 rim_rerm = pow(1.0 - saturate(dot(-to_pixel_ray_ws, gb_normal_ws)), 6);
	
	float cos_term = saturate(dot(gb_normal_ws, -lit_dir));
	float3 lit_color = gb_albedo * (diffuse_term + rim_rerm) * cos_term * lit_intensity;

	// ambient term.
	{
		lit_color += gb_albedo * diffuse_term * saturate(-dot(gb_normal_ws, -lit_dir)) * lit_intensity * 0.33;
	}
	
	// 前回フレームのバッファ.
	float3 prev_light = tex_prev_light.Load(int3(input.pos.xy, 0)).xyz;
	// 過去フレームと合成テスト. 右半分.
	if(input.uv.x > 0.7 && input.uv.y > 0.7)
		lit_color = lerp(lit_color, prev_light, 0.95);

	return float4(lit_color, 1.0);
}
