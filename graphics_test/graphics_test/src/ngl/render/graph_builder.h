﻿#pragma once

#include <unordered_map>
#include <mutex>

#include "ngl/text/hash_text.h"

#include "ngl/rhi/d3d12/device.d3d12.h"
#include "ngl/rhi/d3d12/shader.d3d12.h"
#include "ngl/rhi/d3d12/command_list.d3d12.h"
#include "ngl/rhi/d3d12/resource_view.d3d12.h"

#include "ngl/resource/resource_manager.h"
#include "ngl/gfx/resource/resource_shader.h"

#include "ngl/render/rtg_command_list_pool.h"

namespace ngl
{

	// Render Task Graph 検証実装.
	namespace rtg
	{
		class RenderTaskGraphBuilder;
		using RtgNameType = text::HashCharPtr<64>;

		enum class ETASK_TYPE : int
		{
			GRAPHICS = 0,
			COMPUTE,
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
		// このままMapのキーとして利用するためuint64扱いできるようにしている(もっと整理できそう).
		using ResourceHandleKeyType = uint64_t;
		struct ResourceHandle
		{
			constexpr ResourceHandle() = default;
			constexpr ResourceHandle(ResourceHandleKeyType data)
			{
				this->data = data;
			}
			
			union
			{
				// (u64)0は特殊IDで無効扱い. unique_idが0でもswapchainビットが1であれば有効.
				ResourceHandleKeyType data = 0;
				struct Detail
				{
					uint32_t unique_id;

					uint32_t is_external	: 1; // 一般の外部リソース.
					uint32_t is_swapchain	: 1; // 外部リソースとしてSwapchainを特別扱い. とりあえず簡易にアクセスするため.
					uint32_t dummy			: 30;
				}detail;
			};

			static constexpr ResourceHandle InvalidHandle()
			{
				return ResourceHandle({});
			}
			// 無効はHandleか.
			bool IsInvalid() const
			{
				return detail.unique_id == InvalidHandle().detail.unique_id;
			}
			operator ResourceHandleKeyType() const
			{
				return data;
			}
		};
		static constexpr auto sizeof_ResourceHandle = sizeof(ResourceHandle);

		
		// Taskの基底.
		// 直接このクラスを継承することは出来ない.
		//	IGraphicsTaskBase または IAsyncComputeTaskBase を継承すること.
		struct ITaskNode
		{
		public:
			virtual ~ITaskNode() {}

			// Type.
			virtual ETASK_TYPE TaskType() const = 0;
			
			// レンダリングの実装部. 一旦動的にGraphicsとComputeを分ける.
			// GraphicsTaskの実装用.
			virtual void Run(RenderTaskGraphBuilder& builder, rhi::GraphicsCommandListDep* commandlist) = 0;

			virtual void Run(RenderTaskGraphBuilder& builder, rhi::ComputeCommandListDep* commandlist) = 0;

			
			// ------------------------------------------------------------------------------------------------------------------
			// デバッグ用
		public:
			struct DebugHandleRef
			{
				RtgNameType name{};
				ResourceHandle* p_handle{};
			};
			void RegisterSelfHandle(const char* name, ResourceHandle& handle)
			{
				DebugHandleRef elem = { name , &handle};
				debug_ref_handles_.push_back(elem);
			}
			// Debug用途でメンバとしてHandleを持つ. 名前などを付けたい場合はResourceHandle経由でアクセスできる場所に登録すべきか.
			// Node固有のハンドル情報. ITASK_NODE_HANDLE_REGISTERマクロ経由で登録される.
			// 注意! 一時ハンドルなど, ITASK_NODE_HANDLE_REGISTERを使わないハンドルは登録されないことに注意(このNodeが参照する全てのHandleを網羅する情報ではない).
			std::vector<DebugHandleRef> debug_ref_handles_{};
			
			const RtgNameType& GetDebugNodeName() const { return debug_node_name_; }
		protected:
			void SetDebugNodeName(const char* name){ debug_node_name_ = name; }
			RtgNameType debug_node_name_{};
			// デバッグ用
			// ------------------------------------------------------------------------------------------------------------------
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
				DebugHandleRef elem = { #name , &name};\
				debug_ref_handles_.push_back(elem);\
				};
		// -------------------------------------------------------------

		// -------------------------------------------------------------
		// Nodeクラスのコンストラクタ定義終端マクロ.
		#define ITASK_NODE_DEF_END\
				};
		// -------------------------------------------------------------
		
		/*
		// 生成はRenderTaskGraphBuilder経由.
		// 派生クラスでは基本情報の簡易定義とハンドルのデバッグ情報登録のために専用のマクロを利用する.

			struct TaskSamplePass : public rtg::ITaskNode
			{
				// ノード定義コンストラクタ記述マクロ.
				ITASK_NODE_DEF_BEGIN(TaskSamplePass)
					ITASK_NODE_HANDLE_REGISTER(h_depth_)
				ITASK_NODE_DEF_END

				rtg::ResourceHandle h_depth_{};// メンバハンドル.
			}


		// 他のNodeのハンドルを参照して利用したり一時リソースの登録が可能.

			// リソースとアクセスを定義するプリプロセス.
			void Setup(rtg::RenderTaskGraphBuilder& builder, rtg::ResourceHandle h_depth, rtg::ResourceHandle h_gb0, rtg::ResourceHandle h_gb1)
			{
				// リソース定義.
				rtg::ResourceDesc2D light_desc = rtg::ResourceDesc2D::CreateAsRelative(1.0f, 1.0f, rhi::ResourceFormat::Format_R16G16B16A16_FLOAT);

				// リソースアクセス定義.
				h_depth_ =	builder.RegisterResourceAccess(*this, h_depth, rtg::access_type::SHADER_READ);
				h_gb0_ =	builder.RegisterResourceAccess(*this, h_gb0, rtg::access_type::SHADER_READ);
				h_gb1_ =	builder.RegisterResourceAccess(*this, h_gb1, rtg::access_type::SHADER_READ);
				h_light_=	builder.RegisterResourceAccess(*this, builder.CreateResource(light_desc), rtg::access_type::RENDER_TARTGET);// 他のNodeのものではなく新規リソースを要求する.


				// リソースアクセス期間による再利用のテスト用. 作業用の一時リソース.
				rtg::ResourceDesc2D temp_desc = rtg::ResourceDesc2D::CreateAsRelative(1.0f, 1.0f, rhi::ResourceFormat::Format_R11G11B10_FLOAT);
				auto temp_res0 = builder.RegisterResourceAccess(*this, builder.CreateResource(temp_desc), rtg::access_type::RENDER_TARTGET);
			}
		*/
		// GraphicsTaskの基底クラス.
		struct IGraphicsTaskNode : public ITaskNode
		{
		public:
			virtual ~IGraphicsTaskNode() = default;
			// Type Graphics.
			ETASK_TYPE TaskType() const final
			{ return ETASK_TYPE::GRAPHICS; }

			// 念の為に空で継承も禁止.
			void Run(RenderTaskGraphBuilder& builder, rhi::ComputeCommandListDep* commandlist) final
			{
				assert(false);
			};
		};
		// ComputeTaskの基底クラス.
		// GraphicsでもAsyncComputeでも実行可能なもの. UAVバリア以外のバリアは出来ないようにComputeCommandListのみ利用可能とする.
		// このTaskで利用するリソースのためのState遷移はRtg側の責任とする.
		struct IComputeTaskNode : public ITaskNode
		{
		public:
			virtual ~IComputeTaskNode() = default;
			// Type AsyncCompute.
			ETASK_TYPE TaskType() const final
			{ return ETASK_TYPE::COMPUTE; }
			
			// 念の為に空で継承も禁止.
			void Run(RenderTaskGraphBuilder& builder, rhi::GraphicsCommandListDep* commandlist) final
			{
				assert(false);
			};
		};



		// ハンドル毎のタイムライン上での位置を示す情報を生成.
		// AsyncComputeのFenceを考慮して, 同期で区切られる Stage番号 と Stage内の順序である Step番号 の2つにする予定.
		// [GraphicsとAsyncComputeの間でのリソース再利用やリソース読み書きはstageをまたぐ必要が有る]などのチェックや制御に使う
		struct TaskStage
		{
			constexpr TaskStage() = default;
			
			int stage_ = 0;// Stage番号. Sequence先頭である0からみてさらに以前を表現したいため符号付き.
			int step_ = 0;// Stage内でのローカル番号. Sequence先頭 0 からみてさらに以前を表現したいため符号付き.

			// Stage 0 に対して常に前となるようなStage. リソースプール側リソースのリセットや新規生成リソースのステージとして利用.
			static constexpr TaskStage k_frontmost_stage()
			{
				return TaskStage{std::numeric_limits<int>::min(), std::numeric_limits<int>::min()};
			};
			// 最終端のStage.
			static constexpr TaskStage k_endmost_stage()
			{
				return TaskStage{std::numeric_limits<int>::max(), std::numeric_limits<int>::max()};
			};
			
			// オペレータ.
			constexpr bool operator<(const TaskStage arg) const;
			constexpr bool operator>(const TaskStage arg) const;
			constexpr bool operator<=(const TaskStage arg) const;
			constexpr bool operator>=(const TaskStage arg) const;
		};
		
		// リソースの検索キー.
		struct ResourceSearchKey
		{
			rhi::ResourceFormat format = {};
			int require_width_ = {};
			int require_height_ = {};
			ACCESS_TYPE_MASK	usage_ = {};// 要求する RenderTarget, DepthStencil, UAV等の用途.
		};
		
		// 内部リソースプール用.
		struct InternalResourceInstanceInfo
		{
			// 未使用フレームカウンタ. 一定フレーム未使用だった内部リソースはPoolからの破棄をする.
			int			unused_frame_counter_ = 0;
			
			TaskStage last_access_stage_ = {};// Compile中のシーケンス上でのこのリソースへ最後にアクセスしたタスクの情報. Compile完了後にリセットされる.
				
			rhi::ResourceState	cached_state_ = rhi::ResourceState::Common;// Compileで確定したGraph終端でのステート.
			rhi::ResourceState	prev_cached_state_ = rhi::ResourceState::Common;// 前回情報. Compileで確定したGraph終端でのステート.
				
			rhi::RefTextureDep	tex_ = {};
			
			rhi::RefRtvDep		rtv_ = {};
			rhi::RefDsvDep		dsv_ = {};
			rhi::RefUavDep		uav_ = {};
			rhi::RefSrvDep		srv_ = {};

			bool IsValid() const
			{
				return tex_.IsValid();// 元リソースがあれば有効.
			}
		};
		// 外部リソース登録用. 内部リソース管理クラスを継承して追加情報.
		struct ExternalResourceInfo : public InternalResourceInstanceInfo
		{
			rhi::RhiRef<rhi::SwapChainDep>	swapchain_ = {}; // 外部リソースの場合はSwapchainもあり得るため追加.
			
			rhi::ResourceState	require_begin_state_ = rhi::ResourceState::Common;// 外部登録で指定された開始ステート.
			rhi::ResourceState	require_end_state_ = rhi::ResourceState::Common;// 外部登録で指定された終了ステート. Executeの終端で遷移しているべきステート.
		};
		
		struct RtgCommandListRangeInfo
		{
			ETASK_TYPE type = ETASK_TYPE::GRAPHICS;
			int begin = -1;// 直列化CommandList配列上の開始インデックス.
			int count = 0;// 直列化CommandList配列上の開始インデックスからの個数.
				
			int fence_signal = -1;// Signal発行するFenceのユニークインデックス(-1で無効).
			int fence_wait = -1;// WaitするFenceのユニークインデックス(-1で無効).
		};
		
		// レンダリング処理をもつTaskNode列を記録し, リソースの割当やリソース状態遷移を解決してTaskNodeの並列CommandList構築を実現する.
		//	このクラスのインスタンスは　TaskNodeのRecord, Compile, Execute の一連の処理の後に使い捨てとなる. これは使いまわしのための状態リセットの実装ミスを避けるため.
		//	
		//  TaskNode内部の一時リソースやTaskNode間のリソースフローはHandleを介して記録し, Compileによって実際のリソース割当や状態遷移の解決をする.
		// 	CommandListはCreateされたTaskNodeの順序となる.
		//	AsyncComputeのTaskNodeもサポート予定で, Fence風のSignalとWeightによってGPU側での同期を記述することを検討中.
		class RenderTaskGraphBuilder
		{
			friend class RenderTaskGraphManager;
		public:
			~RenderTaskGraphBuilder();

			// ITaskBase派生クラスをシーケンスの末尾に新規生成する.
			// GraphicsおよびAsyncCompute両方を登録する. それぞれのタイプ毎での実行順は登録順となる. GraphicsとAsyncComputeの同期ポイントは別途指示する予定.
			template<typename TTaskNode>
			TTaskNode* AppendNodeToSequence()
			{
				auto new_node = new TTaskNode();
				node_sequence_.push_back(new_node);
				return new_node;
			}

		public:
			// リソースハンドルを生成.
			//	Graph内リソースを確保してハンドルを取得する.
			ResourceHandle CreateResource(ResourceDesc2D res_desc);

			// 次のフレームへ寿命を延長する.
			//	前回フレームのハンドルのリソースを利用する場合に, この関数で寿命を延長した上で次フレームで同じハンドルを使うことでアクセス可能にする予定.
			ResourceHandle PropagateResouceToNextFrame(ResourceHandle handle);

			// 外部リソースを登録してハンドルを生成. 一般.
			//	rtv,dsv,srv,uavはそれぞれ登録するものだけ有効な参照を指定する.
			// curr_state			: 外部リソースのGraph開始時点のステート.
			// nesesary_end_state	: 外部リソースのGraph実行完了時点で遷移しているべきステート. 外部から要求する最終ステート遷移.
			ResourceHandle AppendExternalResource(rhi::RefTextureDep tex, rhi::RefRtvDep rtv, rhi::RefDsvDep dsv, rhi::RefSrvDep srv, rhi::RefUavDep uav,
				rhi::ResourceState curr_state, rhi::ResourceState nesesary_end_state);
			
			// 外部リソースを登録してハンドルを生成. Swapchain用.
			// curr_state			: 外部リソースのGraph開始時点のステート.
			// nesesary_end_state	: 外部リソースのGraph実行完了時点で遷移しているべきステート. 外部から要求する最終ステート遷移.
			ResourceHandle AppendExternalResource(rhi::RhiRef<rhi::SwapChainDep> swapchain, rhi::RefRtvDep swapchain_rtv,
				rhi::ResourceState curr_state, rhi::ResourceState nesesary_end_state);
			
			// Swapchainリソースハンドルを取得. 外部リソースとしてSwapchainは特別扱い.
			ResourceHandle GetSwapchainResourceHandle() const;
			
			// Handleのリソース定義情報を取得.
			ResourceDesc2D GetResourceHandleDesc(ResourceHandle handle) const;

			
			// Nodeからのリソースアクセスを記録.
			// NodeのRender実行順と一致する順序で登録をする必要がある. この順序によってリソースステート遷移の確定や実リソースの割当等をする.
			ResourceHandle RecordResourceAccess(const ITaskNode& node, const ResourceHandle res_handle, const ACCESS_TYPE access_type);
			
			// Graph実行.
			// Compileしたグラフを実行しCommandListを構築する. Compileはリソースプールを管理する RenderTaskGraphManager 経由で実行する.
			// Task毎にCommandListを割り当て, Submit用のCommandListのリストを返す.
			void ExecuteMultiCommandlist(std::vector<rhi::CommandListBaseDep*>& out_executed_command_list_array, std::vector<RtgCommandListRangeInfo>& out_executed_command_list_range_info);

			// -------------------------------------------------------------------------------------------
			// Compileで割り当てられたHandleのリソース情報.
			struct AllocatedHandleResourceInfo
			{
				AllocatedHandleResourceInfo() = default;

				rhi::ResourceState	prev_state_ = rhi::ResourceState::Common;// NodeからResourceHandleでアクセスした際の直前のリソースステート.
				rhi::ResourceState	curr_state_ = rhi::ResourceState::Common;// NodeからResourceHandleでアクセスした際の現在のリソースステート. RTGによって自動的にステート遷移コマンドが発行される.

				rhi::RefTextureDep				tex_ = {};
				rhi::RhiRef<rhi::SwapChainDep>	swapchain_ = {};// Swapchainの場合はこちらに参照が設定される.

				rhi::RefRtvDep		rtv_ = {};
				rhi::RefDsvDep		dsv_ = {};
				rhi::RefUavDep		uav_ = {};
				rhi::RefSrvDep		srv_ = {};
			};
			// NodeのHandleに対して割り当て済みリソースを取得する.
			// Graphシステム側で必要なBarrierコマンドを発効するため基本的にNode実装側ではBarrierコマンドは不要.
			AllocatedHandleResourceInfo GetAllocatedResource(const ITaskNode* node, ResourceHandle res_handle) const;
			// -------------------------------------------------------------------------------------------
			
		private:
			enum class EBuilderState
			{
				RECORDING,
				COMPILED,
				EXECUTED
			};
			EBuilderState	state_ = EBuilderState::RECORDING;
			
			class RenderTaskGraphManager* p_compiled_manager_ = nullptr;// Compileを実行したManager. 割り当てられたリソースなどはこのManagerが持っている.
			
			// -------------------------------------------------------------------------------------------
			static constexpr  int k_base_height = 1080;
			int res_base_height_ = k_base_height;
			int res_base_width_ = static_cast<int>( static_cast<float>(k_base_height) * 16.0f/9.0f);
			
			std::vector<ITaskNode*> node_sequence_{};// Graph構成ノードシーケンス. 生成順がGPU実行順で, AsyncComputeもFenceで同期をする以外は同様.
			std::unordered_map<ResourceHandleKeyType, ResourceDesc2D> handle_2_desc_{};// Handleからその定義のMap.
			
			struct NodeHandleUsageInfo
			{
				ResourceHandle			handle{};// あるNodeからどのようなHandleで利用されたか.
				ACCESS_TYPE				access{};// あるNodeから上記Handleがどのアクセスタイプで利用されたか.
			};
			std::unordered_map<const ITaskNode*, std::vector<NodeHandleUsageInfo>> node_handle_usage_list_{};// Node毎のResourceHandleアクセス情報をまとめるMap.
			// ------------------------------------------------------------------------------------------------------------------------------------------------------
			// Importリソース用.
			std::vector<ExternalResourceInfo> imported_resource_ = {};
			std::unordered_map<ResourceHandleKeyType, int> imported_handle_2_index_ = {};

			// ImportしたSwapchainは何かとアクセスするためHandle保持.
			ResourceHandle	handle_imported_swapchain_ = {};
			// ------------------------------------------------------------------------------------------------------------------------------------------------------
			
			// 次フレームまで寿命を延長するハンドル.
			std::unordered_map<ResourceHandleKeyType, int> propagate_next_handle_ = {};
			// ------------------------------------------------------------------------------------------------------------------------------------------------------

			
			// ------------------------------------------------------------------------------------------------------------------------------------------------------
			// Compileで構築される情報.
				struct NodeDependency
				{
					int from = -1;
					int to = -1;

					int fence_id = -1;// Wait側に格納されるFence. Signal側はtoの指すIndexが持つこのIDを利用する.
				};
				// Queue違いのNode間のfence依存関係.
				std::vector<NodeDependency> node_dependency_fence_ = {};
				// HandleからリニアインデックスへのMap.
				std::unordered_map<ResourceHandleKeyType, int> handle_2_compiled_index_ = {};
				// NodeSequenceの順序に沿ったHandle配列.
				std::vector<ResourceHandle>						compiled_index_handle_ = {};

				// Compileで構築される情報.
				// Handleに割り当てられたリソースのPool上のIndex.
				// このままMapのキーとして利用するためuint64扱いできるようにしている(もっと整理できそう).
				using CompiledResourceInfoKeyType = uint64_t;  
				struct CompiledResourceInfo
				{
					union
					{
						// (u64)0は特殊IDで無効扱い. unique_idが0でもswapchainビットが1であれば有効.
						CompiledResourceInfoKeyType data = {};
						struct Detail
						{
							int32_t	 resource_id;		// 内部リソースプール又は外部リソースリストへの参照.
							uint32_t is_external	: 1; // 外部リソースマーク.
							uint32_t dummy			: 31;
						}detail;
					};
					
					CompiledResourceInfo() = default;
					constexpr CompiledResourceInfo(CompiledResourceInfoKeyType data)
					{ this->data = data; }
					
					operator CompiledResourceInfoKeyType() const
					{ return data; }

					// 無効値.
					static constexpr CompiledResourceInfo k_invalid()
					{
						CompiledResourceInfo tmp = {};
						tmp.detail.resource_id = -1;// 無効値.
						return tmp;
					}
				};
				// Handleのリニアインデックスから割り当て済みリソースID.
				std::vector<CompiledResourceInfo> handle_compiled_resource_id_ = {};
				
				struct NodeHandleState
				{
					rhi::ResourceState prev_ = {};
					rhi::ResourceState curr_ = {};
				};
				// Compileで構築される情報.
				// NodeのHandle毎のリソース状態遷移.
				std::unordered_map<const ITaskNode*, std::unordered_map<ResourceHandleKeyType, NodeHandleState>> node_handle_state_ = {};
			// ------------------------------------------------------------------------------------------------------------------------------------------------------

		private:
			// グラフからリソース割当と状態遷移を確定.
			// 現状はRenderThreadでCompileしてそのままRenderThreadで実行するというスタイルとする.
			bool Compile(class RenderTaskGraphManager& manager);
			
			// Sequence上でのノードの位置を返す.
			int GetNodeSequencePosition(const ITaskNode* p_node) const;

			// Builderの状態取得用.
			bool IsRecordable() const;
			bool IsCompilable() const;
			bool IsExecutable() const;
			
			// ------------------------------------------
			// 外部リソースを登録共通部.
			ResourceHandle AppendExternalResourceCommon(
				rhi::RefTextureDep tex, rhi::RhiRef<rhi::SwapChainDep> swapchain, rhi::RefRtvDep rtv, rhi::RefDsvDep dsv, rhi::RefSrvDep srv, rhi::RefUavDep uav,
				rhi::ResourceState curr_state, rhi::ResourceState nesesary_end_state);
		};



	// ------------------------------------------------------------------------------------------------------------------------------------------------------
		// RenderTaskGraphBuilderのCompileや, それらが利用するリソースの永続的なプール管理.
		class RenderTaskGraphManager
		{
			friend class RenderTaskGraphBuilder;
			
		public:
			RenderTaskGraphManager() = default;
			~RenderTaskGraphManager();
		public:
			// 初期化.
			bool Init(rhi::DeviceDep* p_device);

			//	フレーム開始通知. 内部リソースプールの中で一定フレームアクセスされていないものを破棄するなどの処理.
			void BeginFrame();

			// builderをCompileしてリソース割当を確定する.
			//	Compileしたbuilderは必ずExecuteする必要がある.
			//	また, 複数のbuilderをCompileした場合はCompileした順序でExecuteが必要(確定したリソースの状態遷移コマンド実行を正しい順序で実行するために).
			bool Compile(RenderTaskGraphBuilder& builder);
			
		private:
			rhi::DeviceDep* p_device_ = nullptr;
			
			// 同一Manager下のBuilderのCompileは排他処理.
			std::mutex	compile_mutex_ = {};
			
			// Compileで割り当てられるリソースのPool.
			std::vector<InternalResourceInstanceInfo> internal_resource_pool_ = {};
			
			// 次のフレームへ伝搬するハンドルとリソースIDのMap.
			std::unordered_map<ResourceHandleKeyType, int> propagate_next_handle_[2] = {};
			// 次のフレームへ伝搬するハンドル登録用FlipIndex. 前回フレームから伝搬されたハンドルは 1-flip_propagate_next_handle_next_ のMapが対応.
			int flip_propagate_next_handle_next_ = 0;

			// Poolからリソース検索または新規生成. 戻り値は実リソースID.
			//	検索用のリソース定義keyと, アクセス期間外の再利用のためのアクセスステージ情報を引数に取る.
			//	access_stage : リソース再利用を有効にしてアクセス開始ステージを指定する, nullptrの場合はリソース再利用をしない.
			int GetOrCreateResourceFromPool(ResourceSearchKey key, const TaskStage* p_access_stage_for_reuse = nullptr);
			// プールリソースの最終アクセス情報を書き換え. BuilderのCompile時の一時的な用途.
			void SetInternalResouceLastAccess(int resource_id, TaskStage last_access_stage);
			// 割り当て済みリソース番号から内部リソースポインタ取得.
			InternalResourceInstanceInfo* GetInternalResourcePtr(int resource_id);

			// BuilderからハンドルとリソースIDを紐づけて次のフレームへ伝搬する.
			void PropagateResourceToNextFrame(ResourceHandle handle, int resource_id);
			// 伝搬されたハンドルに紐付けられたリソースIDを検索.
			int FindPropagatedResourceId(ResourceHandle handle);
			
		public:
			// 本来はこれらはBuilder側で隠蔽する予定(CommandListの確保もスケジューリングする).
			// Builderが利用するCommandListの新規取得.
			void GetNewFrameCommandList(rhi::GraphicsCommandListDep*& out_ref)
			{
				commandlist_pool_.GetFrameCommandList(out_ref);
			}
			// Builderが利用するCommandListの新規取得.
			void GetNewFrameCommandList(rhi::ComputeCommandListDep*& out_ref)
			{
				commandlist_pool_.GetFrameCommandList(out_ref);
			}
		private:
			pool::CommandListPool commandlist_pool_ = {};
			
		private:
			// ユニークなハンドルIDを取得.
			//	64bitにするかもしれない.
			static uint32_t GetNewHandleId();
			static uint32_t	s_res_handle_id_counter_;// リソースハンドルユニークID. 生成のたびに加算しユニーク識別.
		};
		
		// ------------------------------------------------------------------------------------------------------------------------------------------------------
		
	}
}