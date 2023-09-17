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
		using ACCESS_TYPE = int;
		struct access_type
		{
			static constexpr ACCESS_TYPE INVALID		= {0};
			
			static constexpr ACCESS_TYPE RENDER_TARTGET	= {1};
			static constexpr ACCESS_TYPE DEPTH_TARGET	= {2};
			static constexpr ACCESS_TYPE SHADER_READ	= 3;
			static constexpr ACCESS_TYPE UAV			= 4;
			
			static constexpr ACCESS_TYPE _MAX			= 5;
		};
		using ACCESS_TYPE_MASK = int;
		struct access_type_mask
		{
			static constexpr ACCESS_TYPE_MASK RENDER_TARTGET	= 1 << (access_type::RENDER_TARTGET);
			static constexpr ACCESS_TYPE_MASK DEPTH_TARGET		= 1 << (access_type::DEPTH_TARGET);
			static constexpr ACCESS_TYPE_MASK SHADER_READ		= 1 << (access_type::SHADER_READ);
			static constexpr ACCESS_TYPE_MASK UAV				= 1 << (access_type::UAV);
		};
		

		struct RenderTaskGraphBuilder;
		// Passが必要とするリソースの定義.
		struct ResourceDesc2D
		{
			struct Desc
			{
				union
				{
					struct AbsSize
					{
						int w;// 要求するバッファのWidth (例 1920).
						int h;// 要求するバッファのHeight (例 1080).
					} abs_size;

					struct RelSize
					{
						float w;// 要求するバッファの基準Widthに対する相対スケール値(例 半解像度 0.5).
						float h;// 要求するバッファの基準Heightに対する相対スケール値(例 半解像度 0.5).
					} rel_size;
				};
				rhi::ResourceFormat format {};
				bool is_relative {};


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

			// 具体的なサイズ(Width, Height)を計算して返す.
			void GetConcreteTextureSize(int work_width, int work_height, int& out_width, int& out_height) const
			{
				if(desc.is_relative)
				{
					// 相対サイズ解決.
					out_width = static_cast<int>(work_width * desc.rel_size.w);
					out_height = static_cast<int>(work_height * desc.rel_size.h);
				}
				else
				{
					out_width = desc.abs_size.w;
					out_height = desc.abs_size.h;
				}
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

			constexpr ResourceHandle() = default;
			constexpr ResourceHandle(ResourceHandleDataType handle_data)
			{
				this->data = handle_data;
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
			virtual ~ITaskNode() = default;
			
			virtual ETASK_TYPE TaskType() const
			{
				return ETASK_TYPE::GRAPHICS;// 基底はGraphics.
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
				ref_handle_array_.push_back(elem);
			}
			
			std::vector<RefHandle> ref_handle_array_{};

		public:
			const RtgNameType& GetDebugNodeName() const { return debug_node_name_; }
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
							ref_handle_array_.push_back(elem);\
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
				ACCESS_TYPE				access{};
			};
			
			struct NodeResourceUsageInfo
			{
				ResourceHandle			handle{};
				ACCESS_TYPE				access{};
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
			// NodeのRender実行順と一致する順序で登録をする必要がある. この順序によってリソースステート遷移の確定や実リソースの割当等をする.
			ResourceHandle RegisterResourceAccess(const ITaskNode& node, ResourceHandle res_handle, ACCESS_TYPE access_type);

			// グラフからリソース割当と状態遷移を確定.
			bool Compile(rhi::DeviceDep& device);


			// -------------------------------------------------------------------------------------------

			~RenderTaskGraphBuilder();

			// -------------------------------------------------------------------------------------------
			static constexpr  int k_base_height = 1080;
			int res_base_height_ = k_base_height;
			int res_base_width_ = static_cast<int>( static_cast<float>(k_base_height) * 16.0f/9.0f);
			
			std::vector<ITaskNode*> node_sequence_{};// Graph構成ノードシーケンス. 生成順がGPU実行順で, AsyncComputeもFenceで同期をする以外は同様.
			std::unordered_map<ResourceHandleDataType, ResourceDesc2D> res_desc_map_{};// リソースユニークIDからその定義のMap.
			
			std::unordered_map<const ITaskNode*, std::vector<NodeResourceUsageInfo>> node_res_usage_map_{};// Node毎のResourceHandleアクセス情報をまとめるMap.

			struct TextureInstancePoolElement
			{
				bool lending_		= false;// 貸出中の場合true.
				rhi::ResourceState	global_begin_state_ = rhi::ResourceState::General;// Graph開始時点のステート.
				rhi::ResourceState	global_end_state_ = rhi::ResourceState::General;// Graph終了時点のステート.
				
				rhi::RefTextureDep	tex_ = {};
				rhi::RefRtvDep		rtv_ = {};
				rhi::RefDsvDep		dsv_ = {};
				rhi::RefUavDep		uav_ = {};
				rhi::RefSrvDep		srv_ = {};
			};
			std::vector<TextureInstancePoolElement> tex_instance_pool_ = {};
			
			uint32_t s_res_handle_id_counter_{};// リソースハンドルユニークID.
		private:
			// Sequence上でのノードの位置を返す.
			int GetNodeSequencePosition(const ITaskNode* p_node) const;
			
			// ------------------------------------------
			// 実リソースの割当.
			struct ResourceSearchKey
			{
				rhi::ResourceFormat format = {};
				int require_width_ = {};
				int require_height_ = {};
				ACCESS_TYPE_MASK	usage_ = {};// 要求する RenderTarget, DepthStencil, UAV等の用途.
			};
			// Poolからリソース検索または新規生成.
			int GetOrCreateResourceFromPool(rhi::DeviceDep& device, ResourceSearchKey key);
		};

		
	}
}


namespace ngl::render
{
	using GraphResourceNameText = text::HashCharPtr<64>;

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

		void AddFrameResource(const GraphResourceNameText& name, rhi::RefCbvDep v)
		{
			frame_cbv_map_[name] = v;
		}
		void AddFrameResource(const GraphResourceNameText& name, rhi::RefSrvDep v)
		{
			frame_srv_map_[name] = v;
		}
		void AddFrameResource(const GraphResourceNameText& name, rhi::RefUavDep v)
		{
			frame_uav_map_[name] = v;
		}
		void AddFrameResource(const GraphResourceNameText& name, rhi::RefSampDep v)
		{
			frame_samp_map_[name] = v;
		}

		void AddFrameResource(const GraphResourceNameText& name, rhi::RefRtvDep v)
		{
			frame_rtv_map_[name] = v;
		}
		void AddFrameResource(const GraphResourceNameText& name, rhi::RefDsvDep v)
		{
			frame_dsv_map_[name] = v;
		}

		void AddFrameResource(const GraphResourceNameText& name, rhi::RefTextureDep v)
		{
			frame_tex_map_[name] = v;
		}


		rhi::RefCbvDep GetFrameCbv(const GraphResourceNameText& name)
		{
			if (frame_cbv_map_.end() == frame_cbv_map_.find(name))
				return {};
			return frame_cbv_map_[name];
		}
		rhi::RefSrvDep GetFrameSrv(const GraphResourceNameText& name)
		{
			if (frame_srv_map_.end() == frame_srv_map_.find(name))
				return {};
			return frame_srv_map_[name];
		}
		rhi::RefUavDep GetFrameUav(const GraphResourceNameText& name)
		{
			if (frame_uav_map_.end() == frame_uav_map_.find(name))
				return {};
			return frame_uav_map_[name];
		}
		rhi::RefSampDep GetFrameSamp(const GraphResourceNameText& name)
		{
			if (frame_samp_map_.end() == frame_samp_map_.find(name))
				return {};
			return frame_samp_map_[name];
		}

		rhi::RefRtvDep GetFrameRtv(const GraphResourceNameText& name)
		{
			if (frame_rtv_map_.end() == frame_rtv_map_.find(name))
				return {};
			return frame_rtv_map_[name];
		}
		rhi::RefDsvDep GetFrameDsv(const GraphResourceNameText& name)
		{
			if (frame_dsv_map_.end() == frame_dsv_map_.find(name))
				return {};
			return frame_dsv_map_[name];
		}

		rhi::RefTextureDep GetFrameTexture(const GraphResourceNameText& name)
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

		std::unordered_map<GraphResourceNameText, rhi::RefCbvDep>	frame_cbv_map_;
		std::unordered_map<GraphResourceNameText, rhi::RefSrvDep>	frame_srv_map_;
		std::unordered_map<GraphResourceNameText, rhi::RefUavDep>	frame_uav_map_;
		std::unordered_map<GraphResourceNameText, rhi::RefSampDep>	frame_samp_map_;

		std::unordered_map<GraphResourceNameText, rhi::RefRtvDep>	frame_rtv_map_;
		std::unordered_map<GraphResourceNameText, rhi::RefDsvDep>	frame_dsv_map_;

		std::unordered_map<GraphResourceNameText, rhi::RefTextureDep>	frame_tex_map_;


		rhi::RhiRef<rhi::SwapChainDep>	swapchain_;

	};
}