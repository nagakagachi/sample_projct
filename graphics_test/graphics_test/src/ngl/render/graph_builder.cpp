#pragma once

#include "graph_builder.h"

#include "ngl/rhi/d3d12/command_list.d3d12.h"


namespace ngl
{

	// Render Task Graph 検証実装.
	namespace rtg
	{

		
		// オペレータ.
		constexpr bool TaskStage::operator<(const TaskStage arg) const
		{
			if(stage_ < arg.stage_)
				return true;
			else if(stage_ == arg.stage_)
				return step_ < arg.step_;
			return false;
		}
		constexpr bool TaskStage::operator>(const TaskStage arg) const
		{
			if(stage_ > arg.stage_)
				return true;
			else if(stage_ == arg.stage_)
				return step_ > arg.step_;
			return false;
		}
		constexpr bool TaskStage::operator<=(const TaskStage arg) const
		{
			if(stage_ < arg.stage_)
				return true;
			else if(stage_ == arg.stage_)
				return step_ <= arg.step_;
			return false;
		}
		constexpr bool TaskStage::operator>=(const TaskStage arg) const
		{
			if(stage_ > arg.stage_)
				return true;
			else if(stage_ == arg.stage_)
				return step_ >= arg.step_;
			return false;
		}

		

		// リソースハンドルを生成.
		ResourceHandle RenderTaskGraphBuilder::CreateResource(ResourceDesc2D res_desc)
		{
			// ID確保.
			{
				++s_res_handle_id_counter_;
				if (0 == s_res_handle_id_counter_)
					++s_res_handle_id_counter_;// 0は無効ID扱いのためスキップ.
			}
			
			ResourceHandle handle{};
			handle.detail.unique_id = s_res_handle_id_counter_;// ユニークID割当.

			// Desc登録.
			{
				handle_2_desc_[handle] = res_desc;// desc記録.
			}
			
			return handle;
		}

		// 外部リソースを登録共通部.
		ResourceHandle RenderTaskGraphBuilder::RegisterExternalResourceCommon(
			rhi::RefTextureDep tex, rhi::RhiRef<rhi::SwapChainDep> swapchain, rhi::RefRtvDep rtv, rhi::RefDsvDep dsv, rhi::RefSrvDep srv, rhi::RefUavDep uav,
			rhi::ResourceState curr_state, rhi::ResourceState nesesary_end_state)
		{
			// TODO. リソースポインタのMapで二重登録チェックもした方が良い.

			// 無効なリソースチェック.
			if (!swapchain.IsValid() && !tex.IsValid())
			{
				std::cout << u8"[RenderTaskGraphBuilder][RegisterExternalResource] 外部リソース登録のResourceが不正です." << std::endl;
				assert(false);
			}
			// ID確保.
			{
				++s_res_handle_id_counter_;
				if(0 == s_res_handle_id_counter_)
					++s_res_handle_id_counter_;// 0は無効ID扱いのためスキップ.
			}
			// ハンドルセットアップ.
			ResourceHandle new_handle = {};
			{
				new_handle.detail.is_external = 1;// 外部リソースマーク.
				new_handle.detail.is_swapchain = (swapchain.IsValid())? 1 : 0;// Swapchainマーク.
				new_handle.detail.unique_id = s_res_handle_id_counter_;
			}
			
			// ResourceのDescをHandleから引ける用に登録.
			ResourceDesc2D res_desc = {};
			{
				if(swapchain.IsValid())
				{
					res_desc = ResourceDesc2D::CreateAsAbsoluteSize(swapchain->GetWidth(), swapchain->GetHeight(), swapchain->GetDesc().format);
				}
				else
				{
					res_desc = ResourceDesc2D::CreateAsAbsoluteSize(tex->GetWidth(), tex->GetHeight(), tex->GetDesc().format);
				}
				handle_2_desc_[new_handle] = res_desc;// desc記録.
			}

			// 外部リソース情報.
			{
				// 外部リソース用Index.
				const int res_index = (int)ex_resource_.size();
				ex_resource_.push_back({});

				// 外部リソースハンドルから外部リソース用IndexへのMap.
				ex_handle_2_index_[new_handle] = res_index;

				// 外部リソース用Indexで情報登録.
				ExternalResourceRegisterInfo& ex_res_info = ex_resource_[res_index];
				{
					ex_res_info.swapchain_ = swapchain;
					ex_res_info.tex_ = tex;
					ex_res_info.rtv_ = rtv;
					ex_res_info.dsv_ = dsv;
					ex_res_info.srv_ = srv;
					ex_res_info.uav_ = uav;
				
					ex_res_info.require_begin_state_ = curr_state;
					ex_res_info.require_end_state_ = nesesary_end_state;
				
					ex_res_info.cached_state_ = curr_state;
					ex_res_info.prev_cached_state_ = curr_state;
					ex_res_info.last_access_stage_ = TaskStage::k_frontmost_stage();
				}
			}
			
			return new_handle;
		}

		// 外部リソースの登録. 一般.
		ResourceHandle RenderTaskGraphBuilder::RegisterExternalResource(
			rhi::RefTextureDep tex, rhi::RefRtvDep rtv, rhi::RefDsvDep dsv, rhi::RefSrvDep srv, rhi::RefUavDep uav,
			rhi::ResourceState curr_state, rhi::ResourceState nesesary_end_state)
		{
			handle_ex_swapchain_ = RegisterExternalResourceCommon(tex, {}, rtv, {}, {}, {}, curr_state, nesesary_end_state);
			return handle_ex_swapchain_;
		}
		
		// 外部リソースの登録. Swapchain.
		ResourceHandle RenderTaskGraphBuilder::RegisterExternalResource(rhi::RhiRef<rhi::SwapChainDep> swapchain, rhi::RefRtvDep rtv, rhi::ResourceState curr_state, rhi::ResourceState nesesary_end_state)
		{
			handle_ex_swapchain_ = RegisterExternalResourceCommon({}, swapchain, rtv, {}, {}, {}, curr_state, nesesary_end_state);
			return handle_ex_swapchain_;
		}

		// Swapchainリソースハンドルを取得. RegisterExternalResourceで登録しておく必要がある.
		ResourceHandle RenderTaskGraphBuilder::GetSwapchainResourceHandle() const
		{
			return handle_ex_swapchain_;
		}
		
		// Descを取得.
		ResourceDesc2D RenderTaskGraphBuilder::GetRegisteredResouceHandleDesc(ResourceHandle handle) const
		{
			const auto find_it = handle_2_desc_.find(handle);
			if(handle_2_desc_.end() == find_it)
			{
				// 未登録のHandleの定義取得は不正.
				assert(false);
				return {};// 一応空を返しておく.
			}
			return find_it->second;
		}

		// Nodeからのリソースアクセスを記録.
		// NodeのRender実行順と一致する順序で登録をする必要がある. この順序によってリソースステート遷移の確定や実リソースの割当等をする.
		ResourceHandle RenderTaskGraphBuilder::RegisterResourceAccess(const ITaskNode& node, ResourceHandle res_handle, ACCESS_TYPE access_type)
		{
			// Node->Handle&AccessTypeのMap登録.
			{
				if(node_handle_usage_list_.end() == node_handle_usage_list_.find(&node))
				{
					node_handle_usage_list_[&node] = {};
				}

				NodeHandleUsageInfo push_info = {};
				push_info.handle = res_handle;
				push_info.access = access_type;
				node_handle_usage_list_[&node].push_back(push_info);
			}

			// Passメンバに保持するコードを短縮するためHandleをそのままリターン.
			return res_handle;
		}
		// 指定したHandleのリソースを外部へエクスポートできるようにする.
		//	エクスポートされたリソースは内部プールから外部リソースへ移行し, Compile後かつExecute前の期間で取得できるようになる.
		//	なおExecuteによってBuilder内での外部リソース参照はクリアされる(参照カウント減少).
		ResourceHandle RenderTaskGraphBuilder::ExportResource(ResourceHandle handle)
		{
			handle_2_is_export_[handle] = true;

			// TODO.
			// エクスポートしたリソースは強制的にGraphの最後まで生存期間が伸ばされることになる(再利用で別用途で書き換えられないように)
			// そのほか内部プールから外部へ参照を移譲する必要がある.

			return handle;// とりあえずそのまま返す.
		}
		
		// グラフからリソース割当と状態遷移を確定.
		// CompileされたGraphは必ずExecuteが必要.
		bool RenderTaskGraphBuilder::Compile(RenderTaskGraphManager& manager)
		{
			// 多重Compileは禁止.
			assert(!is_compiled_);
			if (is_compiled_)
			{
				return false;
			}
			
			// Compileでリソース割当をするマネージャを保持.
			p_compiled_manager_ = &manager;

			
			// MEMO 終端に寄与しないノードのカリングは保留.
			
			// 念のためのValidation Check.
			{
				for(int sequence_id = 0; sequence_id < node_sequence_.size(); ++sequence_id)
				{
					ITaskNode* p_node = node_sequence_[sequence_id];
					assert(node_handle_usage_list_.end() != node_handle_usage_list_.find(p_node));// ありえない.

					// Nodeが同じHandleに対して重複したRegisterを指定ないかチェック.
					for(int i = 0; i < node_handle_usage_list_[p_node].size() - 1; ++i)
					{
						for(int j = i + 1; j < node_handle_usage_list_[p_node].size(); ++j)
						{
							if(node_handle_usage_list_[p_node][i].handle == node_handle_usage_list_[p_node][j].handle)
							{
								std::cout << u8"[RenderTaskGraphBuilder][Validation Error] 同一リソースへの重複アクセス登録." <<std::endl;
								assert(false);
							}
						}
					}
				}
			}

			
			std::vector<TaskStage> node_stage_array = {};// Sequence上のインデックスからタスクステージ情報を引く.
			{
				int stage_counter = 0;
				int step_counter = 0;
				for(int node_i = 0; node_i < node_sequence_.size(); ++node_i)
				{
					// Fence等でStageが分かれる場合はここ
					if(false)
					{
						++stage_counter;
						step_counter = 0;
					}
					TaskStage node_stage = {};
					node_stage.stage_ = stage_counter;
					node_stage.step_ = step_counter;
					node_stage_array.push_back(node_stage);

					++step_counter;
				}
			}

			// 存在するハンドル毎に仮のユニークインデックスを割り振る. ランダムアクセスのため.
			handle_2_compiled_index_ = {};// クリア.
			int handle_count = 0;
			for(int node_i = 0; node_i < node_sequence_.size(); ++node_i)
			{
				const ITaskNode* p_node = node_sequence_[node_i];
				// このNodeのリソースアクセス情報を巡回.
				for(auto& res_access : node_handle_usage_list_[p_node])
				{
					if(handle_2_compiled_index_.end() == handle_2_compiled_index_.find(res_access.handle))
					{
						// このResourceHandleの要素が未登録なら新規.
						handle_2_compiled_index_[res_access.handle] = handle_count;
						++handle_count;
					}
				}
			}

			
			// SequenceのNodeを順に処理して実リソースの割当とそのステート遷移確定をしていく.
			struct ResourceHandleAccessFromNode
			{
				const ITaskNode*	p_node_ = {};
				ACCESS_TYPE			access_type = access_type::INVALID;
			};
			struct ResourceHandleAccessInfo
			{
				bool access_pattern_[access_type::_MAX] = {};
				std::vector<ResourceHandleAccessFromNode> from_node_ = {};
			};
			// リソースハンドルへのアクセスを時系列で収集.
			std::vector<ResourceHandleAccessInfo> handle_access_info_array(handle_count);
			for(int node_i = 0; node_i < node_sequence_.size(); ++node_i)
			{
				const ITaskNode* p_node = node_sequence_[node_i];
				// このNodeのリソースアクセス情報を巡回.
				for(auto& res_access : node_handle_usage_list_[p_node])
				{
					const auto handle_index = handle_2_compiled_index_[res_access.handle];
					
					// このResourceHandleへのNodeからのアクセスを順にリストアップ.
					ResourceHandleAccessFromNode access_flow_part = {};
					access_flow_part.p_node_ = p_node;
					access_flow_part.access_type = res_access.access;

					// ノードからのアクセス情報を追加.
					handle_access_info_array[handle_index].from_node_.push_back(access_flow_part);
					// アクセスパターンタイプに追加.
					handle_access_info_array[handle_index].access_pattern_[res_access.access] = true;
					
				}
			}
				// アクセスタイプのValidation. ここで RenderTarget且つDepthStencilTarget等の許可されないアクセスチェック.
				for(auto handle_access : handle_access_info_array)
				{
					if(
						handle_access.access_pattern_[access_type::RENDER_TARTGET]
						&&
						handle_access.access_pattern_[access_type::DEPTH_TARGET]
						)
					{
						std::cout << u8"RenderTarget と DepthStencilTarget を同時に指定することは不許可." << std::endl;
						assert(false);
						return false;
					}
				}

			
			// リソースハンドル毎のアクセス期間を計算.
			std::vector<TaskStage> handle_life_first_array(handle_count);
			std::vector<TaskStage> handle_life_last_array(handle_count);
			for(auto handle_index = 0; handle_index < handle_access_info_array.size(); ++handle_index)
			{
				TaskStage stage_first = {std::numeric_limits<int>::max(), std::numeric_limits<int>::max()};// 最大値初期化.
				TaskStage stage_last = {std::numeric_limits<int>::min(), std::numeric_limits<int>::min()};// 最小値初期化.

				for(const auto access_node : handle_access_info_array[handle_index].from_node_)
				{
					const int node_pos = GetNodeSequencePosition(access_node.p_node_);
					stage_first = std::min(stage_first, node_stage_array[node_pos]);// operatorオーバーロード解決.
					stage_last = std::max(stage_last, node_stage_array[node_pos]);
				}

				handle_life_first_array[handle_index] = stage_first;// このハンドルへの最初のアクセス位置
				handle_life_last_array[handle_index] = stage_last;// このハンドルへの最後のアクセス位置
			}
			
			// リソースハンドル毎にPoolから実リソースを割り当てる.
			// ハンドルのアクセス期間を元に実リソースの再利用も可能.
			handle_compiled_resource_id_.clear();
			handle_compiled_resource_id_.resize(handle_count, CompiledResourceInfo::k_invalid());// 無効値-1でHandle個数分初期化.
			for(auto handle_index : handle_2_compiled_index_)
			{
				const ResourceHandle res_handle = ResourceHandle(handle_index.first);
				const auto handle_id = handle_index.second;
				
				// ユニーク割当IDでまだ未割り当ての場合は新規割当.
				if(0 > handle_compiled_resource_id_[handle_id].detail.resource_id)
				{
					// 実際はここでPool等から実リソースを割り当て, 以前のステートを引き継いで遷移を確定させる.
					// 理想的には unique_id は違うが寿命がオーバーラップしていない再利用可能実リソースを使い回す.

					const ResourceHandleAccessInfo& handle_access = handle_access_info_array[handle_id];

					if(res_handle.detail.is_external || res_handle.detail.is_swapchain)
					{
						// 外部リソースの場合.
						
						assert(ex_handle_2_index_.end() != ex_handle_2_index_.find(res_handle));// 登録済み外部リソースかチェック.

						const int ex_res_index = ex_handle_2_index_[res_handle];
						// リソースの最終アクセスステージを更新.
						{
							ex_resource_[ex_res_index].last_access_stage_ = handle_life_last_array[handle_id];
						}
						// 割当情報.
						{
							handle_compiled_resource_id_[handle_id].detail.resource_id = ex_res_index;
							handle_compiled_resource_id_[handle_id].detail.is_external = true;// 外部リソースマーク.
						}
					}
					else
					{
						// 内部リソースの場合.
						
						const auto require_desc = handle_2_desc_[res_handle];

						int concrete_w = res_base_width_;
						int concrete_h = res_base_height_;
						// MEMO ここで相対サイズモードの場合はスケールされたサイズになるが, このまま要求して新規生成された場合小さいサイズで作られて使いまわしされにくいものになる懸念が多少ある.
						require_desc.GetConcreteTextureSize(res_base_width_, res_base_height_, concrete_w, concrete_h);

						// アクセスタイプでUsageを決定. 同時指定が不可能なパターンのチェックはこれ以前に実行している予定.
						ACCESS_TYPE_MASK usage_mask = 0;
						{
							if(handle_access.access_pattern_[access_type::RENDER_TARTGET])
								usage_mask |= access_type_mask::RENDER_TARTGET;
							if(handle_access.access_pattern_[access_type::DEPTH_TARGET])
								usage_mask |= access_type_mask::DEPTH_TARGET;
							if(handle_access.access_pattern_[access_type::UAV])
								usage_mask |= access_type_mask::UAV;
							if(handle_access.access_pattern_[access_type::SHADER_READ])
								usage_mask |= access_type_mask::SHADER_READ;
						}
					
						ResourceSearchKey search_key = {};
						{
							search_key.format = require_desc.desc.format;
							search_key.require_width_ = concrete_w;
							search_key.require_height_ = concrete_h;
							search_key.usage_ = usage_mask;
						}

#if 1
						// リソースのアクセス範囲を考慮して再利用可能なら再利用する
						const TaskStage first_stage = handle_life_first_array[handle_id];
						const TaskStage* p_request_access_stage = &first_stage;
#else
						// 再利用を一切しないデバッグ用.
						const TaskStage* p_request_access_stage = nullptr;
#endif

						// 内部リソースプールからリソース取得.
						int res_id = p_compiled_manager_->GetOrCreateResourceFromPool(search_key, p_request_access_stage);
						assert(0 <= res_id);// 必ず有効なIDが帰るはず.

						// 割当決定したリソースの最終アクセスステージを更新 (このハンドルの最終アクセスステージ).
						{
							p_compiled_manager_->internal_resource_pool_[res_id].last_access_stage_ = handle_life_last_array[handle_id];
						}
						// 割当情報.
						{
							handle_compiled_resource_id_[handle_id].detail.resource_id = res_id;
							handle_compiled_resource_id_[handle_id].detail.is_external = false;
						}
					}
				}
			}

			// Graph上で割り当てられた有効なリソースIDから密なリニアインデックスへのマップ. 有効リソースID毎の情報をワークバッファ上で操作するため.
			std::unordered_map<CompiledResourceInfoKeyType, int> res_id_2_linear_map = {};
			// 有効リソースリニアインデックスからリソースIDへのマッピングをする配列.
			std::vector<CompiledResourceInfo> res_linear_2_id_array = {};
			// Graph上で有効な実リソース数.
			int valid_res_count = 0;
			for(const auto& res_id : handle_compiled_resource_id_)
			{
				if(res_id_2_linear_map.end() == res_id_2_linear_map.find(res_id))
				{
					res_id_2_linear_map[res_id] = valid_res_count;
					res_linear_2_id_array.push_back(res_id);
					++valid_res_count;
				}
			}

			
			//	各Node時点での保持Handleのリソースステートを計算.
			// まず時系列順で実リソースへのアクセスNodeをリストアップ.
			std::vector<std::vector<const ITaskNode*>> res_access_node_array(valid_res_count);
			for(const auto* p_node : node_sequence_)
			{
				const auto& node_handles = node_handle_usage_list_[p_node];
				for(const auto& access_info : node_handles)
				{
					// access_info.access;
					const auto handle_id = handle_2_compiled_index_[access_info.handle];
					const auto res_id = handle_compiled_resource_id_[handle_id];

					const auto res_index = res_id_2_linear_map[res_id];

					// リソースに対して時系列でのアクセスNodeリスト.
					res_access_node_array[res_index].push_back(p_node);
				}
			}

			// 各Nodeの各Handleがその時点でどのようにステート遷移すべきかの情報を構築.
			node_handle_state_ = {};// クリア.
			for(int res_index = 0; res_index < res_access_node_array.size(); ++res_index)
			{
				const CompiledResourceInfo res_id = res_linear_2_id_array[res_index];
				
				assert(0 <= res_id.detail.resource_id);
				
				rhi::ResourceState begin_state = {};
				if(!res_id.detail.is_external)
				{
					begin_state = p_compiled_manager_->internal_resource_pool_[res_id.detail.resource_id].cached_state_;// 実リソースのCompile時点のステートから開始.
				}
				else
				{
					begin_state = ex_resource_[res_id.detail.resource_id].cached_state_;
				}
				
				rhi::ResourceState curr_state = begin_state;
				for(const auto* p_node : res_access_node_array[res_index])
				{
					for(const auto handle : node_handle_usage_list_[p_node])
					{
						const int handle_index = handle_2_compiled_index_[handle.handle];
						if(res_id == handle_compiled_resource_id_[handle_index])
						{
							// Handleへのアクセスタイプから次のrhiステートを決定.
							rhi::ResourceState next_state = {};
							if(access_type::RENDER_TARTGET == handle.access)
							{
								next_state = rhi::ResourceState::RenderTarget;
							}
							else if(access_type::DEPTH_TARGET == handle.access)
							{
								next_state = rhi::ResourceState::DepthWrite;
							}
							else if(access_type::UAV == handle.access)
							{
								next_state = rhi::ResourceState::UnorderedAccess;
							}
							else if(access_type::SHADER_READ == handle.access)
							{
								next_state = rhi::ResourceState::ShaderRead;
							}
							else
							{
								assert(false);
							}
							
							// このリソースに対してこのnode時点では cur_state -> next_state となる.
							NodeHandleState node_handle_state = {};
							{
								node_handle_state.prev_ = curr_state;
								node_handle_state.curr_ = next_state;
							}
							// Node毎のHandle時点での前回ステートと現在ステートを確定.
							node_handle_state_[p_node][handle.handle] = node_handle_state;
							
							// 次へ.
							curr_state = next_state;
							break;
						}
					}
				}

				// 最終ステートを保存.
				if(!res_id.detail.is_external)
				{
					// Compile前のステートを一応保持.
					p_compiled_manager_->internal_resource_pool_[res_id.detail.resource_id].prev_cached_state_
					= p_compiled_manager_->internal_resource_pool_[res_id.detail.resource_id].cached_state_;

					// Compile後のステートに更新.
					p_compiled_manager_->internal_resource_pool_[res_id.detail.resource_id].cached_state_ = curr_state;
				}
				else
				{
					ex_resource_[res_id.detail.resource_id].prev_cached_state_
					= ex_resource_[res_id.detail.resource_id].cached_state_;
					
					// Compile後のステートに更新.
					ex_resource_[res_id.detail.resource_id].cached_state_ = curr_state;
				}
			}
			
			
			// デバッグ表示.
			if(false)
			{
#if defined(_DEBUG)
				// 各ResourceHandleへの処理順でのアクセス情報.
				std::cout << "-Access Flow Debug" << std::endl;
				for(auto handle_index : handle_2_compiled_index_)
				{
					const auto handle = ResourceHandle(handle_index.first);
					const auto handle_id = handle_index.second;
					
					const auto& lifetime_first = handle_life_first_array[handle_id];
					const auto& lifetime_last = handle_life_last_array[handle_id];

					std::cout << "	-ResourceHandle ID " << handle << std::endl;
					std::cout << "		-FirstAccess " << static_cast<int>(lifetime_first.step_) << "/" << static_cast<int>(lifetime_first.stage_) << std::endl;
					std::cout << "		-LastAccess " << static_cast<int>(lifetime_last.step_) << "/" << static_cast<int>(lifetime_last.stage_) << std::endl;

					std::cout << "		-Resource" << std::endl;
					
					const auto res_id = handle_compiled_resource_id_[handle_id];
					if(!res_id.detail.is_external)
					{
						std::cout << "			-Internal" << std::endl;
						
						const auto res = (0 <= res_id.detail.resource_id)? p_compiled_manager_->internal_resource_pool_[res_id.detail.resource_id] : InternalResourceInstanceInfo();
						std::cout << "				-id " << res_id.detail.resource_id << std::endl;
						std::cout << "				-tex_ptr " << res.tex_.Get() << std::endl;
					}
					else
					{
						std::cout << "			-External" << std::endl;
						
						const auto res = (0 <= res_id.detail.resource_id)? ex_resource_[res_id.detail.resource_id] : ExternalResourceRegisterInfo();
						std::cout << "				-id " << res_id.detail.resource_id << std::endl;
						if(res.tex_.IsValid())
							std::cout << "				-tex_ptr " << res.tex_.Get() << std::endl;
						else if(res.swapchain_.IsValid())
							std::cout << "				-swapchain_ptr " << res.swapchain_.Get() << std::endl;
					}
					
					for(auto res_access : handle_access_info_array[handle_id].from_node_)
					{
						const auto prev_state = node_handle_state_[res_access.p_node_][handle].prev_;
						const auto curr_state = node_handle_state_[res_access.p_node_][handle].curr_;
						
						std::cout << "		-Node " << res_access.p_node_->GetDebugNodeName().Get() << std::endl;
						std::cout << "			-AccessType " << static_cast<int>(res_access.access_type) << std::endl;
						// 確定したステート遷移.
						std::cout << "			-PrevState " << static_cast<int>(prev_state) << std::endl;
						std::cout << "			-CurrState " << static_cast<int>(curr_state) << std::endl;
					}
				}
#endif
			}

			// Compile完了して実行準備ができたのでフラグ設定.
			is_compiled_ = true;
			
			return true;
		}

		RenderTaskGraphBuilder::AllocatedHandleResourceInfo RenderTaskGraphBuilder::GetAllocatedHandleResource(const ITaskNode* node, ResourceHandle res_handle)
		{
			// Compileされていない.
			assert(is_compiled_);
			assert(nullptr != p_compiled_manager_);
			
			if (!is_compiled_ || nullptr == p_compiled_manager_)
			{
				return {};
			}

			if (handle_2_compiled_index_.end() == handle_2_compiled_index_.find(res_handle))
			{
				assert(false);// 念のためMapに登録されているかチェック.
			}
			if (node_handle_state_.end() == node_handle_state_.find(node))
			{
				assert(false);// 念のためMapに登録されているかチェック.
			}
			if (node_handle_state_[node].end() == node_handle_state_[node].find(res_handle))
			{
				assert(false);// 念のためMapに登録されているかチェック.
			}
			// ステート遷移情報取得.
			NodeHandleState state_transition = node_handle_state_[node][res_handle];

			const int handle_id = handle_2_compiled_index_[res_handle];
			const CompiledResourceInfo handle_res_id = handle_compiled_resource_id_[handle_id];


			// 返却情報構築.
			AllocatedHandleResourceInfo ret_info = {};
			ret_info.prev_state_ = state_transition.prev_;
			ret_info.curr_state_ = state_transition.curr_;

			if (!handle_res_id.detail.is_external)
			{
				const InternalResourceInstanceInfo res = p_compiled_manager_->internal_resource_pool_[handle_res_id.detail.resource_id];
				ret_info.tex_ = res.tex_;
				ret_info.rtv_ = res.rtv_;
				ret_info.dsv_ = res.dsv_;
				ret_info.uav_ = res.uav_;
				ret_info.srv_ = res.srv_;
			}
			else
			{
				// 外部リソース.
				const ExternalResourceRegisterInfo res = ex_resource_[handle_res_id.detail.resource_id];
				ret_info.swapchain_ = res.swapchain_;// 外部リソースはSwapchainの場合もある.
				ret_info.tex_ = res.tex_;
				ret_info.rtv_ = res.rtv_;
				ret_info.dsv_ = res.dsv_;
				ret_info.uav_ = res.uav_;
				ret_info.srv_ = res.srv_;
			}

			return ret_info;
		}

		// Compileしたグラフを実行しCommandListを生成する. 検証用.
		void RenderTaskGraphBuilder::Execute_ImmediateDebug(rhi::RhiRef<rhi::GraphicsCommandListDep> commandlist)
		{
			// Compileされていない.
			assert(is_compiled_);
			assert(nullptr != p_compiled_manager_);
			if (!is_compiled_ || nullptr == p_compiled_manager_)
			{
				return;
			}

			// TaskNodeをシーケンス順に評価.
			for (const auto& e : node_sequence_)
			{
				// Nodeが登録したHandleを全て列挙. Nodeのdebug_ref_handles_はデバッグ用とであることと, メンバマクロ登録されたHandleしか格納されていないため, Builderに登録されたHandle全てを列挙するにはこの方法しかない.
				const auto& node_handle_access = node_handle_usage_list_[e];

				for (const auto& handle_access : node_handle_access)
				{
					AllocatedHandleResourceInfo handle_res = GetAllocatedHandleResource(e, handle_access.handle);
					if (handle_res.tex_.IsValid())
					{
						// 通常テクスチャリソースの場合.

						// 状態遷移コマンド発効..
						if (handle_res.prev_state_ != handle_res.curr_state_)
						{
							commandlist->ResourceBarrier(handle_res.tex_.Get(), handle_res.prev_state_, handle_res.curr_state_);
						}
					}
					else if (handle_res.swapchain_.IsValid())
					{
						// Swapchain(外部)の場合.
						
						// 状態遷移コマンド発効..
						if (handle_res.prev_state_ != handle_res.curr_state_)
						{
							commandlist->ResourceBarrier(handle_res.swapchain_.Get(), handle_res.swapchain_->GetCurrentBufferIndex(), handle_res.prev_state_, handle_res.curr_state_);
						}
					}
				}

				// BarrierをCommandListに正しく積んでからNodeを実行.
				// MEMO. 現状はシーケンス順にシングルスレッド.
				e->Run(*this, commandlist);
			}

			// 外部リソースの必須最終ステートの解決.
			{
				for(auto& ex_res : ex_resource_)
				{
					// CompileされたGraph内で最終的に遷移したステートが, 登録時に指定された最終ステートと異なる場合は追加で遷移コマンド.
					if(ex_res.require_end_state_ != ex_res.cached_state_)
					{
						if(ex_res.swapchain_.IsValid())
						{
							// Swapchainの場合.
							commandlist->ResourceBarrier(ex_res.swapchain_.Get(), ex_res.swapchain_->GetCurrentBufferIndex(), ex_res.cached_state_, ex_res.require_end_state_);
						}
						else
						{
							commandlist->ResourceBarrier(ex_res.tex_.Get(), ex_res.cached_state_, ex_res.require_end_state_);
						}
					}
				}
			}

			// ExecuteしたらCompile結果は無効になる(Poolのリソースのステートなどが変わるため再度Compileする必要がある).
			{
				is_compiled_ = false;
				p_compiled_manager_ = nullptr;

				// 外部リソースクリア.
				{
					ex_resource_ = {};
					ex_handle_2_index_ = {};

					handle_ex_swapchain_ = {};
				}
			}
		}

		// -------------------------------------------------------------------------------------------

		RenderTaskGraphBuilder::~RenderTaskGraphBuilder()
		{
			for (auto* p : node_sequence_)
			{
				if (p)
				{
					delete p;
					p = nullptr;
				}
			}
			node_sequence_.clear();
		}


		// Sequence上でのノードの位置を返す.
		// シンプルに直列なリスト上での位置.
		int RenderTaskGraphBuilder::GetNodeSequencePosition(const ITaskNode* p_node) const
		{
			int i = 0;
			auto find_pos = std::find(node_sequence_.begin(), node_sequence_.end(), p_node);
			
			if (node_sequence_.end() == find_pos)
				return -1;
			return static_cast<int>(std::distance(node_sequence_.begin(), find_pos));
		}


		// --------------------------------------------------------------------------------------------------------------------
		// Poolからリソース検索または新規生成.
		int RenderTaskGraphManager::GetOrCreateResourceFromPool(ResourceSearchKey key, const TaskStage* p_access_stage_for_reuse)
		{
			//	アクセスステージが引数で渡されなかった場合は未割り当てリソース以外を割り当てないようにTaskStage::k_frontmost_stage(負の最大)扱いとする.
			const TaskStage require_access_stage = (p_access_stage_for_reuse)? ((*p_access_stage_for_reuse)) : TaskStage::k_frontmost_stage();

			assert(nullptr != p_device_);
			
			// keyで既存リソースから検索または新規生成.
					
			// poolから検索.
			int res_id = -1;
			for(int i = 0; i < internal_resource_pool_.size(); ++i)
			{
				// 要求アクセスステージに対してアクセス期間が終わっていなければ再利用不可能.
				//	MEMO. 新規生成した実リソースの last_access_stage_ を負のstageで初期化しておくこと.
				if(internal_resource_pool_[i].last_access_stage_ >= require_access_stage)
					continue;
				
				// フォーマットチェック.
				if(internal_resource_pool_[i].tex_->GetFormat() != key.format)
					continue;
				// 要求サイズを格納できるならOK.
				if(internal_resource_pool_[i].tex_->GetWidth() < static_cast<uint32_t>(key.require_width_))
					continue;
				// 要求サイズを格納できるならOK.
				if(internal_resource_pool_[i].tex_->GetHeight() < static_cast<uint32_t>(key.require_height_))
					continue;
				// RTV要求している場合.
				if(key.usage_ & access_type_mask::RENDER_TARTGET)
				{
					if(!internal_resource_pool_[i].rtv_.IsValid())
						continue;
				}
				// DSV要求している場合.
				if(key.usage_ & access_type_mask::DEPTH_TARGET)
				{
					if(!internal_resource_pool_[i].dsv_.IsValid())
						continue;
				}
				// UAV要求している場合.
				if(key.usage_ & access_type_mask::UAV)
				{
					if(!internal_resource_pool_[i].uav_.IsValid())
						continue;
				}
				// SRV要求している場合.
				if(key.usage_ & access_type_mask::SHADER_READ)
				{
					if(!internal_resource_pool_[i].srv_.IsValid())
						continue;
				}

				// 再利用可能なリソース発見.
				res_id = i;
				break;
			}

			// 新規生成.
			if(0 > res_id)
			{
				rhi::TextureDep::Desc desc = {};
				rhi::ResourceState init_state = rhi::ResourceState::General;
				{
					desc.type = rhi::TextureType::Texture2D;// 現状2D固定.
					desc.initial_state = init_state;
					desc.array_size = 1;
					desc.mip_count = 1;
					desc.sample_count = 1;
					desc.heap_type = rhi::ResourceHeapType::Default;
						
					desc.format = key.format;
					desc.width = key.require_width_;	// MEMO 相対サイズの場合はここには縮小サイズ等が来てしまうので無駄がありそう.
					desc.height = key.require_height_;
					desc.depth = 1;
							
					desc.bind_flag = 0;
					{
						if(key.usage_ & access_type_mask::RENDER_TARTGET)
							desc.bind_flag |= rhi::ResourceBindFlag::RenderTarget;
						if(key.usage_ & access_type_mask::DEPTH_TARGET)
							desc.bind_flag |= rhi::ResourceBindFlag::DepthStencil;
						if(key.usage_ & access_type_mask::UAV)
							desc.bind_flag |= rhi::ResourceBindFlag::UnorderedAccess;
						if(key.usage_ & access_type_mask::SHADER_READ)
							desc.bind_flag |= rhi::ResourceBindFlag::ShaderResource;
					}
				}
				
				rhi::RefTextureDep new_tex = {};
				rhi::RefRtvDep new_rtv = {};
				rhi::RefDsvDep new_dsv = {};
				rhi::RefUavDep new_uav = {};
				rhi::RefSrvDep new_srv = {};

				// Texture.
				new_tex = new rhi::TextureDep();
				if (!new_tex->Initialize(p_device_, desc))
				{
					assert(false);
					return -1;
				}
				// Rtv.
				if(key.usage_ & access_type_mask::RENDER_TARTGET)
				{
					new_rtv = new rhi::RenderTargetViewDep();
					if (!new_rtv->Initialize(p_device_, new_tex.Get(), 0, 0, 1))
					{
						assert(false);
						return -1;
					}
				}
				// Dsv.
				if(key.usage_ & access_type_mask::DEPTH_TARGET)
				{
					new_dsv = new rhi::DepthStencilViewDep();
					if (!new_dsv->Initialize(p_device_, new_tex.Get(), 0, 0, 1))
					{
						assert(false);
						return -1;
					}
				}
				// Uav.
				if(key.usage_ & access_type_mask::UAV)
				{
					new_uav = new rhi::UnorderedAccessViewDep();
					if (!new_uav->Initialize(p_device_, new_tex.Get(), 0, 0, 1))
					{
						assert(false);
						return -1;
					}
				}
				// Srv.
				if(key.usage_ & access_type_mask::SHADER_READ)
				{
					new_srv = new rhi::ShaderResourceViewDep();
					if (!new_srv->InitializeAsTexture(p_device_, new_tex.Get(), 0, 1, 0, 1))
					{
						assert(false);
						return -1;
					}
				}

				InternalResourceInstanceInfo new_pool_elem = {};
				{
					// 新規生成した実リソースは最終アクセスステージを負の最大にしておく(ステージ0のリクエストに割当できるように).
					new_pool_elem.last_access_stage_ = TaskStage::k_frontmost_stage();
					
					new_pool_elem.tex_ = new_tex;
					new_pool_elem.rtv_ = new_rtv;
					new_pool_elem.dsv_ = new_dsv;
					new_pool_elem.uav_ = new_uav;
					new_pool_elem.srv_ = new_srv;
						
					new_pool_elem.cached_state_ = init_state;// 新規生成したらその初期ステートを保持.
					new_pool_elem.prev_cached_state_ = init_state;
				}
				res_id = static_cast<int>(internal_resource_pool_.size());// 新規要素ID.
				internal_resource_pool_.push_back(new_pool_elem);//登録.
			}

			// チェック
			if(p_access_stage_for_reuse)
			{
				// アクセス期間による再利用を有効にしている場合は, 最終アクセスステージリソースは必ず引数のアクセスステージよりも前のもののはず.
				assert(internal_resource_pool_[res_id].last_access_stage_ < (*p_access_stage_for_reuse));
			}

			return res_id;
		}
		
		//	フレーム開始通知. 内部リソースプールの中で一定フレームアクセスされていないものを破棄するなどの処理.
		void RenderTaskGraphManager::BeginFrame()
		{
			// TODO. 未実装!.
		}
		
		// タスクグラフを構築したbuilderをCompileしてリソース割当を確定する.
		// Compileしたbuilderは必ずExecuteする必要がある.
		// また, 複数のbuilderをCompileした場合はCompileした順序でExecuteが必要(確定したリソースの状態遷移コマンド実行を正しい順序で実行するために).
		bool RenderTaskGraphManager::Compile(RenderTaskGraphBuilder& builder)
		{
			assert(nullptr != p_device_);

			// Compile実行.
			const bool result = builder.Compile(*this);

			// Compile完了したのでシーケンス上でのリソース利用情報をクリアする.
			//	Stateは継続するので維持.
			for(auto& e : internal_resource_pool_)
			{
				e.last_access_stage_ = TaskStage::k_frontmost_stage();// 次のCompileのために最終アクセスを負の最大にリセット.
			}

			return result;
		}
		
		
	}
}
