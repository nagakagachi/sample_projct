
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
	float gb_metalness = gb2.y;
	float gb_surface_option = gb2.z;
	float gb_material_id = gb2.w;
	float3 gb_emissive = gb3.xyz;

	

	// ピクセルへのワールド空間レイを計算.
	float3 to_pixel_ray_ws;
	{
		const float3 to_pixel_ray_vs = CalcViewSpaceRay(input.uv, cb_sceneview.cb_proj_mtx);
		to_pixel_ray_ws = mul(cb_sceneview.cb_view_inv_mtx, float4(to_pixel_ray_vs, 0.0));
	}

	const float k_pi = 3.141592;
	const float3 lit_intensity = float3(1.0, 1.0, 1.0) * k_pi;
	const float3 lit_dir = normalize(float3(-0.5, -1.0, -0.4));

	
	float3 diffuse_term = (1.0/k_pi);
#if 0
	float3 rim_rerm = pow(1.0 - saturate(dot(-to_pixel_ray_ws, gb_normal_ws)), 6);
#else
	float3 rim_rerm = (float3)0;
#endif
	
	float cos_term = saturate(dot(gb_normal_ws, -lit_dir));
	float3 lit_color = gb_albedo * (diffuse_term + rim_rerm) * cos_term * lit_intensity;
	// ambient term.
	if(1)
	{
		const float k_ambient_rate = 0.05;
		lit_color += gb_albedo * diffuse_term * saturate(dot(gb_normal_ws, lit_dir) * 0.5 + 0.5) * lit_intensity * k_ambient_rate;
	}
	
	// 前回フレームのバッファ.
	{
		float3 prev_light = tex_prev_light.Load(int3(input.pos.xy, 0)).xyz;
		
		const float2 dist_from_center = (input.uv - 0.5);
		const float length_from_center = length(dist_from_center);
		// 画面端でテスト用のフィードバックブラー.
		float prev_blend_rate = (0.49 < length_from_center)? 1.0 : 0.0;
		lit_color = lerp(lit_color, prev_light, prev_blend_rate * 0.9);
	}

	// GBufferデバッグ.
	{
		{
			const float2 k_lt = float2(0.5, 0.0);
			const float2 k_size = float2(0.5, 0.5);

			const float2 area_rate = (input.uv - k_lt)/(k_size);
			if(all(0.0 < area_rate) && all(1.0 > area_rate))
				lit_color = gb_albedo;
		}
		{
			const float2 k_lt = float2(0.5, 0.5);
			const float2 k_size = float2(0.5, 0.5);

			const float2 area_rate = (input.uv - k_lt)/(k_size);
			if(all(0.0 < area_rate) && all(1.0 > area_rate))
				lit_color = gb_normal_ws * 0.5 + 0.5;// 可視化は [0,1] 範囲
		}
		{
			const float2 k_lt = float2(0.0, 0.0);
			const float2 k_size = float2(0.2, 0.5);

			const float2 area_rate = (input.uv - k_lt)/(k_size);
			if(all(0.0 < area_rate) && all(1.0 > area_rate))
				lit_color = gb_rounghness;
		}
		{
			const float2 k_lt = float2(0.0, 0.5);
			const float2 k_size = float2(0.2, 0.25);

			const float2 area_rate = (input.uv - k_lt)/(k_size);
			if(all(0.0 < area_rate) && all(1.0 > area_rate))
				lit_color = gb_metalness;
		}
		{
			const float2 k_lt = float2(0.0, 0.75);
			const float2 k_size = float2(0.2, 0.25);

			const float2 area_rate = (input.uv - k_lt)/(k_size);
			if(all(0.0 < area_rate) && all(1.0 > area_rate))
				lit_color = gb_occlusion;
		}
	}
	
	return float4(lit_color, 1.0);
}
