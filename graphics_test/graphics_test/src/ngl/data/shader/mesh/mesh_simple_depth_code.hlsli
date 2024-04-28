
// SceneView定数バッファ構造定義.
#include "../include/scene_view_struct.hlsli"
ConstantBuffer<SceneViewInfo> cb_sceneview;

struct InstanceInfo
{
    float3x4 mtx;
};
ConstantBuffer<InstanceInfo> cb_instance;


// -------------------------------------------------------------------------------------------
// VS.
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
    };

    VS_OUTPUT main_vs(VS_INPUT input)
    {
        VS_OUTPUT output = (VS_OUTPUT)0;

        float3 pos_ws = mul(cb_instance.mtx, float4(input.pos, 1.0));
        float3 pos_vs = mul(cb_sceneview.cb_view_mtx, float4(pos_ws, 1.0));
        float4 pos_cs = mul(cb_sceneview.cb_proj_mtx, float4(pos_vs, 1.0));

        output.pos = pos_cs;
        output.uv = input.uv;

        return output;
    }

// -------------------------------------------------------------------------------------------
// PS.
    void main_ps(VS_OUTPUT input)
    {
        // TODO.
        //	アルファテストが必要なら実行.
    }
