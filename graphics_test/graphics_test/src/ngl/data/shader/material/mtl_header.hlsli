#ifndef NGL_SHADER_MTL_HEADER_H
#define NGL_SHADER_MTL_HEADER_H

// nglのmatrix系ははrow-majorメモリレイアウトであるための指定.
#pragma pack_matrix( row_major )


// SceneView定数バッファ構造定義.
#include "../include/scene_view_struct.hlsli"
ConstantBuffer<SceneViewInfo> ngl_cb_sceneview;


// ------------------------------------------------------
    struct VS_INPUT
    {
        float3 pos		:	POSITION;

        float3 normal	:	NORMAL;
        float3 tangent	:	TANGENT;
        float3 binormal	:	BINORMAL;
	        
        float2 uv		:	TEXCOORD0;
    };

    struct VS_OUTPUT
    {
        float4 pos		:	SV_POSITION;
        float2 uv		:	TEXCOORD0;

        float3 pos_ws	:	POSITION_WS;
        float3 pos_vs	:	POSITION_VS;
	        
        float3 normal_ws	:	NORMAL_WS;
        float3 tangent_ws	:	TANGENT_WS;
        float3 binormal_ws	:	BINORMAL_WS;
    };

// ------------------------------------------------------
    struct MtlVsInput
    {
        float3 position_ws;
        float3 normal_ws;
    };
    struct MtlVsOutput
    {
        float3 position_offset_ws;
    };

// ------------------------------------------------------
    struct MtlPsInput
    {
        float4  pos_sv;
        float2  uv0;

        float3  pos_ws;
        float3  pos_vs;
	        
        float3  normal_ws;
        float3  tangent_ws;
        float3  binormal_ws;
    };

    struct MtlPsOutput
    {
        float3  base_color;
        float   occlusion;
        float3  normal_ws;
        float   roughness;
        float3  emissive;
        float   metalness;
        float   opacity;
};

#endif


