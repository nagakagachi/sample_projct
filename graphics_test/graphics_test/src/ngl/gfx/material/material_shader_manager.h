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
#include <ostream>
#include <string>
#include <unordered_map>
#include <iostream>

#include "ngl/gfx/common_struct.h"
#include "ngl/rhi/rhi.h"
#include "ngl/util/singleton.h"

namespace ngl::gfx
{
    class ResShader;
}

namespace ngl::rhi
{
    class GraphicsPipelineStateDep;
    class DeviceDep;
}

namespace ngl
{
namespace gfx
{
    struct MaterialPassPsoDesc
    {
        const ResShader* p_vs = {};
        const ResShader* p_ps = {};

        MeshVertexSemanticSlotMask  vs_input_layout_mask = {};
    };
    // Pass Pso Creator Interface.
    class IMaterialPassPsoCreator
    {
    public:
        virtual ~IMaterialPassPsoCreator() = default;
        virtual rhi::GraphicsPipelineStateDep* Create(rhi::DeviceDep* p_device, const MaterialPassPsoDesc& pass_pso_desc);
    };

    
    // Depth Pass Pso Creator.
    class MaterialPassPsoCreator_depth : public IMaterialPassPsoCreator
    {
    public:
        static constexpr char k_name[] = "depth";
        rhi::GraphicsPipelineStateDep* Create(rhi::DeviceDep* p_device, const MaterialPassPsoDesc& pass_pso_desc) override;
        
        // Depth.
        static constexpr auto k_depth_format = rhi::EResourceFormat::Format_D32_FLOAT;
        
    private:
    };

    // GBUffer Pass Pso Creator.
    class MaterialPassPsoCreator_gbuffer : public IMaterialPassPsoCreator
    {
    public:
        static constexpr char k_name[] = "gbuffer";
        rhi::GraphicsPipelineStateDep* Create(rhi::DeviceDep* p_device, const MaterialPassPsoDesc& pass_pso_desc) override;
        
        // Depth.
        static constexpr auto k_depth_format = rhi::EResourceFormat::Format_D32_FLOAT;
        // GBuffer0 BaseColor, Occlusion
        static constexpr auto k_gbuffer0_format = rhi::EResourceFormat::Format_R8G8B8A8_UNORM_SRGB;
        // GBuffer1 WorldNormal.xyz, 1bitOption.w
        static constexpr auto k_gbuffer1_format = rhi::EResourceFormat::Format_R10G10B10A2_UNORM;
        // GBuffer2 Roughness, Metallic, Optional, MaterialId
        static constexpr auto k_gbuffer2_format = rhi::EResourceFormat::Format_R8G8B8A8_UNORM;
        // GBuffer3 Emissive.xyz, Unused.w
        static constexpr auto k_gbuffer3_format = rhi::EResourceFormat::Format_R16G16B16A16_FLOAT;
        // Velocity xy
        static constexpr auto k_velocity_format = rhi::EResourceFormat::Format_R16G16_FLOAT;
    private:
    };


    
    
    // ランタイムでMaterialShaderPSOの問い合わせに対応するクラス.
    class MaterialShaderManager : public Singleton<MaterialShaderManager>
    {
    public:
        MaterialShaderManager();
        ~MaterialShaderManager();

        // PassPso生成クラスを登録する.
        //  Setup()よりも先に登録が必要.
        template<typename PASS_PSO_CREATOR_TYPE>
        void RegisterPassPsoCreator();
        
        //  generated_shader_root_dir : マテリアルシェーダディレクトリ. ここに マテリアル名/マテリアル毎のPassシェーダ群 が生成される.
        bool Setup(rhi::DeviceDep* p_device, const char* generated_shader_root_dir);
        void Finalize();

        // マテリアル名と追加情報からPipeline生成またはCacheから取得.
        rhi::GraphicsPipelineStateDep* CreateMaterialPipeline(const char* material_name, const char* pass_name, MeshVertexSemanticSlotMask vsin_slot);
    private:
        void RegisterPassPsoCreator(const char* name, IMaterialPassPsoCreator* p_instance);
        // Pass Pso Creator登録用.
        std::unordered_map<std::string, IMaterialPassPsoCreator*> pso_creator_map_;
    private:
        rhi::DeviceDep* p_device_ = {};
        // 内部データ隠蔽.
        class MaterialShaderManagerImpl* p_impl_ = {};
    };
    template<typename PASS_PSO_CREATOR_TYPE>
    inline void MaterialShaderManager::RegisterPassPsoCreator()
    {
        std::cout << "[MaterialShaderManager] RegisterPassPsoCreator " << PASS_PSO_CREATOR_TYPE::k_name << std::endl;

        // Pass Pso CreatorはStateを持たないため, 外部からは型だけ指定して内部でstaticインスタンスを登録する.
        static PASS_PSO_CREATOR_TYPE register_instance = {}; 
        RegisterPassPsoCreator(PASS_PSO_CREATOR_TYPE::k_name, &register_instance);
    }
}
}
