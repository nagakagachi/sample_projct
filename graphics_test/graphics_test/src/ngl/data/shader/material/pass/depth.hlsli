
#include "../mtl_header.hlsli"
#include "../mtl_instance_transform_buffer.hlsli"



// -------------------------------------------------------------------------------------------
// VS.
    VS_OUTPUT main_vs(VS_INPUT input)
    {
        const float3x4 instance_mtx = NglGetInstanceTransform(0);
        
        float3 pos_ws = mul(instance_mtx, float4(input.pos, 1.0)).xyz;
        float3 pos_vs = mul(ngl_cb_sceneview.cb_view_mtx, float4(pos_ws, 1.0));
        float4 pos_cs = mul(ngl_cb_sceneview.cb_proj_mtx, float4(pos_vs, 1.0));

        // ジオメトリ側のTangentFrameが正規化されていない場合があるため.
        float3 normal_ws = normalize(mul(instance_mtx, float4(input.normal, 0.0)).xyz);
        float3 tangent_ws = normalize(mul(instance_mtx, float4(input.tangent, 0.0)).xyz);
        float3 binormal_ws = normalize(mul(instance_mtx, float4(input.binormal, 0.0)).xyz);

        MtlVsInput mtl_input = (MtlVsInput)0;
        {
            mtl_input.position_ws = pos_ws;
            mtl_input.normal_ws = normal_ws;
        }
        MtlVsOutput mtl_output = MtlVsEntryPoint(mtl_input);
        {
            pos_ws = pos_ws + mtl_output.position_offset_ws;
            // 再計算.
            pos_vs = mul(ngl_cb_sceneview.cb_view_mtx, float4(pos_ws, 1.0));
            pos_cs = mul(ngl_cb_sceneview.cb_proj_mtx, float4(pos_vs, 1.0));
        }
        
        VS_OUTPUT output = (VS_OUTPUT)0;
        {
            output.pos = pos_cs;
            output.uv = input.uv;

            output.pos_ws = pos_ws;
            output.pos_vs = pos_vs;

            output.normal_ws = normal_ws;
            output.tangent_ws = tangent_ws;
            output.binormal_ws = binormal_ws;
        }
	    
        return output;
    }

// -------------------------------------------------------------------------------------------
// PS.
    void main_ps(VS_OUTPUT input)
    {
        MtlPsInput mtl_input = (MtlPsInput)0;
        {
            mtl_input.pos_sv = input.pos;
            mtl_input.uv0 = input.uv;
            
            mtl_input.pos_ws = input.pos_ws;
            mtl_input.pos_vs = input.pos_vs;
            
            mtl_input.normal_ws = normalize(input.normal_ws);
            mtl_input.tangent_ws = normalize(input.tangent_ws);
            mtl_input.binormal_ws = normalize(input.binormal_ws);
        }

        // マテリアル処理実行.
        MtlPsOutput mtl_output = MtlPsEntryPoint(mtl_input);
        
        // TODO.
        //	アルファテストが必要なら実行.
        clip((0.0 >= mtl_output.opacity)? -1 : 1);
        
    }
