#pragma once

#include "graph_builder.h"

#include "ngl/rhi/d3d12/command_list.d3d12.h"


namespace ngl
{

	// Render Task Graph 検証実装.
	namespace rtg
	{

		
		// オペレータ.
		constexpr bool RenderTaskGraphBuilder::TaskStage::operator<(const TaskStage arg) const
		{
			if(stage_ < arg.stage_)
				return true;
			else if(stage_ == arg.stage_)
				return step_ < arg.step_;
			return false;
		}
		constexpr bool RenderTaskGraphBuilder::TaskStage::operator>(const TaskStage arg) const
		{
			if(stage_ > arg.stage_)
				return true;
			else if(stage_ == arg.stage_)
				return step_ > arg.step_;
			return false;
		}
		constexpr bool RenderTaskGraphBuilder::TaskStage::operator<=(const TaskStage arg) const
		{
			if(stage_ < arg.stage_)
				return true;
			else if(stage_ == arg.stage_)
				return step_ <= arg.step_;
			return false;
		}
		constexpr bool RenderTaskGraphBuilder::TaskStage::operator>=(const TaskStage arg) const
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
			++s_res_handle_id_counter_;
			if (0 == s_res_handle_id_counter_)
				++s_res_handle_id_counter_;// 0は無効ID扱いのためスキップ.


			ResourceHandle handle{};

			handle.detail.unique_id = s_res_handle_id_counter_;// ユニークID割当.

			res_desc_map_[handle.data] = res_desc;// desc記録.

			return handle;
		}
		// Swapchainリソースハンドルを取得.
		ResourceHandle RenderTaskGraphBuilder::GetSwapchainResourceHandle()
		{
			ResourceHandle handle{};

			handle.detail.is_swapchain = 1;// swapchain.

			return handle;
		}

		// Nodeからのリソースアクセスを記録.
		// NodeのRender実行順と一致する順序で登録をする必要がある. この順序によってリソースステート遷移の確定や実リソースの割当等をする.
		ResourceHandle RenderTaskGraphBuilder::RegisterResourceAccess(const ITaskNode& node, ResourceHandle res_handle, ACCESS_TYPE access_type)
		{
			// Node->Handle&AccessTypeのMap登録.
			{
				if(node_handle_usage_map_.end() == node_handle_usage_map_.find(&node))
				{
					node_handle_usage_map_[&node] = {};
				}

				NodeHandleUsageInfo push_info = {};
				push_info.handle = res_handle;
				push_info.access = access_type;
				node_handle_usage_map_[&node].push_back(push_info);
			}

			// Passメンバに保持するコードを短縮するためHandleをそのままリターン.
			return res_handle;
		}


		// Poolからリソース検索または新規生成.
		int RenderTaskGraphBuilder::GetOrCreateResourceFromPool(rhi::DeviceDep& device, ResourceSearchKey key, const TaskStage* p_access_stage_for_reuse)
		{
			constexpr TaskStage k_firstest_stage = {std::numeric_limits<int>::min(), std::numeric_limits<int>::min()};
			
			//	アクセスステージが引数で渡されなかった場合は未割り当てリソース以外を割り当てないようにk_firstest_stage扱いとする.
			const TaskStage require_access_stage = (p_access_stage_for_reuse)? ((*p_access_stage_for_reuse)) : k_firstest_stage;
			
			// keyで既存リソースから検索または新規生成.
					
			// poolから検索.
			int res_id = -1;
			for(int i = 0; i < tex_instance_pool_.size(); ++i)
			{
				// 要求アクセスステージに対してアクセス期間が終わっていなければ再利用不可能.
				//	MEMO. 新規生成した実リソースの last_access_stage_ を負のstageで初期化しておくこと.
				if(tex_instance_pool_[i].last_access_stage_ >= require_access_stage)
					continue;
				
				// フォーマットチェック.
				if(tex_instance_pool_[i].tex_->GetFormat() != key.format)
					continue;
				// 要求サイズを格納できるならOK.
				if(tex_instance_pool_[i].tex_->GetWidth() < static_cast<uint32_t>(key.require_width_))
					continue;
				// 要求サイズを格納できるならOK.
				if(tex_instance_pool_[i].tex_->GetHeight() < static_cast<uint32_t>(key.require_height_))
					continue;
				// RTV要求している場合.
				if(key.usage_ & access_type_mask::RENDER_TARTGET)
				{
					if(!tex_instance_pool_[i].rtv_.IsValid())
						continue;
				}
				// DSV要求している場合.
				if(key.usage_ & access_type_mask::DEPTH_TARGET)
				{
					if(!tex_instance_pool_[i].dsv_.IsValid())
						continue;
				}
				// UAV要求している場合.
				if(key.usage_ & access_type_mask::UAV)
				{
					if(!tex_instance_pool_[i].uav_.IsValid())
						continue;
				}
				// SRV要求している場合.
				if(key.usage_ & access_type_mask::SHADER_READ)
				{
					if(!tex_instance_pool_[i].srv_.IsValid())
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
				if (!new_tex->Initialize(&device, desc))
				{
					assert(false);
					return -1;
				}
				// Rtv.
				if(key.usage_ & access_type_mask::RENDER_TARTGET)
				{
					new_rtv = new rhi::RenderTargetViewDep();
					if (!new_rtv->Initialize(&device, new_tex.Get(), 0, 0, 1))
					{
						assert(false);
						return -1;
					}
				}
				// Dsv.
				if(key.usage_ & access_type_mask::DEPTH_TARGET)
				{
					new_dsv = new rhi::DepthStencilViewDep();
					if (!new_dsv->Initialize(&device, new_tex.Get(), 0, 0, 1))
					{
						assert(false);
						return -1;
					}
				}
				// Uav.
				if(key.usage_ & access_type_mask::UAV)
				{
					new_uav = new rhi::UnorderedAccessViewDep();
					if (!new_uav->Initialize(&device, new_tex.Get(), 0, 0, 1))
					{
						assert(false);
						return -1;
					}
				}
				// Srv.
				if(key.usage_ & access_type_mask::SHADER_READ)
				{
					new_srv = new rhi::ShaderResourceViewDep();
					if (!new_srv->InitializeAsTexture(&device, new_tex.Get(), 0, 1, 0, 1))
					{
						assert(false);
						return -1;
					}
				}

				TextureInstancePoolElement new_pool_elem = {};
				{
					// 新規生成した実リソースは最終アクセスステージを負の最大にしておく(ステージ0のリクエストに割当できるように).
					new_pool_elem.last_access_stage_ = k_firstest_stage;
					
					new_pool_elem.tex_ = new_tex;
					new_pool_elem.rtv_ = new_rtv;
					new_pool_elem.dsv_ = new_dsv;
					new_pool_elem.uav_ = new_uav;
					new_pool_elem.srv_ = new_srv;
						
					new_pool_elem.global_begin_state_ = init_state;
					new_pool_elem.global_end_state_ = init_state;
				}
				res_id = static_cast<int>(tex_instance_pool_.size());// 新規要素ID.
				tex_instance_pool_.push_back(new_pool_elem);//登録.
			}

			// チェック
			if(p_access_stage_for_reuse)
			{
				// アクセス期間による再利用を有効にしている場合は, 最終アクセスステージリソースは必ず引数のアクセスステージよりも前のもののはず.
				assert(tex_instance_pool_[res_id].last_access_stage_ < (*p_access_stage_for_reuse));
			}

			return res_id;
		}
		

		// グラフからリソース割当と状態遷移を確定.
		bool RenderTaskGraphBuilder::Compile(rhi::DeviceDep& device)
		{
			// MEMO 終端に寄与しないノードのカリングは保留.
			
			// node_sequence_内で自身より後ろのノードに参照されていないノードを終端ノードとしてリストアップ.
			// 終端ノードから遡って有効ノードをカリング.
			// 同時にレンダリングフローとしてのValidationチェック.
			/*
			std::vector<ITaskNode*> goal_nodes{};
			// TODO.
			for (auto ni = 0; ni < node_sequence_.size(); ++ni)
			{
				auto* p_node = node_sequence_[ni];

				// 自身よりも後ろのNodeで参照されているか.
				bool exist_access_after = false;

				// Nodeのハンドルを検査.
				for (const auto& ref_h : p_node->ref_handle_array_)
				{
					// ハンドルへのアクセス情報を引き出し.
					assert(res_access_map_.end() != res_access_map_.find(*ref_h.p_handle));
					const auto& handle_access_list = res_access_map_[*ref_h.p_handle];


					// handle_access_list[]のアクセス元ノードに, node_sequence_[]のni+1以降のノードがあれば exist_access_after=true.
					for (auto& ha : handle_access_list)
					{
						// 後ろかどうかはシーケンス上のインデックスから決定.
						int node_pos = GetNodeSequencePosition(ha.p_node);
						if (ni < node_pos)
						{
							exist_access_after = true;
							break;
						}
					}
				}

				if (!exist_access_after)
					goal_nodes.push_back(p_node);
			}

			// 終端Nodeのリスト.
			for (auto& n : goal_nodes)
			{
				std::cout << n->GetDebugNodeName().Get() << std::endl;
			}
			*/

			
			// 念のためのValidation Check.
			{
				for(int sequence_id = 0; sequence_id < node_sequence_.size(); ++sequence_id)
				{
					ITaskNode* p_node = node_sequence_[sequence_id];
					assert(node_handle_usage_map_.end() != node_handle_usage_map_.find(p_node));// ありえない.

					// Nodeが同じHandleに対して重複したRegisterを指定ないかチェック.
					for(int i = 0; i < node_handle_usage_map_[p_node].size() - 1; ++i)
					{
						for(int j = i + 1; j < node_handle_usage_map_[p_node].size(); ++j)
						{
							if(node_handle_usage_map_[p_node][i].handle == node_handle_usage_map_[p_node][j].handle)
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
			std::unordered_map<ResourceHandleDataType, int> handle_index_map = {};
			int handle_count = 0;
			for(int node_i = 0; node_i < node_sequence_.size(); ++node_i)
			{
				const ITaskNode* p_node = node_sequence_[node_i];
				// このNodeのリソースアクセス情報を巡回.
				for(auto& res_access : node_handle_usage_map_[p_node])
				{
					if(handle_index_map.end() == handle_index_map.find(res_access.handle))
					{
						// このResourceHandleの要素が未登録なら新規.
						handle_index_map[res_access.handle] = handle_count;
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
				for(auto& res_access : node_handle_usage_map_[p_node])
				{
					const auto handle_index = handle_index_map[res_access.handle];
					
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
			

			// TODO. 外部依存ハンドルのアクセス期間の補正.
			//	外部リソースの開始アクセスをシーケンス先頭に補正したり, 外部出力リソースの終端アクセスをシーケンス末尾に補正するといった処理をする.
			//	ヒストリリソースの解決などをどうするか.
			

			
			// リソースハンドル毎にPoolから実リソースを割り当てる.
			// ハンドルのアクセス期間を元に実リソースの再利用も可能.
			std::vector<int> handle_res_id_array(handle_count, -1);// 無効値-1初期化.
			for(auto handle_index : handle_index_map)
			{
				const ResourceHandle res_handle = ResourceHandle(handle_index.first);
				const auto handle_id = handle_index.second;
				
				// SwapChainは外部供給である点に注意. その他外部リソース登録等も考慮が必要.
				
				// ユニーク割当IDでまだ未割り当ての場合は新規割当.
				if(0 > handle_res_id_array[handle_id])
				{
					// 実際はここでPool等から実リソースを割り当て, 以前のステートを引き継いで遷移を確定させる.
					// 理想的には unique_id は違うが寿命がオーバーラップしていない再利用可能実リソースを使い回す.

					const ResourceHandleAccessInfo& handle_access = handle_access_info_array[handle_id];

					if(!res_handle.detail.is_swapchain)
					{
						// Swapchainではない通常リソース.
						
						const auto require_desc = res_desc_map_[res_handle];

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
						// このハンドルの開始アクセスステージで再利用可能なものを検索する.
						const TaskStage first_stage = handle_life_first_array[handle_id];
						const TaskStage* p_request_access_stage = &first_stage;
#else
						// テスト用. 再利用せずに未割り当てのみ許可する場合.
						const TaskStage* p_request_access_stage = nullptr;
#endif

						// 割当可能なリソース検索または新規生成.
						int res_id = GetOrCreateResourceFromPool(device, search_key, p_request_access_stage);
						assert(0 <= res_id);// 必ず有効なIDが帰るはず.

						// 割当決定したリソースの最終アクセスステージを更新 (このハンドルの最終アクセスステージ).
						tex_instance_pool_[res_id].last_access_stage_ = handle_life_last_array[handle_id];

						// ハンドルから実リソースを引けるように登録.
						handle_res_id_array[handle_id] = res_id;
					}
					else
					{
						// Swapchainの場合.
						// 一旦無効にしておく.
						handle_res_id_array[handle_id] = -1;
					}
				}
			}

			// Graph上で割り当てられた有効なリソースIDから密なリニアインデックスへのマップ. 有効リソースID毎の情報をワークバッファ上で操作するため.
			std::unordered_map<int, int> res_id_2_linear_map = {};
			// 有効リソースリニアインデックスからリソースIDへのマッピングをする配列.
			std::vector<int> res_linear_2_id_array = {};
			// Graph上で有効な実リソース数.
			int valid_res_count = 0;
			for(const auto& res_id : handle_res_id_array)
			{
				if(res_id_2_linear_map.end() == res_id_2_linear_map.find(res_id))
				{
					res_id_2_linear_map[res_id] = valid_res_count;
					res_linear_2_id_array.push_back(res_id);
					++valid_res_count;
				}
			}

			// TODO.
			//	各Node時点でのリソースステートを計算.
			//
			std::vector<std::vector<const ITaskNode*>> res_access_node_array(valid_res_count);
			for(const auto* p_node : node_sequence_)
			{
				const auto& node_handles = node_handle_usage_map_[p_node];
				for(const auto& access_info : node_handles)
				{
					// access_info.access;
					const auto handle_id = handle_index_map[access_info.handle];
					const auto res_id = handle_res_id_array[handle_id];

					const auto res_index = res_id_2_linear_map[res_id];

					// リソースに対して時系列でのアクセスNodeリスト.
					res_access_node_array[res_index].push_back(p_node);
				}
			}

			struct NodeHandleState
			{
				rhi::ResourceState prev_ = {};
				rhi::ResourceState next_ = {};
			};
			// 各Nodeの各Handleがその時点でどのようにステート遷移すべきかの情報. 多段Map.
			std::unordered_map<const ITaskNode*, std::unordered_map<ResourceHandleDataType, NodeHandleState>> node_handle_state_ = {};
			for(int res_index = 0; res_index < res_access_node_array.size(); ++res_index)
			{
				const int res_id = res_linear_2_id_array[res_index];
				rhi::ResourceState begin_state = {};
				if(0 <= res_id)
				{
					begin_state = tex_instance_pool_[res_id].global_begin_state_;
				}
				else
				{
					// TODO. ここはSwapchain等の外部リソース依存の箇所なので登録時に一緒に指定された開始ステートとなるはず.
					begin_state = rhi::ResourceState::General;
				}
				
				rhi::ResourceState cur_state = begin_state;
				for(const auto* p_node : res_access_node_array[res_index])
				{
					for(const auto handle : node_handle_usage_map_[p_node])
					{
						const int handle_index = handle_index_map[handle.handle];
						if(res_id == handle_res_id_array[handle_index])
						{
							// TODO
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
							// TODO.
							NodeHandleState node_handle_state = {};
							{
								node_handle_state.prev_ = cur_state;
								node_handle_state.next_ = next_state;
							}
							node_handle_state_[p_node][handle.handle] = node_handle_state;
							
							// 更新.
							cur_state = next_state;
							
							break;
						}
					}
				}

				// 最終ステートを保存.
				if(0 <= res_id)
				{
					tex_instance_pool_[res_id].global_end_state_ = cur_state;
				}
				else
				{
					// TODO. ここはSwapchain等の外部リソース依存の箇所なので適切な変数に保存する.
				}
				
			}
			
			
			// デバッグ表示.
			{
				// 各ResourceHandleへの処理順でのアクセス情報.
				std::cout << "-Access Flow Debug" << std::endl;
				for(auto handle_index : handle_index_map)
				{
					const auto handle_id = handle_index.second;
					
					const auto& lifetime_first = handle_life_first_array[handle_id];
					const auto& lifetime_last = handle_life_last_array[handle_id];

					const auto res_id = handle_res_id_array[handle_id];
					const auto res = (0 <= res_id)? tex_instance_pool_[res_id] : TextureInstancePoolElement();
						
					std::cout << "	-ResourceHandle ID " << handle_index.first << std::endl;
					std::cout << "		-FirstAccess " << static_cast<int>(lifetime_first.step_) << "/" << static_cast<int>(lifetime_first.stage_) << std::endl;
					std::cout << "		-LastAccess " << static_cast<int>(lifetime_last.step_) << "/" << static_cast<int>(lifetime_last.stage_) << std::endl;

					std::cout << "		-Resource" << std::endl;
					std::cout << "			-id " << res_id << std::endl;
					if(res.tex_.IsValid())
						std::cout << "			-ptr " << res.tex_.Get() << std::endl;
					else
						std::cout << "			-ptr " << nullptr << std::endl;// Swapchain等の外部リソース.
					
					for(auto res_access : handle_access_info_array[handle_id].from_node_)
					{
						std::cout << "		-Node " << res_access.p_node_->GetDebugNodeName().Get() << std::endl;
						std::cout << "			-AccessType " << static_cast<int>(res_access.access_type) << std::endl;
					}
				}
			}
			
			return true;
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
	}
}
