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
    enum EMaterialShaderNamePart
    {
        MATERIAL_NAME,
        PASS_NAME,
        SHADER_STAGE_NAME,

        _MAX
    };
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
    
    struct MaterialPassPso
    {
        std::string pass_name = {};
        rhi::RhiRef<rhi::GraphicsPipelineStateDep> ref_pso = {};
    };
    struct MaterialPassPsoSet
    {
        std::string material_name = {};
            
        std::vector<MaterialPassPso> pass_pso_set = {};
        std::unordered_map<std::string, int> pass_pso_name_index = {};
    };
    
    class MaterialShaderManagerImpl
    {
    public:
        std::vector<MaterialShaderSet> material_shader_set_ = {};
        std::unordered_map<std::string, int> material_shader_name_index_ = {};

        
        std::vector<MaterialPassPsoSet> material_pso_ = {};
        std::unordered_map<std::string, int> material_pso_name_index_ = {};
    };

    
    MaterialShaderManager::MaterialShaderManager()
    {
        p_impl_ = new MaterialShaderManagerImpl();
    }
    MaterialShaderManager::~MaterialShaderManager()
    {
        if(p_impl_)
        {
            delete p_impl_;
            p_impl_ = {};
        }
    }

    //  generated_shader_root_dir : マテリアルシェーダディレクトリ. ここに マテリアル名/マテリアル毎のPassシェーダ群 が生成される.
    bool MaterialShaderManager::Setup(rhi::DeviceDep* p_device, const char* generated_shader_root_dir)
    {
        assert(p_device);
        p_device_ = p_device;
        
        
        std::filesystem::path root_dir_path = generated_shader_root_dir;
        if(!exists(root_dir_path) || !is_directory(root_dir_path))
        {
            assert(false);
            return false;
        }


        auto& material_shader_set = p_impl_->material_shader_set_;
        auto& material_shader_name_index = p_impl_->material_shader_name_index_;

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

        // PipelineStateObject生成.
        auto& material_pso_set = p_impl_->material_pso_;
        auto& material_pso_name_index = p_impl_->material_pso_name_index_;
        for(size_t mtl_i = 0; mtl_i < material_shader_set.size(); ++mtl_i)
        {
            auto& mtl_set = material_shader_set[mtl_i];

            // Mapに未登録なら新規追加.
            if(material_pso_name_index.end() == material_pso_name_index.find(mtl_set.material_name))
            {
                material_pso_name_index[mtl_set.material_name] = static_cast<int>(material_pso_set.size());
                material_pso_set.push_back({});
                {
                    auto& new_elem = material_pso_set.back();
                    new_elem.material_name = mtl_set.material_name;
                }
            }

            
            auto& pass_pso_set = material_pso_set[material_pso_name_index[mtl_set.material_name]];
            for(size_t pass_i = 0; pass_i < mtl_set.pass_shader_set.size(); ++pass_i)
            {
                auto& pass_set = mtl_set.pass_shader_set[pass_i];

                // VS-PS Pso.
                if(pass_set.res_vs.IsValid() && pass_set.res_ps.IsValid())
                {
                    if(pso_creator_map_.end() == pso_creator_map_.find(pass_set.pass_name))
                    {
                        assert(false);
                        continue;
                    }

                    MaterialPassPsoDesc pso_desc = {};
                    {
                        pso_desc.p_vs = pass_set.res_vs.Get();
                        pso_desc.p_ps = pass_set.res_ps.Get();
                        // TODO. option.
                    }
                    rhi::RhiRef<rhi::GraphicsPipelineStateDep> ref_pso = pso_creator_map_[pass_set.pass_name]->Create(p_device_, pso_desc);
                    if(!ref_pso.IsValid())
                    {
                        assert(false);
                        continue;
                    }

                    // 登録.
                    if(pass_pso_set.pass_pso_name_index.end() == pass_pso_set.pass_pso_name_index.find(pass_set.pass_name))
                    {
                        pass_pso_set.pass_pso_name_index[pass_set.pass_name] = static_cast<int>(pass_pso_set.pass_pso_set.size());
                        pass_pso_set.pass_pso_set.push_back({});
                        {
                            auto& new_elem = pass_pso_set.pass_pso_set.back();
                            new_elem.pass_name = pass_set.pass_name;
                            new_elem.ref_pso = ref_pso;
                        }
                    }
                }
            }
        }
        
        return true;
    }

    void MaterialShaderManager::Finalize()
    {
        (*p_impl_) = {};
        pso_creator_map_ = {};
        p_device_ = {};
    }

    void MaterialShaderManager::RegisterPassPsoCreator(const char* name, IMaterialPassPsoCreator* p_instance)
    {
        if(pso_creator_map_.end() != pso_creator_map_.find(name))
        {
            // 二重登録はエラー.
            assert(false);
            return;
        }
        pso_creator_map_.insert( std::make_pair(name, p_instance));
    }

    // マテリアル名と追加情報からPipeline取得.
    rhi::GraphicsPipelineStateDep* MaterialShaderManager::FindMaterialPipeline(const char* material_name, const char* pass_name) const
    {
        // Material検索.
        if(p_impl_->material_pso_name_index_.end() == p_impl_->material_pso_name_index_.find(material_name))
        {
            return {};
        }

        // Material内のPass検索.
        auto& mtl_pso_set = p_impl_->material_pso_[p_impl_->material_pso_name_index_[material_name]];
        if(mtl_pso_set.pass_pso_name_index.end() == mtl_pso_set.pass_pso_name_index.find(pass_name))
        {
            return {};
        }

        return mtl_pso_set.pass_pso_set[mtl_pso_set.pass_pso_name_index[pass_name]].ref_pso.Get();
    }

    
    // Depth Pass用PSO生成.
    rhi::GraphicsPipelineStateDep* MaterialPassPsoCreator_depth::Create(rhi::DeviceDep* p_device, const MaterialPassPsoDesc& pass_pso_desc)
    {   
        ngl::rhi::GraphicsPipelineStateDep::Desc desc = {};
        desc.vs = &pass_pso_desc.p_vs->data_;
        desc.ps = &pass_pso_desc.p_ps->data_;

        desc.depth_stencil_state.depth_enable = true;
        desc.depth_stencil_state.depth_func = ngl::rhi::ECompFunc::Greater; // ReverseZ.
        desc.depth_stencil_state.depth_write_enable = true;
        desc.depth_stencil_state.stencil_enable = false;
        desc.depth_stencil_format = k_depth_format;

        // 入力レイアウト
        std::array<ngl::rhi::InputElement, 5> input_elem_data;
        desc.input_layout.num_elements = static_cast<ngl::u32>(input_elem_data.size());
        desc.input_layout.p_input_elements = input_elem_data.data();
        {
            input_elem_data[0].semantic_name = ngl::gfx::MeshVertexSemantic::SemanticNameStr(ngl::gfx::EMeshVertexSemanticKind::POSITION);
            input_elem_data[0].semantic_index = 0;
            input_elem_data[0].format = ngl::rhi::EResourceFormat::Format_R32G32B32_FLOAT;
            input_elem_data[0].stream_slot = ngl::gfx::MeshVertexSemantic::SemanticSlot(ngl::gfx::EMeshVertexSemanticKind::POSITION, 0);
            input_elem_data[0].element_offset = 0;

            input_elem_data[1].semantic_name = ngl::gfx::MeshVertexSemantic::SemanticNameStr(ngl::gfx::EMeshVertexSemanticKind::NORMAL);
            input_elem_data[1].semantic_index = 0;
            input_elem_data[1].format = ngl::rhi::EResourceFormat::Format_R32G32B32_FLOAT;
            input_elem_data[1].stream_slot = ngl::gfx::MeshVertexSemantic::SemanticSlot(ngl::gfx::EMeshVertexSemanticKind::NORMAL, 0);
            input_elem_data[1].element_offset = 0;
						
            input_elem_data[2].semantic_name = ngl::gfx::MeshVertexSemantic::SemanticNameStr(ngl::gfx::EMeshVertexSemanticKind::TANGENT);
            input_elem_data[2].semantic_index = 0;
            input_elem_data[2].format = ngl::rhi::EResourceFormat::Format_R32G32B32_FLOAT;
            input_elem_data[2].stream_slot = ngl::gfx::MeshVertexSemantic::SemanticSlot(ngl::gfx::EMeshVertexSemanticKind::TANGENT, 0);
            input_elem_data[2].element_offset = 0;
						
            input_elem_data[3].semantic_name = ngl::gfx::MeshVertexSemantic::SemanticNameStr(ngl::gfx::EMeshVertexSemanticKind::BINORMAL);
            input_elem_data[3].semantic_index = 0;
            input_elem_data[3].format = ngl::rhi::EResourceFormat::Format_R32G32B32_FLOAT;
            input_elem_data[3].stream_slot = ngl::gfx::MeshVertexSemantic::SemanticSlot(ngl::gfx::EMeshVertexSemanticKind::BINORMAL, 0);
            input_elem_data[3].element_offset = 0;

            input_elem_data[4].semantic_name = ngl::gfx::MeshVertexSemantic::SemanticNameStr(ngl::gfx::EMeshVertexSemanticKind::TEXCOORD);
            input_elem_data[4].semantic_index = 0;
            input_elem_data[4].format = ngl::rhi::EResourceFormat::Format_R32G32_FLOAT;
            input_elem_data[4].stream_slot = ngl::gfx::MeshVertexSemantic::SemanticSlot(ngl::gfx::EMeshVertexSemanticKind::TEXCOORD, 0);
            input_elem_data[4].element_offset = 0;
        }
        // PSO生成.
        auto p_pso = new rhi::GraphicsPipelineStateDep();
        if (!p_pso->Initialize(p_device, desc))
        {
            assert(false);
            return {};
        }
        return p_pso;
    }
    // GBuffer Pass用PSO生成.
    rhi::GraphicsPipelineStateDep* MaterialPassPsoCreator_gbuffer::Create(rhi::DeviceDep* p_device, const MaterialPassPsoDesc& pass_pso_desc)
    {   
		ngl::rhi::GraphicsPipelineStateDep::Desc desc = {};
		desc.vs = &pass_pso_desc.p_vs->data_;
		desc.ps = &pass_pso_desc.p_ps->data_;
		{
			desc.num_render_targets = 5;
			desc.render_target_formats[0] = k_gbuffer0_format;
			desc.render_target_formats[1] = k_gbuffer1_format;
			desc.render_target_formats[2] = k_gbuffer2_format;
			desc.render_target_formats[3] = k_gbuffer3_format;
			desc.render_target_formats[4] = k_velocity_format;
		}
		{
			desc.depth_stencil_state.depth_enable = true;
			desc.depth_stencil_state.depth_func = ngl::rhi::ECompFunc::Equal; // Maskedを含めたEarlyZ Full PreZのためEqual.
			desc.depth_stencil_state.depth_write_enable = false;// 描き込み無効. 深度テストのみで書き込み無効.
			desc.depth_stencil_state.stencil_enable = false;
			desc.depth_stencil_format = k_depth_format;
		}
		// 入力レイアウト
		std::array<ngl::rhi::InputElement, 5> input_elem_data;
		desc.input_layout.num_elements = static_cast<ngl::u32>(input_elem_data.size());
		desc.input_layout.p_input_elements = input_elem_data.data();
		{
			input_elem_data[0].semantic_name = ngl::gfx::MeshVertexSemantic::SemanticNameStr(ngl::gfx::EMeshVertexSemanticKind::POSITION);
			input_elem_data[0].semantic_index = 0;
			input_elem_data[0].format = ngl::rhi::EResourceFormat::Format_R32G32B32_FLOAT;
			input_elem_data[0].stream_slot = ngl::gfx::MeshVertexSemantic::SemanticSlot(ngl::gfx::EMeshVertexSemanticKind::POSITION, 0);
			input_elem_data[0].element_offset = 0;

			input_elem_data[1].semantic_name = ngl::gfx::MeshVertexSemantic::SemanticNameStr(ngl::gfx::EMeshVertexSemanticKind::NORMAL);
			input_elem_data[1].semantic_index = 0;
			input_elem_data[1].format = ngl::rhi::EResourceFormat::Format_R32G32B32_FLOAT;
			input_elem_data[1].stream_slot = ngl::gfx::MeshVertexSemantic::SemanticSlot(ngl::gfx::EMeshVertexSemanticKind::NORMAL, 0);
			input_elem_data[1].element_offset = 0;
			
			input_elem_data[2].semantic_name = ngl::gfx::MeshVertexSemantic::SemanticNameStr(ngl::gfx::EMeshVertexSemanticKind::TANGENT);
			input_elem_data[2].semantic_index = 0;
			input_elem_data[2].format = ngl::rhi::EResourceFormat::Format_R32G32B32_FLOAT;
			input_elem_data[2].stream_slot = ngl::gfx::MeshVertexSemantic::SemanticSlot(ngl::gfx::EMeshVertexSemanticKind::TANGENT, 0);
			input_elem_data[2].element_offset = 0;
			
			input_elem_data[3].semantic_name = ngl::gfx::MeshVertexSemantic::SemanticNameStr(ngl::gfx::EMeshVertexSemanticKind::BINORMAL);
			input_elem_data[3].semantic_index = 0;
			input_elem_data[3].format = ngl::rhi::EResourceFormat::Format_R32G32B32_FLOAT;
			input_elem_data[3].stream_slot = ngl::gfx::MeshVertexSemantic::SemanticSlot(ngl::gfx::EMeshVertexSemanticKind::BINORMAL, 0);
			input_elem_data[3].element_offset = 0;

			input_elem_data[4].semantic_name = ngl::gfx::MeshVertexSemantic::SemanticNameStr(ngl::gfx::EMeshVertexSemanticKind::TEXCOORD);
			input_elem_data[4].semantic_index = 0;
			input_elem_data[4].format = ngl::rhi::EResourceFormat::Format_R32G32_FLOAT;
			input_elem_data[4].stream_slot = ngl::gfx::MeshVertexSemantic::SemanticSlot(ngl::gfx::EMeshVertexSemanticKind::TEXCOORD, 0);
			input_elem_data[4].element_offset = 0;
		}
        // PSO生成.
        auto p_pso = new rhi::GraphicsPipelineStateDep();
        if (!p_pso->Initialize(p_device, desc))
        {
            assert(false);
            return {};
        }
        return p_pso;
    }
    
}
}
