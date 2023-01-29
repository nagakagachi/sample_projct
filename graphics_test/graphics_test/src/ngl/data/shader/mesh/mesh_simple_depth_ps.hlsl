

// SceneView定数バッファ構造定義.
#include "../include/scene_view_struct.hlsli"
ConstantBuffer<SceneViewInfo> cb_sceneview;


struct VS_OUTPUT
{
	float4 pos	:	SV_POSITION;
	float2 uv	:	TEXCOORD0;
};


void main_ps(VS_OUTPUT input)
{
	// TODO.
	//	アルファテストが必要なら実行.
}