
struct VS_OUTPUT
{
	float4 pos	:	SV_POSITION;
	float2 uv	:	TEXCOORD0;
};


Texture2D tex_lineardepth;
SamplerState samp;

float4 main_ps(VS_OUTPUT input) : SV_TARGET
{
	float ld = tex_lineardepth.SampleLevel(samp, input.uv, 0).r;

	float oc = saturate(ld / 200.0);
	
	oc = pow(oc, 1.0/0.8);

	return oc;
}
