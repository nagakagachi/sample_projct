

cbuffer CbTest
{
	float cb_param0 = 1234;
	uint cb_param1 = 5678;
};


struct VS_INPUT
{
	float4 pos	:	POSITION;
	float2 uv	:	TEXCOORD;
};

struct VS_OUTPUT
{
	float4 pos	:	SV_POSITION;
	float2 uv	:	TEXCOORD0;
};


VS_OUTPUT main_vs(VS_INPUT input)
{
	VS_OUTPUT output = (VS_OUTPUT)0;

	output.pos = input.pos;

	if (cb_param0)
	{
		output.uv = float2(1, 1);
	}

	return output;
}