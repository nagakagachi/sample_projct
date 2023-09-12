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
		ResourceHandle RenderTaskGraphBuilder::RegisterResourceAccess(const ITaskNode& node, ResourceHandle res_handle, EACCESS_TYPE access_type)
		{
			if (res_access_map_.end() == res_access_map_.find(res_handle))
			{
				res_access_map_[res_handle] = {};
			}

			ResourceAccessInfo push_info{};
			push_info.p_node = &node;
			push_info.access = access_type;
			res_access_map_[res_handle].push_back(push_info);

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


			node_sequence_;
			
			
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
