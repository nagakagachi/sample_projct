
struct VS_OUTPUT
{
	float4 pos	:	SV_POSITION;
};


cbuffer CbSamplePs
{
	float4 cb_param0;
};

float4 main_ps(VS_OUTPUT input) : SV_TARGET
{
	return float4(cb_param0.x, cb_param0.y, cb_param0.z, 1.0f);
}