// material_shader_generator.cpp

#include "material_shader_generator.h"
#include "ngl/file/file.h"

#include <array>
#include <filesystem>
#include <fstream>

// xml操作.
#include <iostream>
#include <tinyxml2.h>
#include <unordered_map>


namespace ngl
{
namespace gfx
{
    MaterialShaderGenerator::~MaterialShaderGenerator()
    {   
    }

    /*
     // マテリアル実装hlsliにコメント等でXMLによるマテリアル定義が記述されているものとしてそれをParseする.
     // Parseの簡易化のために開始終了の <material_config>, </material_config> は行頭で不要なスペースや改行が無いものとする.
     
        <material_config>
            <pass name="depth"/>
            <pass name="gbuffer"/>
        </material_config>
    */
    bool MaterialShaderGenerator::GenerateMaterialShaderFiles(const char* material_impl_dir, const char* material_pass_dir, const char* generated_root_dir)
    {   
        constexpr char k_material_config_xml_begin_tag[] = "<material_config>";
        constexpr char k_material_config_xml_end_tag[] = "</material_config>";
        constexpr char k_material_config_pass_tag[] = "pass";

        // 現状は固定で VS-PS.
        const std::array<std::string,2> k_stage_names =
        {
            "vs",
            "ps"
        };

        
        struct PassShaderInfo
        {
            // ./src/ngl/data/shader/material/pass\depth.hlsli, etc.
            std::string path = {};
            // depth, gbuffer, etc.
            std::string name = {};
        };
        struct MaterialConfig
        {
            // ./src/ngl/data/shader/material/impl\opaque_standard.hlsli
            std::string impl_file_path = {};
            // opaque_standard
            std::string name = {};
            // depth, gbuffer, etc.
            std::vector<std::string> pass = {};
        };

        // パスチェック.
        std::filesystem::path impl_dir_path(material_impl_dir);
        std::filesystem::path pass_dir_path(material_pass_dir);
        std::filesystem::path generated_dir_path(generated_root_dir);
        if(!std::filesystem::is_directory(impl_dir_path))
        {
            assert(false);
            return false;
        }
        if(!std::filesystem::is_directory(pass_dir_path))
        {
            assert(false);
            return false;
        }
        if(!std::filesystem::is_directory(generated_dir_path))
        {
            assert(false);
            return false;
        }

        
        // 生成用収集データ.
        std::vector<PassShaderInfo> pass_shader_info_list = {};
        std::unordered_map<std::string, int> pass_shader_info_map = {};// pass name -> pass_shader_info_list index.
        std::vector<MaterialConfig> material_config_list = {};
        
        // Passシェーダ情報収集.
        for(auto it : std::filesystem::directory_iterator(pass_dir_path))
        {
            if(it.is_directory())
                continue;

            if(0 == it.path().extension().compare(".hlsli"))
            {
                const auto pass_name = it.path().stem().string();
                
                pass_shader_info_map[pass_name] = static_cast<int>(pass_shader_info_list.size());// name->index.
                pass_shader_info_list.push_back({});

                auto& pass_shader_info = pass_shader_info_list.back();
                {
                    pass_shader_info.path = it.path().string();
                    pass_shader_info.name = pass_name;
                }
            }
        }

        // Materialシェーダ情報収集.
        for(auto it : std::filesystem::directory_iterator(impl_dir_path))
        {
            if(it.is_directory() || !it.path().has_stem())
                continue;

            material_config_list.push_back({});
            auto& mtl_config = material_config_list.back();
            {
                mtl_config.impl_file_path = it.path().string();
                mtl_config.name = it.path().filename().stem().string();
                mtl_config.pass = {};
            }
        }
        for(size_t i = 0; i < material_config_list.size(); ++i)
        {
            // impl.hlsliを1行ずつReadして定義XML部を取り出す.
            std::string mtl_config_xml_buf = {};
            {
                enum EReadMtlCofigXmlStatus
                {
                    SEARCH,
                    FOUND,
                    END,
                };
                std::ifstream ifs(material_config_list[i].impl_file_path);
                if(ifs.is_open())
                {
                    EReadMtlCofigXmlStatus status = EReadMtlCofigXmlStatus::SEARCH;
                
                    std::string line_buf;
                    while(std::getline(ifs, line_buf))
                    {
                        if(EReadMtlCofigXmlStatus::SEARCH == status)
                        {
                            if(0 == line_buf.compare(k_material_config_xml_begin_tag))
                                status = EReadMtlCofigXmlStatus::FOUND;
                        }
                        else if(EReadMtlCofigXmlStatus::FOUND == status)
                        {
                            if(0 == line_buf.compare(k_material_config_xml_end_tag))
                                status = EReadMtlCofigXmlStatus::END;
                        }
                        else
                        {
                            break;
                        }
                        if(EReadMtlCofigXmlStatus::SEARCH != status)
                        {
                            mtl_config_xml_buf += line_buf;// XML部分を抽出.
                        }
                    }

                    // 正常にXML部読み取り出来ていない.
                    if(EReadMtlCofigXmlStatus::END != status)
                    {
                        std::cout << "[ERROR] Invalid  Material Config XML. " << material_config_list[i].name << std::endl;
                        mtl_config_xml_buf = {};// 失敗しているのでクリア.
                    }
                }
            }

            // XML部の解析.
            // mtl_config_xml_buf をXMLとして解釈してShaderFile生成情報取得.
            tinyxml2::XMLDocument xml_doc;
            if(tinyxml2::XML_SUCCESS == xml_doc.Parse(mtl_config_xml_buf.c_str()))
            {
                auto* xml_root = xml_doc.RootElement();
                // Root直下巡回.
                auto* xml_pass_tag = xml_root->FirstChildElement();
                while(xml_pass_tag)
                {
                    // Pass情報.
                    if(0 == strncmp(xml_pass_tag->Name(), k_material_config_pass_tag, 4))
                    {
                        if(auto* pass_name = xml_pass_tag->Attribute("name"))
                        {
                            material_config_list[i].pass.push_back(pass_name);
                        }
                    }

                    // to next.
                    xml_pass_tag = xml_pass_tag->NextSiblingElement();
                }
            }
        }

        struct MaterialPassShaderSeed
        {
            int material_index = -1;    // to material_config_list index.
            int pass_index = -1;        // to pass_shader_info_list index.

            // 頂点入力パターンやVariationパターン等も入るかも.
        };
        using ShaderSeedList = std::vector<MaterialPassShaderSeed>;
        std::vector<ShaderSeedList> per_mtl_shader_seed_list = {};
        per_mtl_shader_seed_list.resize(material_config_list.size());
        
        for(size_t mtl_i = 0; mtl_i < material_config_list.size(); ++mtl_i)
        {
            const auto& mtl_config = material_config_list[mtl_i];
            // Passが存在しないものはありえない.
            if( 0 >= mtl_config.pass.size())
                continue;

            for(size_t pass_i = 0; pass_i < mtl_config.pass.size(); ++pass_i)
            {
                auto pass_it = pass_shader_info_map.find(mtl_config.pass[pass_i]);
                if(pass_shader_info_map.end() == pass_it)
                    continue;
                
                int pass_index = pass_it->second;

                per_mtl_shader_seed_list[mtl_i].push_back({});
                MaterialPassShaderSeed& seed = per_mtl_shader_seed_list[mtl_i].back();
                {
                    seed.material_index = static_cast<int>(mtl_i);
                    seed.pass_index = pass_index;
                }
            }
        }
        
        // 収集した情報から generated へMaterialPassシェーダを出力する. TODO.
        for(size_t mtl_i = 0; mtl_i < per_mtl_shader_seed_list.size(); ++mtl_i)
        {
            const auto& per_mtl_seed = per_mtl_shader_seed_list[mtl_i];
            if(0 >= per_mtl_seed.size())
                    continue;

            std::filesystem::path mtl_pass_shader_dir_path = generated_dir_path;
            mtl_pass_shader_dir_path += "/" + material_config_list[mtl_i].name;

            // 既存のMaterialディレクトリ削除.
            if(is_directory(mtl_pass_shader_dir_path))
            {
                std::filesystem::remove_all(mtl_pass_shader_dir_path);// 内部ファイルもすべて削除.
            }
            // Materialディレクトリ再生成.
            std::filesystem::create_directories(mtl_pass_shader_dir_path);

            for(const auto& pass_seed : per_mtl_seed)
            {
                const int mtl_index = static_cast<int>(pass_seed.material_index);
                const int pass_index = static_cast<int>(pass_seed.pass_index);

                // material.pass
                const std::string shader_base_name = material_config_list[mtl_index].name + "." + pass_shader_info_list[pass_index].name;

                // Stage毎に生成.
                for(const auto& stage : k_stage_names)
                {
                    // material.pass.stage
                    const std::string stage_shader_name = shader_base_name + "." + stage;
                    
                    std::filesystem::path shader_path = mtl_pass_shader_dir_path;
                    shader_path += "/" + stage_shader_name + ".hlsl";
                    {
                        // Open.
                        std::ofstream ofs(shader_path);
                        assert(ofs.is_open());

                        // 生成Shaderディレクトリからの相対パス.
                        std::filesystem::path rel_impl_path = std::filesystem::path(material_config_list[mtl_index].impl_file_path).lexically_relative(mtl_pass_shader_dir_path);
                        std::filesystem::path rel_pass_path = std::filesystem::path(pass_shader_info_list[pass_index].path).lexically_relative(mtl_pass_shader_dir_path);

                        /*
                            #include "../../impl/opaque_standard.hlsli"
                            #include "../../pass/depth.hlsli"
                        */
                        // generic_string() で / 区切りに変換.
                        ofs << "#include " << "\"" << rel_impl_path.generic_string() << "\"" << std::endl;
                        ofs << "#include " << "\"" << rel_pass_path.generic_string() << "\"" << std::endl;

                        // Complete.
                        ofs.close();
                    }
                }
            }
        }

        return true;
    }
    
}
}
