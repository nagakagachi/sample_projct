// material_shader_generator.cpp

#include "material_shader_generator.h"
#include "ngl/file/file.h"

#include <filesystem>
#include <fstream>

// xml操作.
#include <iostream>
#include <tinyxml2.h>


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

        std::vector<PassShaderInfo> pass_shader_info_list = {};
        std::vector<MaterialConfig> material_config_list = {};

        
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

        // Passシェーダ情報収集.
        for(auto it : std::filesystem::directory_iterator(pass_dir_path))
        {
            if(it.is_directory())
                continue;

            if(0 == it.path().extension().compare(".hlsli"))
            {
                pass_shader_info_list.push_back({});

                auto& pass_shader_info = pass_shader_info_list.back();
                {
                    pass_shader_info.path = it.path().string();
                    pass_shader_info.name = it.path().stem().string();
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

        // 収集した情報から generated へMaterialPassシェーダを出力する. TODO.

        return true;
    }
    
}
}
