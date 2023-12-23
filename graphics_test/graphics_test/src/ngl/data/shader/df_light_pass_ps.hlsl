
/*

	df_light_pass_ps.hlsl
 
 */

struct VS_OUTPUT
{
	float4 pos	:	SV_POSITION;
	float2 uv	:	TEXCOORD0;
};


Texture2D tex_lineardepth;
Texture2D tex_prev_light;
SamplerState samp;

float4 main_ps(VS_OUTPUT input) : SV_TARGET
{
	// リニア深度.
	float ld = tex_lineardepth.SampleLevel(samp, input.uv, 0).r;
	float oc = saturate(ld / 200.0);
	oc = pow(oc, 1.0/0.8);

	// 前回フレームのバッファ.
	float4 prev_light = tex_prev_light.SampleLevel(samp, input.uv, 0);
	
	float4 color = float4(oc, oc, oc, 1.0);

	// 過去フレームと合成テスト. 右半分.
	if(input.uv.x > 0.5)
		color = lerp(color, prev_light, 0.9);

	return color;
}
