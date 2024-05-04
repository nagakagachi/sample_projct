/*
    material_shader_manager.h

    // Materialシェーダコードは以下のように記述する.
    //  MaterialShaderFileGenerator によってPass毎のMaterialShaderFileが生成される.
    
        // マテリアルパス定義テキストを記述するためのコメントブロック.
        // 記述の制限
        //  <material_config> と </material_config> は行頭に記述され, 余分な改行やスペース等が入っていることは許可されない(Parseの簡易化のため).
        #if 0
        <material_config>
            <pass name="depth"/>
            <pass name="gbuffer"/>
        </material_config>
        #endif

        #include "../mtl_header.hlsli"

        // Material VertexShader関数.
        MtlVsOutput MtlVsEntryPoint(MtlVsInput input)
        {
            MtlVsOutput output = (MtlVsOutput)0;
            // TODO.
            return output;
        }

        // Material PixelShader関数.
        MtlPsOutput MtlPsEntryPoint(MtlPsInput input)
        {
            MtlPsOutput output = (MtlPsOutput)0;
            // TODO.
            return output;
        }


    // Passシェーダ
    //  MaterialShaderCodeを呼び出すPass毎のエントリポイントを含むシェーダ.
    //  エントリポイントは main_[ステージ名] とする.
    //      例r main_vs
    
        VS_OUTPUT main_vs(VS_INPUT input)
        {
            // ...
            // MaterialのVertexShader関数を呼び出す
            MtlVsOutput mtl_output = MtlVsEntryPoint(mtl_input);
            // ...
        }
        void main_ps(VS_OUTPUT input)
        {
            // ...
            // PixelShader関数を呼び出す
            MtlPsOutput mtl_output = MtlPsEntryPoint(mtl_input);
            // ...
        }
    
*/
#pragma once

namespace ngl::rhi
{
    class DeviceDep;
}

namespace ngl
{
namespace gfx
{
    class MaterialShaderManager
    {
    public:
        MaterialShaderManager() = default;
        ~MaterialShaderManager();

        //  generated_shader_root_dir : マテリアルシェーダディレクトリ. ここに マテリアル名/マテリアル毎のPassシェーダ群 が生成される.
        bool Initialize(rhi::DeviceDep* p_device, const char* generated_shader_root_dir);

        int FindMaterialPipeline(const char* material_name, int option = 0);
        
    private:
        rhi::DeviceDep* p_device_ = {};
    };
}
}
