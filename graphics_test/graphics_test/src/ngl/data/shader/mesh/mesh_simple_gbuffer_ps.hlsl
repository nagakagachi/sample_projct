

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


	const float3 base_color = float3(input.uv * 2.0, abs(normalize(input.normal_ws).y));// 適当なベースカラー.
	const float occlusion = 1.0;
	const float roughness = 1.0;
	const float metallic = 0.0;
	const float surface_optional = 0.0;
	const float material_id = 0.0;
	const float3 normal_ws = normalize(input.normal_ws);
	const float3 emissive = float3(0.0, 0.0, 0.0);

	// GBuffer Encode.
	{
		output.gbuffer0.xyz = base_color;
		output.gbuffer0.w = occlusion;

		// [-1,+1]のNormal を unorm[0,1]で格納.
		output.gbuffer1.xyz = normal_ws * 0.5 + 0.5;
		output.gbuffer1.w = 0.0;

		output.gbuffer2 = float4(roughness, metallic, surface_optional, material_id);

		output.gbuffer3 = float4(emissive, 0.0);
	
		output.velocity = float2(0.0, 0.0);// velocityは保留.
	}
	
	return output;
}