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
			// node_sequence_内で自身より後ろのノードに参照されていないノードを終端ノードとしてリストアップ.
			// 
			// 終端ノードから遡って有効ノードをカリング.
			// 同時にレンダリングフローとしてのValidationチェック.

			// TODO.
			int i = node_sequence_.size() - 1;
			for (; 0 <= i; --i)
			{
				auto* p_node = node_sequence_[i];

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

	}
}
