#pragma once

#include <unordered_map>

#include "ngl/text/hash_text.h"

#include "ngl/rhi/d3d12/device.d3d12.h"
#include "ngl/rhi/d3d12/shader.d3d12.h"
#include "ngl/rhi/d3d12/command_list.d3d12.h"
#include "ngl/rhi/d3d12/resource_view.d3d12.h"

#include "ngl/resource/resource_manager.h"
#include "ngl/gfx/resource/resource_shader.h"


namespace ngl
{

	// Render Task Graph 検証実装.
	namespace rtg
	{
		using RtgNameType = text::HashCharPtr<64>;

		enum class ETASK_TYPE : int
		{
			GRAPHICS,
			COMPUTE,
			ALLOW_ASYNC_COMPUTE,
		};

		// リソースアクセス時のリソース解釈.
		// ひとまず用途のみ指定してそこから書き込みや読み取りなどは自明ということにする. 必要になったら情報追加するなど.
		enum class EACCESS_TYPE : int
		{
			RENDER_TARTGET,
			DEPTH_TARGET,
			SHADER_READ,
			UAV,
		};

		struct RenderTaskGraphBuilder;
		struct ResourceDesc2D
		{
			struct Desc
			{
				union
				{
					struct AbsSize
					{
						int w;
						int h;
					} abs_size;

					struct RelSize
					{
						float w;
						float h;
					} rel_size;
				};
				rhi::ResourceFormat format;
				bool is_relative;


				// サイズ直接指定. その他データはEmpty.
				static constexpr ResourceDesc2D CreateAsAbsoluteSize(int w, int h)
				{
					ResourceDesc2D v{};
					v.desc.is_relative = false;
					v.desc.abs_size = { w, h };
					return v;
				}
				// 相対サイズ指定. その他データはEmpty.
				static constexpr ResourceDesc2D CreateAsRelative(float w_rate, float h_rate)
				{
					ResourceDesc2D v{};
					v.desc.is_relative = true;
					v.desc.rel_size = { w_rate, h_rate };
					return v;
				}
			};

			// オブジェクトのHashKey用全域包括Storage.
			// unionでこのオブジェクトがResourceDesc2D全体を包括する.
			struct Storage
			{
				uint64_t a{};
				uint64_t b{};
			};


			// データ部.
			union
			{
				Storage storage{};// HashKey用.
				Desc	desc; // 実際のデータ用.
			};

			// サイズ直接指定.
			static constexpr ResourceDesc2D CreateAsAbsoluteSize(int w, int h, rhi::ResourceFormat format)
			{
				ResourceDesc2D v = Desc::CreateAsAbsoluteSize(w, h);

				v.desc.format = format;
				return v;
			}
			// 相対サイズ指定.
			static constexpr ResourceDesc2D CreateAsRelative(float w_rate, float h_rate, rhi::ResourceFormat format)
			{
				ResourceDesc2D v = Desc::CreateAsRelative(w_rate, h_rate);

				v.desc.format = format;
				return v;
			}
		};
		// StorageをHashKey扱いするためStorageがオブジェクト全体を包括するサイズである必要がある.
		static_assert(sizeof(ResourceDesc2D) == sizeof(ResourceDesc2D::Storage));
		static constexpr auto sizeof_ResourceDesc2D_Desc = sizeof(ResourceDesc2D::Desc);
		static constexpr auto sizeof_ResourceDesc2D_Storage = sizeof(ResourceDesc2D::Storage);
		static constexpr auto sizeof_ResourceDesc2D = sizeof(ResourceDesc2D);

		// RTGのノードが利用するリソースハンドル.
		// 識別IDやSwapchain識別等の情報を保持.
		using ResourceHandleDataType = uint64_t;
		struct ResourceHandle
		{
			union
			{
				// (u64)0は特殊IDで無効扱い. unique_idが0でもswapchainビットが1であれば有効.
				ResourceHandleDataType data = 0;
				struct detail
				{
					uint32_t unique_id;

					uint32_t is_swapchain : 1;
					uint32_t dummy : 31;
				}detail;
			};

			static constexpr ResourceHandle InvalidHandle()
			{
				return ResourceHandle({});
			}
			bool IsInvalid() const
			{
				return detail.unique_id == InvalidHandle().detail.unique_id;
			}

			operator ResourceHandleDataType() const
			{
				return data;
			}
		};
		static constexpr auto sizeof_ResourceHandle = sizeof(ResourceHandle);


		/*
		// 生成はRenderTaskGraphBuilder経由.
		// 派生クラスではメンバハンドルや基本情報の簡易定義のために専用のマクロを利用する.

			struct TaskSamplePass : public rtg::ITaskNode
			{
				// ノード定義コンストラクタ記述マクロ.
				ITASK_NODE_DEF_BEGIN(TaskSamplePass)
					ITASK_NODE_HANDLE_REGISTER(h_depth_)
				ITASK_NODE_DEF_END

				rtg::ResourceHandle h_depth_{};// メンバハンドル.
			}
		*/
		struct ITaskNode
		{
			virtual ETASK_TYPE TaskType() const
			{
				return ETASK_TYPE::GRAPHICS;
			}


			virtual void Run(RenderTaskGraphBuilder& builder)
			{
			}

			struct RefHandle
			{
				RtgNameType name{};
				ResourceHandle* p_handle{};
			};

			void RegisterSelfHandle(const char* name, ResourceHandle& handle)
			{
				RefHandle elem = { name , &handle};
				ref_handle_.push_back(elem);
			}
			
			std::vector<RefHandle> ref_handle_{};

		protected:
			void SetDebugNodeName(const char* name){ debug_node_name_ = name; }
			RtgNameType debug_node_name_{};
		};

// -------------------------------------------------------------
		// Nodeクラスのコンストラクタ定義開始マクロ. デバッグ名登録等も自動化.
#		define ITASK_NODE_DEF_BEGIN(CLASS_NAME) CLASS_NAME()\
				{\
					SetDebugNodeName( #CLASS_NAME );
// -------------------------------------------------------------

// -------------------------------------------------------------
		// Nodeのメンバハンドル定義. 
		// ITASK_NODE_DEF_BEGIN と ITASK_NODE_DEF_END の間に記述.
		// マクロで登録処理などを隠蔽.
#define ITASK_NODE_HANDLE_REGISTER(name)\
					{\
							RefHandle elem = { #name , &name};\
							ref_handle_.push_back(elem);\
					};
// -------------------------------------------------------------

// -------------------------------------------------------------
		// Nodeクラスのコンストラクタ定義終端マクロ.
#define ITASK_NODE_DEF_END\
				};
// -------------------------------------------------------------



		// Taskノードのリソースアロケーションやノード間リソース状態遷移を計算する.
		// GPU実行順はCreateされたTaskノードの順序.
		//  
		struct RenderTaskGraphBuilder
		{
			struct ResourceAccessInfo
			{
				const ITaskNode*		p_node{};
				EACCESS_TYPE			access{};
			};

			// ITaskNode派生クラスをシーケンスの末尾に新規生成する.
			template<typename TPassNode>
			TPassNode* CreateNewNodeInSequenceTail()
			{
				auto new_node = new TPassNode();
				node_sequence_.push_back(new_node);
				return new_node;
			}

			// リソースハンドルを生成.
			ResourceHandle CreateResource(ResourceDesc2D res_desc);
			// Swapchainリソースハンドルを取得.
			ResourceHandle GetSwapchainResourceHandle();

			// Nodeからのリソースアクセスを記録.
			ResourceHandle RegisterResourceAccess(const ITaskNode& node, ResourceHandle res_handle, EACCESS_TYPE access_type);

			// グラフからリソース割当と状態遷移を確定.
			void Compile();


			// -------------------------------------------------------------------------------------------

			~RenderTaskGraphBuilder();

			// -------------------------------------------------------------------------------------------

			std::vector<ITaskNode*> node_sequence_{};// Graph構成ノードシーケンス. 生成順がGPU実行順で, AsyncComputeもFenceで同期をする以外は同様.
			std::unordered_map<ResourceHandleDataType, ResourceDesc2D> res_desc_map_{};// リソースユニークIDからその定義のMap.
			std::unordered_map<ResourceHandleDataType, std::vector<ResourceAccessInfo>> res_access_map_{};// ResourceHandleからそのリソースへのノードアクセス情報.

			uint32_t s_res_handle_id_counter_{};// 生成リソースユニークID.
		};
	}
}


namespace ngl::render
{
	using GraphResouceNameText = text::HashCharPtr<64>;

	// 簡易リソース登録クラス.
	//	将来的にはRenderGraphの構築とリソース割り当て, 自動ステート遷移などをしたい.
	//	現状はメイン側で必要なリソースを全部登録してPass側で直接引いている.
	class GraphBuilder
	{

	public:
		GraphBuilder(rhi::DeviceDep* p_device)
		{
			p_device_ = p_device;
		}

		void AddFrameResource(const GraphResouceNameText& name, rhi::RefCbvDep v)
		{
			frame_cbv_map_[name] = v;
		}
		void AddFrameResource(const GraphResouceNameText& name, rhi::RefSrvDep v)
		{
			frame_srv_map_[name] = v;
		}
		void AddFrameResource(const GraphResouceNameText& name, rhi::RefUavDep v)
		{
			frame_uav_map_[name] = v;
		}
		void AddFrameResource(const GraphResouceNameText& name, rhi::RefSampDep v)
		{
			frame_samp_map_[name] = v;
		}

		void AddFrameResource(const GraphResouceNameText& name, rhi::RefRtvDep v)
		{
			frame_rtv_map_[name] = v;
		}
		void AddFrameResource(const GraphResouceNameText& name, rhi::RefDsvDep v)
		{
			frame_dsv_map_[name] = v;
		}

		void AddFrameResource(const GraphResouceNameText& name, rhi::RefTextureDep v)
		{
			frame_tex_map_[name] = v;
		}


		rhi::RefCbvDep GetFrameCbv(const GraphResouceNameText& name)
		{
			if (frame_cbv_map_.end() == frame_cbv_map_.find(name))
				return {};
			return frame_cbv_map_[name];
		}
		rhi::RefSrvDep GetFrameSrv(const GraphResouceNameText& name)
		{
			if (frame_srv_map_.end() == frame_srv_map_.find(name))
				return {};
			return frame_srv_map_[name];
		}
		rhi::RefUavDep GetFrameUav(const GraphResouceNameText& name)
		{
			if (frame_uav_map_.end() == frame_uav_map_.find(name))
				return {};
			return frame_uav_map_[name];
		}
		rhi::RefSampDep GetFrameSamp(const GraphResouceNameText& name)
		{
			if (frame_samp_map_.end() == frame_samp_map_.find(name))
				return {};
			return frame_samp_map_[name];
		}

		rhi::RefRtvDep GetFrameRtv(const GraphResouceNameText& name)
		{
			if (frame_rtv_map_.end() == frame_rtv_map_.find(name))
				return {};
			return frame_rtv_map_[name];
		}
		rhi::RefDsvDep GetFrameDsv(const GraphResouceNameText& name)
		{
			if (frame_dsv_map_.end() == frame_dsv_map_.find(name))
				return {};
			return frame_dsv_map_[name];
		}

		rhi::RefTextureDep GetFrameTexture(const GraphResouceNameText& name)
		{
			if (frame_tex_map_.end() == frame_tex_map_.find(name))
				return {};
			return frame_tex_map_[name];
		}

	public:
		void SetSwapchain(rhi::RhiRef<rhi::SwapChainDep> v)
		{
			swapchain_ = v;
		}
		rhi::RhiRef<rhi::SwapChainDep> GetSwapchain()
		{
			return swapchain_;
		}

	public:
		rhi::DeviceDep* GetDevice() { return p_device_; }
	private:
		rhi::DeviceDep* p_device_ = {};

		std::unordered_map<GraphResouceNameText, rhi::RefCbvDep>	frame_cbv_map_;
		std::unordered_map<GraphResouceNameText, rhi::RefSrvDep>	frame_srv_map_;
		std::unordered_map<GraphResouceNameText, rhi::RefUavDep>	frame_uav_map_;
		std::unordered_map<GraphResouceNameText, rhi::RefSampDep>	frame_samp_map_;

		std::unordered_map<GraphResouceNameText, rhi::RefRtvDep>	frame_rtv_map_;
		std::unordered_map<GraphResouceNameText, rhi::RefDsvDep>	frame_dsv_map_;

		std::unordered_map<GraphResouceNameText, rhi::RefTextureDep>	frame_tex_map_;


		rhi::RhiRef<rhi::SwapChainDep>	swapchain_;

	};
}