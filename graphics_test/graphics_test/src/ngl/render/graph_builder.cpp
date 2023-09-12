#pragma once

#include "graph_builder.h"


namespace ngl
{

	// Render Task Graph 検証実装.
	namespace rtg
	{

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
		ResourceHandle RenderTaskGraphBuilder::RegisterResourceAccess(const ITaskNode& node, ResourceHandle res_handle, EACCESS_TYPE access_type)
		{
			// Node->Handle&AccessTypeのMap登録.
			{
				if(node_res_usage_map_.end() == node_res_usage_map_.find(&node))
				{
					node_res_usage_map_[&node] = {};
				}

				NodeResourceUsageInfo push_info = {};
				push_info.handle = res_handle;
				push_info.access = access_type;
				node_res_usage_map_[&node].push_back(push_info);
			}

			// Passメンバに保持するコードを短縮するためHandleをそのままリターン.
			return res_handle;
		}

		// グラフからリソース割当と状態遷移を確定.
		void RenderTaskGraphBuilder::Compile()
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
					assert(node_res_usage_map_.end() != node_res_usage_map_.find(p_node));// ありえない.

					// Nodeが同じHandleに対して重複したRegisterを指定ないかチェック.
					for(int i = 0; i < node_res_usage_map_[p_node].size() - 1; ++i)
					{
						for(int j = i + 1; j < node_res_usage_map_[p_node].size(); ++j)
						{
							if(node_res_usage_map_[p_node][i].handle == node_res_usage_map_[p_node][j].handle)
							{
								std::cout << u8"[RenderTaskGraphBuilder][Validation Error] 同一リソースへの重複アクセス登録." <<std::endl;
								assert(false);
							}
						}
					}
				}
			}

			
			// SequenceのNodeを順に処理して実リソースの割当とそのステート遷移確定をしていく.
			struct NodeResourceHandleAccessInfo
			{
				const ITaskNode*	p_node_ = {};
				EACCESS_TYPE		access_type = EACCESS_TYPE::INVALID;
			};
			// ResourceHandle -> このResourceHandleへの{Node, AccessType}のSequenceと同じ順序でのリスト.
			std::unordered_map<ResourceHandleDataType, std::vector<NodeResourceHandleAccessInfo>> res_access_flow = {};

			for(int node_i = 0; node_i < node_sequence_.size(); ++node_i)
			{
				const ITaskNode* p_node = node_sequence_[node_i];
				// このNodeのリソースアクセス情報を巡回.
				for(auto& res_access : node_res_usage_map_[p_node])
				{
					if(res_access_flow.end() == res_access_flow.find(res_access.handle))
					{
						// このResourceHandleの要素が未登録なら新規.
						res_access_flow[res_access.handle] = {};
					}
					// このResourceHandleへのNodeからのアクセスを順にリストアップ.
					NodeResourceHandleAccessInfo access_flow_part = {};
					access_flow_part.p_node_ = p_node;
					access_flow_part.access_type = res_access.access;
					res_access_flow[res_access.handle].push_back(access_flow_part);
				}
			}

			// デバッグ表示.
			{
				// 各ResourceHandleへの処理順でのアクセス情報.
				std::cout << "-Access Flow Debug" << std::endl;
				for(auto res_access_flow_elem : res_access_flow)
				{
					std::cout << "	-ResourceHandle ID " << res_access_flow_elem.first << std::endl;
					for(auto res_access : res_access_flow_elem.second)
					{
						std::cout << "		-Node " << res_access.p_node_->GetDebugNodeName().Get() << std::endl;
						std::cout << "			-AccessType " << static_cast<int>(res_access.access_type) << std::endl;
					}
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
	}
}
