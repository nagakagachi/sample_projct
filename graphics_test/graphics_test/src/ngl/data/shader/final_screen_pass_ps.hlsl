
#if 0

	最終画面描画パスPS.
		sRGB OETF も兼ねている.
#endif

#include "include/tonemap.hlsli"

struct VS_OUTPUT
{
	float4 pos	:	SV_POSITION;
	float2 uv	:	TEXCOORD0;
};


Texture2D tex_light;
Texture2D tex_rt;// テクスチャデバッグ.
Texture2D tex_res_data;// テクスチャデバッグ.
SamplerState samp;

float4 main_ps(VS_OUTPUT input) : SV_TARGET
{
	// Lighting Buffer.
	float4 color = tex_light.SampleLevel(samp, input.uv, 0);

	// Tonemap Test.
	{
		if(0.4 > input.uv.y)
		{
			color.rgb = Tonempa_Clamp(color.rgb);
		}
		else
		{
			if(0.5 > input.uv.x)
			{
				color.rgb = Tonemap_AcesFilm_Approx(color.rgb);
			}
			else
			{
				color.rgb = Tonemap_Uncharted2_Filmic(color.rgb);
			}
		}
	}

	if(true)
	{
		// レイトレ描画確認用に一部に貼り付け.
		{
			const float2 debug_area_size = float2(0.2, 0.2);
			const float2 debug_area_lt = float2(0.0, 0.0);
			const float2 debug_area_br = debug_area_lt + debug_area_size;
			if (all(debug_area_lt <= input.uv) && all(debug_area_br >= input.uv))
			{
				float4 sample_color = tex_rt.SampleLevel(samp, (input.uv-debug_area_lt) / debug_area_size, 0);
				color = sample_color;
			}
		}

		{
			const float2 debug_area_size = float2(1.0, 1.0) * 0.3;
			const float2 debug_area_lt = float2(0.7, 0.7);
			const float2 debug_area_br = debug_area_lt + debug_area_size;
			if (all(debug_area_lt <= input.uv) && all(debug_area_br >= input.uv))
			{
				float4 sample_color = tex_res_data.SampleLevel(samp, (input.uv-debug_area_lt) / debug_area_size, 0);
				color = sample_color;
			}
		}
		
		// 50% Gray dot. 半分のピクセルが1, 半分のピクセルが0のドットで50%の輝度表現.
		{
			const float2 debug_area_size = float2(0.1, 0.02);
			const float2 debug_area_lt = float2(0.5, 0.0);
			const float2 debug_area_br = debug_area_lt + debug_area_size;
			if (all(debug_area_lt <= input.uv) && all(debug_area_br >= input.uv))
			{
				const float v = (int(input.pos.x) & 0x01) == (1-(int(input.pos.y) & 0x01))? 1.0 : 0.0;
				color.rgb = v.xxx;
			}
		}
	}
	
	// sRGB向けOETF.
	if(true)
	{
		color.rgb = pow(color.rgb, 1.0/2.2);
	}
	return color;
}
