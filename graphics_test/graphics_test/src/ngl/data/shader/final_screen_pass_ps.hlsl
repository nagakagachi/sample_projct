
struct VS_OUTPUT
{
	float4 pos	:	SV_POSITION;
	float2 uv	:	TEXCOORD0;
};


Texture2D tex_lineardepth;
Texture2D tex_rt;
SamplerState samp;

float4 main_ps(VS_OUTPUT input) : SV_TARGET
{
	// リニア深度.
	float ld = tex_lineardepth.SampleLevel(samp, input.uv, 0).r;
	float oc = saturate(ld / 200.0);
	oc = pow(oc, 1.0/0.8);

	float4 color = float4(oc, oc, oc, 1.0);


	// レイトレ描画確認用に一部に貼り付け.
	const float2 rt_debug_area = float2(0.3, 0.3);
	if (rt_debug_area.x >= input.uv.x && rt_debug_area.y >= input.uv.y)
	{
		float4 rt_color = tex_rt.SampleLevel(samp, input.uv / rt_debug_area, 0);

		color = rt_color;
	}

	return color;
}
