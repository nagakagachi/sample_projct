
// SceneView定数バッファ構造定義.
#include "../include/scene_view_struct.hlsli"
ConstantBuffer<SceneViewInfo> cb_sceneview;



struct VS_INPUT
{
	float3 pos		:	POSITION;
	float3 normal	:	NORMAL;
	float2 uv		:	TEXCOORD0;
};

struct VS_OUTPUT
{
	float4 pos		:	SV_POSITION;
	float2 uv		:	TEXCOORD0;

	float3 pos_ws	:	POSITION_WS;
	float3 pos_vs	:	POSITION_VS;
};


struct InstanceInfo
{
	float3x4 mtx;
};
ConstantBuffer<InstanceInfo> cb_instance;


VS_OUTPUT main_vs(VS_INPUT input)
{
	VS_OUTPUT output = (VS_OUTPUT)0;

	float3 pos_ws = mul(cb_instance.mtx, float4(input.pos, 1.0));
	float3 pos_vs = mul(cb_sceneview.cb_view_mtx, float4(pos_ws, 1.0));
	float4 pos_cs = mul(cb_sceneview.cb_proj_mtx, float4(pos_vs, 1.0));

	output.pos = pos_cs;
	output.uv = input.uv;

	output.pos_ws = pos_ws;
	output.pos_vs = pos_vs;

	return output;
}