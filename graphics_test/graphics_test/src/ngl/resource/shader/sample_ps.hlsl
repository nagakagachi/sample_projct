

struct VS_OUTPUT
{
	float4 pos	:	SV_POSITION;
	float2 uv	:	TEXCOORD0;
};


cbuffer CbSamplePs
{
	float4 cb_param0;
};


Texture2D		TexPs;
SamplerState	SmpPs;

float4 main_ps(VS_OUTPUT input) : SV_TARGET
{
	float4 tex0 = TexPs.SampleLevel(SmpPs, input.uv, 0);

	//return float4(input.uv.x, tex0.x, cb_param0.x, 1.0);
	//return float4(input.uv.x, input.uv.y, 0.0, 1.0);
	return tex0;
}