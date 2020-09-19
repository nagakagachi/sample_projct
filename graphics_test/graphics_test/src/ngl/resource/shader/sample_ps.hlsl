

cbuffer CbTest
{
	float cb_param0;
};

float4 main_ps() : SV_TARGET
{
	return float4(cb_param0, 1.0f, 1.0f, 1.0f);
}