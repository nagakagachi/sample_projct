
// SceneView定数バッファ構造定義.
#include "../include/scene_view_struct.hlsli"
ConstantBuffer<SceneViewInfo> ngl_cb_sceneview;

#include "mesh_transform_buffer.hlsli"

// -------------------------------------------------------------------------------------------
// VS.
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

    VS_OUTPUT main_vs(VS_INPUT input)
    {
        VS_OUTPUT output = (VS_OUTPUT)0;

        const float3x4 instance_mtx = NglGetInstanceTransform(0);
        
        float3 pos_ws = mul(instance_mtx, float4(input.pos, 1.0)).xyz;
        float3 pos_vs = mul(ngl_cb_sceneview.cb_view_mtx, float4(pos_ws, 1.0));
        float4 pos_cs = mul(ngl_cb_sceneview.cb_proj_mtx, float4(pos_vs, 1.0));

        float3 normal_ws = mul(instance_mtx, float4(input.normal, 0.0)).xyz;
        float3 tangent_ws = mul(instance_mtx, float4(input.tangent, 0.0)).xyz;
        float3 binormal_ws = mul(instance_mtx, float4(input.binormal, 0.0)).xyz;
	    
        output.pos = pos_cs;
        output.uv = input.uv;

        output.pos_ws = pos_ws;
        output.pos_vs = pos_vs;

        output.normal_ws = normal_ws;
        output.tangent_ws = tangent_ws;
        output.binormal_ws = binormal_ws;
	    
        return output;
    }


// -------------------------------------------------------------------------------------------
// PS.
    struct GBufferOutput
    {
        float4 gbuffer0 : SV_TARGET0;
        float4 gbuffer1 : SV_TARGET1;
        float4 gbuffer2 : SV_TARGET2;
        float4 gbuffer3 : SV_TARGET3;
        float2 velocity : SV_TARGET4;
    };

    // sampler.
    SamplerState samp_default;

    Texture2D tex_basecolor;
    Texture2D tex_normal;
    Texture2D tex_occlusion;
    Texture2D tex_roughness;
    Texture2D tex_metalness;

    GBufferOutput main_ps(VS_OUTPUT input)
    {
        GBufferOutput output = (GBufferOutput)0;

        const float3 mtl_base_color = tex_basecolor.Sample(samp_default, input.uv).rgb;
        const float3 mtl_normal = tex_normal.Sample(samp_default, input.uv).rgb * 2.0 - 1.0;
	    
        const float mtl_occlusion = tex_occlusion.Sample(samp_default, input.uv).r;	// glTFでは別テクスチャでもチャンネルはORMそれぞれRGBになっている?.
        const float mtl_roughness = tex_roughness.Sample(samp_default, input.uv).g;	// .
        const float mtl_metalness = tex_metalness.Sample(samp_default, input.uv).b;	// .
	    
        const float occlusion = mtl_occlusion;
        const float roughness = mtl_roughness;
        const float metallic = mtl_metalness;
        const float surface_optional = 0.0;
        const float material_id = 0.0;
        const float3 normal_ws = normalize(input.tangent_ws * mtl_normal.x + input.binormal_ws * mtl_normal.y + input.normal_ws * mtl_normal.z);
        const float3 emissive = float3(0.0, 0.0, 0.0);

        // GBuffer Encode.
        {
            output.gbuffer0.xyz = mtl_base_color;
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
