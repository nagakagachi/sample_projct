#pragma once

#include <functional>
#include <memory>

#include "util/types.h"
#include "rhi/d3d12/command_list.d3d12.h"

namespace ngl::rhi
{
	struct GpuScopeStatLatestDep
	{
		bool valid = false;
		double last_ms = 0.0;
		double avg_ms = 0.0;
		double p95_ms = 0.0;
		double max_ms = 0.0;
		u64 frame_id = 0;
		u64 gpu_begin_tick = 0;
	};

	struct GpuScopeStatEntryDep
	{
		const char* label = nullptr;
		GpuScopeStatLatestDep stat = {};
	};

	// D3D12 timestamp query ベースの GPU スコーププロファイラ実装.
	class GpuScopeProfilerDep final : public IGpuScopeProfiler
	{
	public:
		GpuScopeProfilerDep();
		~GpuScopeProfilerDep();

		bool Initialize(DeviceDep* p_device, GraphicsCommandQueueDep* p_graphics_queue);
		void Finalize();

		void BeginFrame(u64 frame_id);
		void ResolveCurrentFrame(GraphicsCommandListDep* p_command_list);
		void CollectCompleted(u64 completed_frame_id);

		bool TryGetLatestByLabel(const char* label, GpuScopeStatLatestDep& out_stat) const;
		void EnumerateLatest(const std::function<void(const GpuScopeStatEntryDep&)>& fn) const;
		// 現フレームの特定CommandListにRTGなどの親コンテキストプレフィックスを関連付ける.
		void RegisterCommandListContextPrefixForCurrentFrame(CommandListBaseDep* p_command_list, const char* prefix);

		void BeginScope(CommandListBaseDep* p_command_list, const char* label, GpuProfileScopeToken& out_token) override;
		void EndScope(CommandListBaseDep* p_command_list, const GpuProfileScopeToken& token) override;

	private:
		class Impl;
		std::unique_ptr<Impl> p_impl_{};
	};
}
