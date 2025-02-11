﻿#pragma once
/*

	// IGraphicsTaskNode を継承してPreZPassを実装する例.
	struct TaskDepthPass : public rtg::IGraphicsTaskNode
	{
		rtg::ResourceHandle h_depth_{};// RTGリソースハンドル保持.
		
		// リソースとアクセスを定義するプリプロセス.
		void Setup(rtg::RenderTaskGraphBuilder& builder, rhi::DeviceDep* p_device, const SetupDesc& desc)
		{
			// リソース定義.
			rtg::ResourceDesc2D depth_desc = rtg::ResourceDesc2D::CreateAsRelative(1.0f, 1.0f, gfx::MaterialPassPsoCreator_depth::k_depth_format);
			// 新規作成したDepthBufferリソースをDepthTarget使用としてレコード.
			h_depth_ = builder.RecordResourceAccess(*this, builder.CreateResource(depth_desc), rtg::access_type::DEPTH_TARGET);
		}
		// 実際のレンダリング処理.
		void Run(rtg::RenderTaskGraphBuilder& builder, rhi::GraphicsCommandListDep* gfx_commandlist) override
		{
			// ハンドルからリソース取得. 必要なBarrier/ステート遷移はRTGシステムが担当するため, 個々のTaskは必要な状態になっているリソースを使用できる.
			auto res_depth = builder.GetAllocatedResource(this, h_depth_);
			assert(res_depth.tex_.IsValid() && res_depth.dsv_.IsValid());

			// 例:クリア
			gfx_commandlist->ClearDepthTarget(res_depth.dsv_.Get(), 0.0f, 0, true, true);// とりあえずクリアだけ.ReverseZなので0クリア.

			// 例:DepthRenderTagetとして設定してレンダリング.
			gfx_commandlist->SetRenderTargets(nullptr, 0, res_depth.dsv_.Get());
			ngl::gfx::helper::SetFullscreenViewportAndScissor(gfx_commandlist, res_depth.tex_->GetWidth(), res_depth.tex_->GetHeight());
			// TODO. 描画.
			// ....
		}
	};

	// IGraphicsTaskNode を継承してHardwareDepthからLinearDepthを計算するPass例.
	//	先行するPreZPass(DepthPass)の書き込み先リソースハンドルを読み取り使用し, LinearDepthを出力する.
	//	別のTaskのリソースハンドルを利用する例.
	struct TaskLinearDepthPass : public rtg::IGraphicsTaskNode
	{
		rtg::ResourceHandle h_depth_{};
		rtg::ResourceHandle h_linear_depth_{};

		// リソースとアクセスを定義するプリプロセス.
		void Setup(rtg::RenderTaskGraphBuilder& builder, rhi::DeviceDep* p_device, rtg::ResourceHandle h_depth, rtg::ResourceHandle h_tex_compute, const SetupDesc& desc)
		{
			// LinearDepth出力用のバッファを新規に作成してUAV使用としてレコード.
			rtg::ResourceDesc2D linear_depth_desc = rtg::ResourceDesc2D::CreateAsRelative(1.0f, 1.0f, rhi::EResourceFormat::Format_R32_FLOAT);
			h_linear_depth_ = builder.RecordResourceAccess(*this, builder.CreateResource(linear_depth_desc), rtg::access_type::UAV);
			
			// 先行するDepth書き込みTaskの出力先リソースハンドルを利用し, 読み取り使用としてレコード.
			h_depth_ = builder.RecordResourceAccess(*this, h_depth, rtg::access_type::SHADER_READ);
		}
		// 実際のレンダリング処理.
		void Run(rtg::RenderTaskGraphBuilder& builder, rhi::GraphicsCommandListDep* gfx_commandlist) override
		{
			// ハンドルからリソース取得. 必要なBarrierコマンドは外部で発行済である.
			auto res_depth = builder.GetAllocatedResource(this, h_depth_);
			auto res_linear_depth = builder.GetAllocatedResource(this, h_linear_depth_);
			// TODO. depthからlinear_depthを生成するシェーダディスパッチ.
			// ...
		}
	};


	// ------------------------------------------
	// BuilderによるPassグラフの定義例.
	// 最初にDepthPass
	auto* task_depth = rtg_builder.AppendTaskNode<ngl::render::task::TaskDepthPass>();
	// ...

	auto* task_linear_depth = rtg_builder.AppendTaskNode<ngl::render::task::TaskLinearDepthPass>();
	// 先行するDepthPassの出力Depthハンドルを読み取り使用でレコードするため引数にとる.
	task_linear_depth->Setup(..., task_depth->h_depth_, ...);


	// 次回フレームへの伝搬. このGraphで生成されたハンドルとそのリソースを次フレームでも利用できるようにする. ヒストリバッファ用の機能.
	h_propagate_lit = rtg_builder.PropagateResouceToNextFrame(task_light->h_light_);


	// Compile.
	//	ManagerでCompileを実行する. ここで内部リソースプールからのリソース割り当てやTask間のリソースステート遷移スケジュールを確定.
	//	Compileによって各種スケジューリングが決定されるため, 必ずExecuteとGPU-Submitをして実行をしなければリソース整合性が破綻する点に注意.
	//		また複数のGraphをCompileした場合はその順番でGpu-Submitしなければならない
	rtg_manager.Compile(rtg_builder);
		
	// Graphを構成するTaskの Run() を実行して, CommandListを生成する.
	rtg_builder.Execute(out_graphics_cmd, out_compute_cmd);

	// out_graphics_cmd と out_compute_cmd は非同期コンピュートを考慮したコマンドリスト列.
	// Managerのヘルパー関数でGPUへSubmitする.
	ngl::rtg::RenderTaskGraphBuilder::SubmitCommand(graphics_queue_, compute_queue_, out_graphics_cmd, out_compute_cmd);
 */

#include <unordered_map>
#include <mutex>

#include "ngl/text/hash_text.h"

#include "ngl/rhi/d3d12/device.d3d12.h"
#include "ngl/rhi/d3d12/shader.d3d12.h"
#include "ngl/rhi/d3d12/command_list.d3d12.h"
#include "ngl/rhi/d3d12/resource_view.d3d12.h"

#include "ngl/resource/resource_manager.h"

#include "rtg_command_list_pool.h"

#include "ngl/thread/job_thread.h"

namespace ngl
{

	// Render Task Graph.
	namespace rtg
	{
		class RenderTaskGraphBuilder;
		using RtgNameType = text::HashText<64>;

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
		inline bool RtgIsWriteAccess(ACCESS_TYPE type)
		{
			constexpr auto k_write_assecc_mask = access_type_mask::RENDER_TARTGET | access_type_mask::DEPTH_TARGET | access_type_mask::UAV;
			const ACCESS_TYPE_MASK mask = 1 << type;
			return 0 != (k_write_assecc_mask & mask);
		}
		

		// Passが必要とするリソースの定義.
		struct RtgResourceDesc2D
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
				rhi::EResourceFormat format {};
				bool is_relative {};


				// サイズ直接指定. その他データはEmpty.
				static constexpr RtgResourceDesc2D CreateAsAbsoluteSize(int w, int h)
				{
					RtgResourceDesc2D v{};
					v.desc.is_relative = false;
					v.desc.abs_size = { w, h };
					return v;
				}
				// 相対サイズ指定. その他データはEmpty.
				static constexpr RtgResourceDesc2D CreateAsRelative(float w_rate, float h_rate)
				{
					RtgResourceDesc2D v{};
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
			static constexpr RtgResourceDesc2D CreateAsAbsoluteSize(int w, int h, rhi::EResourceFormat format)
			{
				RtgResourceDesc2D v = Desc::CreateAsAbsoluteSize(w, h);
				v.desc.format = format;
				return v;
			}
			// 相対サイズ指定.
			static constexpr RtgResourceDesc2D CreateAsRelative(float w_rate, float h_rate, rhi::EResourceFormat format)
			{
				RtgResourceDesc2D v = Desc::CreateAsRelative(w_rate, h_rate);
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
		static_assert(sizeof(RtgResourceDesc2D) == sizeof(RtgResourceDesc2D::Storage));
		static constexpr auto sizeof_ResourceDesc2D_Desc = sizeof(RtgResourceDesc2D::Desc);
		static constexpr auto sizeof_ResourceDesc2D_Storage = sizeof(RtgResourceDesc2D::Storage);
		static constexpr auto sizeof_ResourceDesc2D = sizeof(RtgResourceDesc2D);

		
		// RTGのノードが利用するリソースハンドル.
		// 識別IDやSwapchain識別等の情報を保持.
		// このままMapのキーとして利用するためuint64扱いできるようにしている(もっと整理できそう).
		using RtgResourceHandleKeyType = uint64_t;
		struct RtgResourceHandle
		{
			constexpr RtgResourceHandle() = default;
			constexpr RtgResourceHandle(RtgResourceHandleKeyType data)
			{
				this->data = data;
			}
			
			union
			{
				// (u64)0は特殊IDで無効扱い. unique_idが0でもswapchainビットが1であれば有効.
				RtgResourceHandleKeyType data = 0;
				struct Detail
				{
					uint32_t unique_id;

					uint32_t is_external	: 1; // 一般の外部リソース.
					uint32_t is_swapchain	: 1; // 外部リソースとしてSwapchainを特別扱い. とりあえず簡易にアクセスするため.
					uint32_t dummy			: 30;
				}detail;
			};

			static constexpr RtgResourceHandle InvalidHandle()
			{
				return RtgResourceHandle({});
			}
			// 無効はHandleか.
			bool IsInvalid() const
			{
				return detail.unique_id == InvalidHandle().detail.unique_id;
			}
			operator RtgResourceHandleKeyType() const
			{
				return data;
			}
		};
		static constexpr auto sizeof_ResourceHandle = sizeof(RtgResourceHandle);

		
		// Taskの基底.
		// 直接このクラスを継承することは出来ない.
		//	IGraphicsTaskBase または IAsyncComputeTaskBase を継承すること.
		struct ITaskNode
		{
		public:
			virtual ~ITaskNode() {}

			// Type.
			virtual ETASK_TYPE TaskType() const = 0;
			
			// レンダリングの実装部.
			virtual void Run(RenderTaskGraphBuilder& builder, rhi::GraphicsCommandListDep* commandlist) = 0;
			virtual void Run(RenderTaskGraphBuilder& builder, rhi::ComputeCommandListDep* commandlist) = 0;

		public:
			const RtgNameType& GetDebugNodeName() const { return debug_node_name_; }
		protected:
			void SetDebugNodeName(const char* name){ debug_node_name_ = name; }
			RtgNameType debug_node_name_{};
		};
		
		// 生成はRenderTaskGraphBuilder経由.
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

		// -------------------------------------------------------------------------------------------
		// Compileで割り当てられたHandleのリソース情報.
		struct RtgAllocatedResourceInfo
		{
			RtgAllocatedResourceInfo() = default;

			rhi::EResourceState	prev_state_ = rhi::EResourceState::Common;// NodeからResourceHandleでアクセスした際の直前のリソースステート.
			rhi::EResourceState	curr_state_ = rhi::EResourceState::Common;// NodeからResourceHandleでアクセスした際の現在のリソースステート. RTGによって自動的にステート遷移コマンドが発行される.

			rhi::RefTextureDep				tex_ = {};
			rhi::RhiRef<rhi::SwapChainDep>	swapchain_ = {};// Swapchainの場合はこちらに参照が設定される.

			rhi::RefRtvDep		rtv_ = {};
			rhi::RefDsvDep		dsv_ = {};
			rhi::RefUavDep		uav_ = {};
			rhi::RefSrvDep		srv_ = {};
		};


		// ハンドル毎のタイムライン上での位置を示す情報を生成.
		struct TaskStage
		{
			int step_ = 0;
			
			constexpr TaskStage() = default;
			
			// Stage 0 に対して常に前となるようなStage. リソースプール側リソースのリセットや新規生成リソースのステージとして利用.
			static constexpr TaskStage k_frontmost_stage()
			{
				return TaskStage{std::numeric_limits<int>::min()};
			};
			// 最終端のStage.
			static constexpr TaskStage k_endmost_stage()
			{
				return TaskStage{std::numeric_limits<int>::max()};
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
			rhi::EResourceFormat format = {};
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
				
			rhi::EResourceState	cached_state_ = rhi::EResourceState::Common;// Compileで確定したGraph終端でのステート.
			rhi::EResourceState	prev_cached_state_ = rhi::EResourceState::Common;// 前回情報. Compileで確定したGraph終端でのステート.
				
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
			
			rhi::EResourceState	require_begin_state_ = rhi::EResourceState::Common;// 外部登録で指定された開始ステート.
			rhi::EResourceState	require_end_state_ = rhi::EResourceState::Common;// 外部登録で指定された終了ステート. Executeの終端で遷移しているべきステート.
		};
		
		// Execute結果のCommandSequence要素のタイプ.
		enum ERtgSubmitCommandType
		{
			CommandList,
			Signal,
			Wait
		};
		// Execute結果のCommandSequence要素.
		struct RtgSubmitCommandSequenceElem
		{
			ERtgSubmitCommandType type = ERtgSubmitCommandType::CommandList;

			// ERtgSubmitCommandType == CommandList
			//	Rtg管理化のPoolから現在フレームのみの寿命として割り当てられたCommandList. 内部プール利用の関係でRefではなくポインタ.
			rhi::CommandListBaseDep* command_list = {};

			// ERtgSubmitCommandType == Signal or Wait
			//	Rtg管理化のPoolから現在フレームのみの寿命として割り当てられたFence.
			rhi::RhiRef<rhi::FenceDep>	fence = {};
			//	Rtgがスケジュールした同期のためのSignalまたはWaitで使用するFenceValue.
			u64							fence_value = 0;
		};

		// レンダリングパスのシーケンスとそれらのリソース依存関係解決.
		//	このクラスのインスタンスは　TaskNodeのRecord, Compile, Execute の一連の処理の後に使い捨てとなる. これは使いまわしのための状態リセットの実装ミスを避けるため.
		//  TaskNode内部の一時リソースやTaskNode間のリソースフローはHandleを介して記録し, Compileによって実際のリソース割当や状態遷移の解決をする.
		// 	CommandListはCreateされたTaskNodeの順序となる.
		//	AsyncComputeのTaskNodeもサポート予定で, Fence風のSignalとWeightによってGPU側での同期を記述することを検討中.
		class RenderTaskGraphBuilder
		{
			friend class RenderTaskGraphManager;
		public:
			RenderTaskGraphBuilder() = default;
			RenderTaskGraphBuilder(int base_resolution_width, int base_resolution_height)
			{
				res_base_height_ = base_resolution_height;
				res_base_width_ = base_resolution_width;
			}
			
			~RenderTaskGraphBuilder();

			// ITaskBase派生クラスをシーケンスの末尾に新規生成する.
			// GraphicsおよびAsyncCompute両方を登録する. それぞれのタイプ毎での実行順は登録順となる. GraphicsとAsyncComputeの同期ポイントは別途指示する予定.
			template<typename TTaskNode>
			TTaskNode* AppendTaskNode()
			{
				// Compile前のRecordフェーズでのみ許可.
				assert(IsRecordable());
				
				auto new_node = new TTaskNode();
				node_sequence_.push_back(new_node);
				return new_node;
			}

		public:
			// リソースハンドルを生成.
			//	Graph内リソースを確保してハンドルを取得する.
			RtgResourceHandle CreateResource(RtgResourceDesc2D res_desc);

			// 次のフレームへ寿命を延長する.
			//	前回フレームのハンドルのリソースを利用する場合に, この関数で寿命を延長した上で次フレームで同じハンドルを使うことでアクセス可能にする予定.
			RtgResourceHandle PropagateResouceToNextFrame(RtgResourceHandle handle);

			// 外部リソースを登録してハンドルを生成. 一般.
			//	rtv,dsv,srv,uavはそれぞれ登録するものだけ有効な参照を指定する.
			// curr_state			: 外部リソースのGraph開始時点のステート.
			// nesesary_end_state	: 外部リソースのGraph実行完了時点で遷移しているべきステート. 外部から要求する最終ステート遷移.
			RtgResourceHandle RegisterExternalResource(rhi::RefTextureDep tex, rhi::RefRtvDep rtv, rhi::RefDsvDep dsv, rhi::RefSrvDep srv, rhi::RefUavDep uav,
				rhi::EResourceState curr_state, rhi::EResourceState nesesary_end_state);
			
			// 外部リソースを登録してハンドルを生成. Swapchain用.
			// curr_state			: 外部リソースのGraph開始時点のステート.
			// nesesary_end_state	: 外部リソースのGraph実行完了時点で遷移しているべきステート. 外部から要求する最終ステート遷移.
			RtgResourceHandle RegisterSwapchainResource(rhi::RhiRef<rhi::SwapChainDep> swapchain, rhi::RefRtvDep swapchain_rtv,
				rhi::EResourceState curr_state, rhi::EResourceState nesesary_end_state);
			
			// Swapchainリソースハンドルを取得. 外部リソースとしてSwapchainは特別扱い.
			RtgResourceHandle GetSwapchainResourceHandle() const;
			
			// Handleのリソース定義情報を取得.
			RtgResourceDesc2D GetResourceHandleDesc(RtgResourceHandle handle) const;

			
			// Nodeからのリソースアクセスを記録.
			// NodeのRender実行順と一致する順序で登録をする必要がある. この順序によってリソースステート遷移の確定や実リソースの割当等をする.
			RtgResourceHandle RecordResourceAccess(const ITaskNode& node, const RtgResourceHandle res_handle, const ACCESS_TYPE access_type);

		public:
			// Graph実行.
			// Compile済みのGraphを実行しCommandListを構築する. JobSystemが指定された場合は利用して並列実行する.
			// 結果はQueueへSubmitするCommandListとFenceのSequence.
			// 結果のSequenceは外部でQueueに直接Submitすることも可能であるが, ヘルパ関数SubmitCommand()を利用することを推奨する.
			void Execute(
				std::vector<RtgSubmitCommandSequenceElem>& out_graphics_commands, std::vector<RtgSubmitCommandSequenceElem>& out_compute_commands,
				thread::JobSystem* p_job_system = nullptr
				);

			// RtgのExecute() で構築して生成したComandListのSequenceをGPUへSubmitするヘルパー関数.
			static void SubmitCommand(
				rhi::GraphicsCommandQueueDep& graphics_queue, rhi::ComputeCommandQueueDep& compute_queue,
				std::vector<RtgSubmitCommandSequenceElem>& graphics_commands, std::vector<RtgSubmitCommandSequenceElem>& compute_commands);
			
		public:
			// NodeのHandleに対して割り当て済みリソースを取得する.
			// Graphシステム側で必要なBarrierコマンドを発効するため基本的にNode実装側ではBarrierコマンドは不要.
			RtgAllocatedResourceInfo GetAllocatedResource(const ITaskNode* node, RtgResourceHandle res_handle) const;
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
			uint32_t compiled_order_id_ = {};
			
			// -------------------------------------------------------------------------------------------
			static constexpr  int k_base_height = 1080;
			int res_base_height_ = k_base_height;
			int res_base_width_ = static_cast<int>( static_cast<float>(k_base_height) * 16.0f/9.0f);
			
			std::vector<ITaskNode*> node_sequence_{};// Graph構成ノードシーケンス. 生成順がGPU実行順で, AsyncComputeもFenceで同期をする以外は同様.
			std::unordered_map<RtgResourceHandleKeyType, RtgResourceDesc2D> handle_2_desc_{};// Handleからその定義のMap.
			
			struct NodeHandleUsageInfo
			{
				RtgResourceHandle			handle{};// あるNodeからどのようなHandleで利用されたか.
				ACCESS_TYPE				access{};// あるNodeから上記Handleがどのアクセスタイプで利用されたか.
			};
			std::unordered_map<const ITaskNode*, std::vector<NodeHandleUsageInfo>> node_handle_usage_list_{};// Node毎のResourceHandleアクセス情報をまとめるMap.
			// ------------------------------------------------------------------------------------------------------------------------------------------------------
			// Importリソース用.
			std::vector<ExternalResourceInfo> imported_resource_ = {};
			std::unordered_map<RtgResourceHandleKeyType, int> imported_handle_2_index_ = {};

			// ImportしたSwapchainは何かとアクセスするためHandle保持.
			RtgResourceHandle	handle_imported_swapchain_ = {};
			// ------------------------------------------------------------------------------------------------------------------------------------------------------
			
			// 次フレームまで寿命を延長するハンドル.
			std::unordered_map<RtgResourceHandleKeyType, int> propagate_next_handle_ = {};
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
				std::unordered_map<RtgResourceHandleKeyType, int> handle_2_compiled_index_ = {};
				// NodeSequenceの順序に沿ったHandle配列.
				std::vector<RtgResourceHandle>						compiled_index_handle_ = {};

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
					rhi::EResourceState prev_ = {};
					rhi::EResourceState curr_ = {};
				};
				// NodeのHandle毎のリソース状態遷移.
				std::unordered_map<const ITaskNode*, std::unordered_map<RtgResourceHandleKeyType, NodeHandleState>> node_handle_state_ = {};
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
			RtgResourceHandle RegisterExternalResourceCommon(
				rhi::RefTextureDep tex, rhi::RhiRef<rhi::SwapChainDep> swapchain, rhi::RefRtvDep rtv, rhi::RefDsvDep dsv, rhi::RefSrvDep srv, rhi::RefUavDep uav,
				rhi::EResourceState curr_state, rhi::EResourceState nesesary_end_state);
		};



	// ------------------------------------------------------------------------------------------------------------------------------------------------------
		// Rtg core system.
		// RenderTaskGraphBuilderのCompileやそれらが利用するリソースの永続的なプール管理.
		class RenderTaskGraphManager
		{
			friend class RenderTaskGraphBuilder;
			
		public:
			RenderTaskGraphManager() = default;
			~RenderTaskGraphManager();
		public:
			// 初期化.
			bool Init(rhi::DeviceDep* p_device, int job_thread_count = 8);

			//	フレーム開始通知. Game-Render同期中に呼び出す.
			//		内部リソースプールの中で一定フレームアクセスされていないものを破棄するなどの処理.
			void BeginFrame();

			// builderをCompileしてリソース割当を確定する.
			//	Compileしたbuilderは必ずExecuteする必要がある.
			//	また, 複数のbuilderをCompileした場合はCompileした順序でExecuteが必要(確定したリソースの状態遷移コマンド実行を正しい順序で実行するために).
			bool Compile(RenderTaskGraphBuilder& builder);
			
		public:
			// Builderが利用するCommandListをPoolから取得(Graphics).
			void GetNewFrameCommandList(rhi::GraphicsCommandListDep*& out_ref)
			{
				commandlist_pool_.GetFrameCommandList(out_ref);
			}
			// Builderが利用するCommandListをPoolから取得(Compute).
			void GetNewFrameCommandList(rhi::ComputeCommandListDep*& out_ref)
			{
				commandlist_pool_.GetFrameCommandList(out_ref);
			}

		public:
			rhi::DeviceDep* GetDevice()
			{
				return p_device_;
			}
			
			thread::JobSystem* GetJobSystem()
			{
				return &job_system_;
			}
			
		private:
			// Poolからリソース検索または新規生成. 戻り値は実リソースID.
			//	検索用のリソース定義keyと, アクセス期間外の再利用のためのアクセスステージ情報を引数に取る.
			//	access_stage : リソース再利用を有効にしてアクセス開始ステージを指定する, nullptrの場合はリソース再利用をしない.
			int GetOrCreateResourceFromPool(ResourceSearchKey key, const TaskStage* p_access_stage_for_reuse = nullptr);
			// プールリソースの最終アクセス情報を書き換え. BuilderのCompile時の一時的な用途.
			void SetInternalResouceLastAccess(int resource_id, TaskStage last_access_stage);
			// 割り当て済みリソース番号から内部リソースポインタ取得.
			InternalResourceInstanceInfo* GetInternalResourcePtr(int resource_id);

			// BuilderからハンドルとリソースIDを紐づけて次のフレームへ伝搬する.
			void PropagateResourceToNextFrame(RtgResourceHandle handle, int resource_id);
			// 伝搬されたハンドルに紐付けられたリソースIDを検索.
			int FindPropagatedResourceId(RtgResourceHandle handle);
			
		private:
			rhi::DeviceDep* p_device_ = nullptr;

			// 同一Manager下のBuilderのCompileは排他処理.
			std::mutex	compile_mutex_ = {};
			
			// Compileで割り当てられるリソースのPool.
			std::vector<InternalResourceInstanceInfo> internal_resource_pool_ = {};
			
			// 次のフレームへ伝搬するハンドルとリソースIDのMap.
			std::unordered_map<RtgResourceHandleKeyType, int> propagate_next_handle_[2] = {};
			// 次のフレームへ伝搬するハンドル登録用FlipIndex. 前回フレームから伝搬されたハンドルは 1-flip_propagate_next_handle_next_ のMapが対応.
			int flip_propagate_next_handle_next_ = 0;
			std::unordered_map<RtgResourceHandleKeyType, int> propagate_next_handle_temporal_ = {};
			
			pool::CommandListPool commandlist_pool_ = {};
		private:
			// JobSystem. 専用に内部で持っているが要検討.
			thread::JobSystem	job_system_;
		private:
			// ユニークなハンドルIDを取得.
			//	TODO. 64bit.
			static uint32_t GetNewHandleId();
			static uint32_t	s_res_handle_id_counter_;// リソースハンドルユニークID. 生成のたびに加算しユニーク識別.
		};
		
		// ------------------------------------------------------------------------------------------------------------------------------------------------------
	}
}