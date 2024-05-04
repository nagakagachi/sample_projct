// material_shader_manager.cpp

#include "material_shader_manager.h"

#include <filesystem>

#include "ngl/gfx/resource/resource_shader.h"
#include "ngl/resource/resource_manager.h"
#include "ngl/rhi/d3d12/device.d3d12.h"

namespace ngl
{
namespace gfx
{
    MaterialShaderManager::~MaterialShaderManager()
    {
    }

    //  generated_shader_root_dir : マテリアルシェーダディレクトリ. ここに マテリアル名/マテリアル毎のPassシェーダ群 が生成される.
    bool MaterialShaderManager::Initialize(rhi::DeviceDep* p_device, const char* generated_shader_root_dir)
    {
        enum EMaterialShaderNamePart
        {
            MATERIAL_NAME,
            PASS_NAME,
            SHADER_STAGE_NAME,

            _MAX
        };
        
        assert(p_device);
        p_device_ = p_device;
        
        std::filesystem::path root_dir_path = generated_shader_root_dir;
        if(!exists(root_dir_path) || !is_directory(root_dir_path))
        {
            assert(false);
            return false;
        }

        struct MaterialPassShaderSet
        {
            std::string pass_name = {};
            
            std::string vs_file = {};
            std::string ps_file = {};

            res::ResourceHandle<ResShader> res_vs = {};
            res::ResourceHandle<ResShader> res_ps = {};
        };
        struct MaterialShaderSet
        {
            std::string material_name = {};
            
            std::vector<MaterialPassShaderSet> pass_shader_set = {};
            std::unordered_map<std::string, int> pass_shader_name_index = {};
        };

        std::vector<MaterialShaderSet> material_shader_set = {};
        std::unordered_map<std::string, int> material_shader_name_index = {};

        // 直下のマテリアル別ディレクトリ巡回.
        for(auto mtl_dir_it : std::filesystem::directory_iterator(root_dir_path))
        {
            if(!mtl_dir_it.is_directory())
                continue;

            const std::string mtl_name = mtl_dir_it.path().filename().string();

            for(auto shader_it : std::filesystem::directory_iterator(mtl_dir_it))
            {
                std::filesystem::path shader_file_name_path = shader_it.path().filename();
                if(0 != shader_file_name_path.extension().compare(".hlsl"))
                    continue;

                // shader_filename_splitは . で区切られて各種識別名を含んでいる.
                // 先頭2つは Material名.Pass名
                // 末尾は ShaderStage名
                // 中間には頂点入力タイプやVariation情報を含む予定
                //   例　opaque_standard.gbuffer.ps 
                auto shader_filename = shader_file_name_path.stem().string();
                std::vector<std::string> shader_filename_split = {};
                int split_begin = 0;
                int split_pos = 0;
                for(split_pos = 0; split_pos < shader_filename.size(); ++split_pos)
                {
                    if('.' == shader_filename[split_pos])
                    {
                        shader_filename_split.push_back(shader_filename.substr(split_begin, split_pos - split_begin));
                        split_begin = split_pos + 1;
                    }
                }
                if(split_begin < split_pos)
                {
                    // 最後の要素.
                    shader_filename_split.push_back(shader_filename.substr(split_begin, split_pos - split_begin));
                }

                // ファイルから取得した情報数チェック.
                //  現状では少なくとも MaterialName.
                if(EMaterialShaderNamePart::_MAX > shader_filename_split.size())
                {
                    assert(false);
                    continue;
                }
                
                const auto material_name = shader_filename_split[EMaterialShaderNamePart::MATERIAL_NAME];
                const auto pass_name = shader_filename_split[EMaterialShaderNamePart::PASS_NAME];
                const auto shader_stage_name = shader_filename_split[EMaterialShaderNamePart::SHADER_STAGE_NAME];

                // Material毎のデータベース.
                if(material_shader_name_index.end() == material_shader_name_index.find(material_name))
                {
                    // name -> index.
                    material_shader_name_index[material_name] = static_cast<int>(material_shader_set.size());
                    material_shader_set.push_back({});
                    auto& new_mtl_shader_set = material_shader_set.back();
                    {
                        // 新規要素初期化.
                        new_mtl_shader_set.material_name = material_name;
                    }
                }
                const int mtl_index = material_shader_name_index[material_name];
                auto& mtl_shader_set = material_shader_set[mtl_index];

                // Pass毎のシェーダセット.
                if(mtl_shader_set.pass_shader_name_index.end() == mtl_shader_set.pass_shader_name_index.find(pass_name))
                {
                    // name -> index.
                    mtl_shader_set.pass_shader_name_index[pass_name] = static_cast<int>(mtl_shader_set.pass_shader_set.size());

                    mtl_shader_set.pass_shader_set.push_back({});
                    auto& new_pass_shader_set = mtl_shader_set.pass_shader_set.back();
                    {
                        // 新規要素初期化.
                        new_pass_shader_set.pass_name = pass_name;
                    }
                }
                const int pass_shader_set_index = mtl_shader_set.pass_shader_name_index[pass_name];
                auto& pass_shader_set = mtl_shader_set.pass_shader_set[pass_shader_set_index];

                // Passを構成するStage毎のShader設定.
                {
                    if(shader_stage_name == "vs")
                    {
                        pass_shader_set.vs_file = shader_it.path().generic_string();// generic_string で / 区切りパスとする.
                    }
                    else if(shader_stage_name == "ps")
                    {
                        pass_shader_set.ps_file = shader_it.path().generic_string();// generic_string で / 区切りパスとする.
                    }
                    else
                    {
                        assert(false);
                    }
                }
            }
        }
        
        // Shaderロード.
        static constexpr char k_shader_model[] = "6_3";
		auto& ResourceMan = ngl::res::ResourceManager::Instance();
        for(size_t mtl_i = 0; mtl_i < material_shader_set.size(); ++mtl_i)
        {
            auto& mtl_set = material_shader_set[mtl_i];

            for(size_t pass_i = 0; pass_i < mtl_set.pass_shader_set.size(); ++pass_i)
            {
                auto& pass_set = mtl_set.pass_shader_set[pass_i];

                if(0 < pass_set.vs_file.size())
                {
                    ngl::gfx::ResShader::LoadDesc loaddesc_vs = {};
                    loaddesc_vs.entry_point_name = "main_vs";
                    loaddesc_vs.stage = ngl::rhi::EShaderStage::Vertex;
                    loaddesc_vs.shader_model_version = k_shader_model;

                    pass_set.res_vs = ResourceMan.LoadResource<ngl::gfx::ResShader>(p_device, pass_set.vs_file.c_str(), &loaddesc_vs);
                }
                if(0 < pass_set.vs_file.size())
                {
                    ngl::gfx::ResShader::LoadDesc loaddesc_vs = {};
                    loaddesc_vs.entry_point_name = "main_ps";
                    loaddesc_vs.stage = ngl::rhi::EShaderStage::Pixel;
                    loaddesc_vs.shader_model_version = k_shader_model;

                    pass_set.res_ps = ResourceMan.LoadResource<ngl::gfx::ResShader>(p_device, pass_set.ps_file.c_str(), &loaddesc_vs);
                }
            }
        }

        // PipelineStateObject生成. TODO.
        for(size_t mtl_i = 0; mtl_i < material_shader_set.size(); ++mtl_i)
        {
            auto& mtl_set = material_shader_set[mtl_i];

            for(size_t pass_i = 0; pass_i < mtl_set.pass_shader_set.size(); ++pass_i)
            {
                auto& pass_set = mtl_set.pass_shader_set[pass_i];

                // VS-PS Pso.
                if(pass_set.res_vs.IsValid() && pass_set.res_ps.IsValid())
                {
                    // TODO.
                    // pass_nameに対応したMaterialPass毎のPSOの生成をする.
                    //  depth ならRenderTarget無しであったり, gbuffer なら RenderTargetがGBuffer枚数分あるなど.
                    
                }
            }
        }
        
        return true;
    }

    // マテリアル名と追加情報からPipeline取得.
    int MaterialShaderManager::FindMaterialPipeline(const char* material_name, int option)
    {
        return -1;
    }
    
}
}
