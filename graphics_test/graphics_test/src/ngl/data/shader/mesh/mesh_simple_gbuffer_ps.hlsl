

// SceneView定数バッファ構造定義.
#include "../include/scene_view_struct.hlsli"
ConstantBuffer<SceneViewInfo> cb_sceneview;


struct VS_OUTPUT
{
	float4 pos	:	SV_POSITION;
	float2 uv	:	TEXCOORD0;

	float3 pos_ws	:	POSITION_WS;
	float3 pos_vs	:	POSITION_VS;
	
	float3 normal_ws	:	NORMAL_WS;
};

struct GBufferOutput
{
	float4 gbuffer0 : SV_TARGET0;
	float4 gbuffer1 : SV_TARGET1;
	float4 gbuffer2 : SV_TARGET2;
	float4 gbuffer3 : SV_TARGET3;
	float2 velocity : SV_TARGET4;
};

GBufferOutput main_ps(VS_OUTPUT input)
{
	GBufferOutput output = (GBufferOutput)0;

	output.gbuffer0.xyz = float3(input.uv, 0.0);	// BaseColorには適当にUV描き込み.
	
	output.gbuffer1.xyz = normalize(input.normal_ws) * 0.5 + 0.5;// [-1,+1] を unorm に [0,1]で格納.

	output.velocity = input.uv;
	
	return output;
}