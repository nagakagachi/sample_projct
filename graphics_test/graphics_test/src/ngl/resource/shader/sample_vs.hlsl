



struct VS_INPUT
{
	float4 pos	:	POSITION;
};

struct VS_OUTPUT
{
	float4 pos	:	SV_POSITION;
};


VS_OUTPUT main_vs(VS_INPUT input)
{
	VS_OUTPUT output = (VS_OUTPUT)0;

	output.pos = input.pos;

	return output;
}