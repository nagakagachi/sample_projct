#include "rhi/d3d12/gpu_scope_profiler.d3d12.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <map>
#include <mutex>
#include <numeric>
#include <string>
#include <unordered_map>

#include "rhi/d3d12/device.d3d12.h"
#include "rhi/d3d12/resource_view.d3d12.h"

namespace ngl::rhi
{
	namespace
	{
		// 固定リング: QueryHeap/Readbackを再利用して毎フレームの割り当てコストを抑える.
		static constexpr u32 k_gpu_profiler_buffered_frame_count = 8u;
		static constexpr u32 k_gpu_profiler_max_scope_count_per_frame = 2048u;
		static constexpr u32 k_gpu_profiler_max_query_count_per_frame = k_gpu_profiler_max_scope_count_per_frame * 2u;
		static constexpr u32 k_gpu_profiler_total_query_count = k_gpu_profiler_buffered_frame_count * k_gpu_profiler_max_query_count_per_frame;
		static constexpr u32 k_gpu_profiler_invalid_index = 0xffffffffu;
		static constexpr size_t k_gpu_profiler_history_size = 120u;

		static u32 CalcGpuScopeId(const char* label)
		{
			if (!label)
			{
				return 0u;
			}
			u32 hash = 2166136261u;
			for (const unsigned char* p = reinterpret_cast<const unsigned char*>(label); *p != 0; ++p)
			{
				hash ^= static_cast<u32>(*p);
				hash *= 16777619u;
			}
			return hash;
		}
	}

	class GpuScopeProfilerDep::Impl final : public IGpuScopeProfiler
	{
	public:
		bool Initialize(DeviceDep* p_device, GraphicsCommandQueueDep* p_graphics_queue)
		{
			if (!p_device || !p_graphics_queue)
			{
				return false;
			}

			p_device_ = p_device;

			D3D12_QUERY_HEAP_DESC query_heap_desc = {};
			query_heap_desc.Count = k_gpu_profiler_total_query_count;
			query_heap_desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
			query_heap_desc.NodeMask = 0;
			if (FAILED(p_device->GetD3D12Device()->CreateQueryHeap(&query_heap_desc, IID_PPV_ARGS(&query_heap_))))
			{
				std::cout << "[ERROR] Create QueryHeap for GPU profiler" << std::endl;
				return false;
			}

			readback_buffer_.Reset(new BufferDep());
			BufferDep::Desc readback_desc = {};
			readback_desc.element_byte_size = sizeof(u64);
			readback_desc.element_count = k_gpu_profiler_total_query_count;
			readback_desc.bind_flag = 0;
			readback_desc.heap_type = EResourceHeapType::Readback;
			readback_desc.initial_state = EResourceState::CopyDst;
			if (!readback_buffer_->Initialize(p_device, readback_desc, "GpuScopeProfiler_Readback"))
			{
				std::cout << "[ERROR] Create Readback buffer for GPU profiler" << std::endl;
				return false;
			}

			UINT64 timestamp_frequency = 0;
			if (FAILED(p_graphics_queue->GetD3D12CommandQueue()->GetTimestampFrequency(&timestamp_frequency)) || timestamp_frequency == 0)
			{
				std::cout << "[ERROR] GetTimestampFrequency for GPU profiler" << std::endl;
				return false;
			}
			timestamp_to_millisec_ = 1000.0 / static_cast<double>(timestamp_frequency);

			for (u32 i = 0; i < k_gpu_profiler_buffered_frame_count; ++i)
			{
				auto& slot = frame_slots_[i];
				slot.query_base = i * k_gpu_profiler_max_query_count_per_frame;
				slot.sample_base = i * k_gpu_profiler_max_scope_count_per_frame;
				slot.frame_id = 0;
				slot.submitted = false;
				slot.collected = true;
			}
			frame_samples_.resize(k_gpu_profiler_buffered_frame_count * k_gpu_profiler_max_scope_count_per_frame);
			return true;
		}

		void Finalize()
		{
			std::scoped_lock lock(mutex_);
			frame_samples_.clear();
			scope_stats_.clear();
			for (auto& e : command_list_context_prefix_by_slot_)
			{
				e.clear();
			}
			query_heap_.Reset();
			readback_buffer_.Reset();
			p_device_ = nullptr;
			current_slot_index_ = k_gpu_profiler_invalid_index;
			current_frame_id_ = 0;
		}

		void BeginFrame(u64 frame_id)
		{
			std::scoped_lock lock(mutex_);
			current_frame_id_ = frame_id;
			current_slot_index_ = static_cast<u32>(frame_id % k_gpu_profiler_buffered_frame_count);
			auto& slot = frame_slots_[current_slot_index_];
			slot.frame_id = frame_id;
			slot.query_count = 0;
			slot.sample_count = 0;
			slot.submitted = false;
			slot.collected = false;
			command_list_context_prefix_by_slot_[current_slot_index_].clear();
		}

		void ResolveCurrentFrame(GraphicsCommandListDep* p_command_list)
		{
			std::scoped_lock lock(mutex_);
			if (!p_command_list || current_slot_index_ == k_gpu_profiler_invalid_index || !query_heap_ || readback_buffer_.Get() == nullptr)
			{
				return;
			}

			auto& slot = frame_slots_[current_slot_index_];
			if (slot.submitted)
			{
				return;
			}
			if (slot.query_count > 0u)
			{
				p_command_list->FlushPendingBarriers();
				p_command_list->GetD3D12GraphicsCommandList()->ResolveQueryData(
					query_heap_.Get(),
					D3D12_QUERY_TYPE_TIMESTAMP,
					slot.query_base,
					slot.query_count,
					readback_buffer_->GetD3D12Resource(),
					static_cast<u64>(slot.query_base) * sizeof(u64));
			}
			slot.submitted = true;
		}

		void CollectCompleted(u64 completed_frame_id)
		{
			std::scoped_lock lock(mutex_);
			if (readback_buffer_.Get() == nullptr)
			{
				return;
			}
			auto* mapped_timestamp = readback_buffer_->MapAs<u64>();
			if (!mapped_timestamp)
			{
				return;
			}

			for (u32 slot_index = 0; slot_index < k_gpu_profiler_buffered_frame_count; ++slot_index)
			{
				auto& slot = frame_slots_[slot_index];
				if (!slot.submitted || slot.collected || slot.frame_id > completed_frame_id)
				{
					continue;
				}

				struct ResolvedSample
				{
					std::string label{};
					double duration_ms = 0.0;
					u64 begin_tick = 0;
					u32 begin_query = k_gpu_profiler_invalid_index;
				};
				std::vector<ResolvedSample> resolved_samples = {};
				resolved_samples.reserve(slot.sample_count);
				// 同一フレームslotで事前登録された「CommandList -> RTG親ラベル」対応表.
				const auto& command_list_context_prefix = command_list_context_prefix_by_slot_[slot_index];
				for (u32 local_sample_index = 0; local_sample_index < slot.sample_count; ++local_sample_index)
				{
					const auto& sample = frame_samples_[slot.sample_base + local_sample_index];
					if (sample.begin_query == k_gpu_profiler_invalid_index || sample.end_query == k_gpu_profiler_invalid_index)
					{
						continue;
					}

					const u64 begin_tick = mapped_timestamp[sample.begin_query];
					const u64 end_tick = mapped_timestamp[sample.end_query];
					if (end_tick < begin_tick)
					{
						continue;
					}
					ResolvedSample resolved = {};
					const auto context_it = command_list_context_prefix.find(sample.stack_owner_command_list);
					if (context_it != command_list_context_prefix.end())
					{
						// 例: Graph_0_Gfx/DepthPass/... のように親コンテキストを先頭へ合成.
						resolved.label = context_it->second;
						resolved.label += "/";
						resolved.label += sample.label;
					}
					else
					{
						resolved.label = sample.label;
					}
					resolved.duration_ms = static_cast<double>(end_tick - begin_tick) * timestamp_to_millisec_;
					resolved.begin_tick = begin_tick;
					resolved.begin_query = sample.begin_query;
					resolved_samples.push_back(resolved);
				}

				if (!resolved_samples.empty())
				{
					std::unordered_map<std::string, u32> label_count = {};
					label_count.reserve(resolved_samples.size());
					for (const auto& sample : resolved_samples)
					{
						label_count[sample.label] += 1u;
					}

					std::sort(resolved_samples.begin(), resolved_samples.end(), [](const ResolvedSample& a, const ResolvedSample& b)
					{
						if (a.begin_tick != b.begin_tick)
						{
							return a.begin_tick < b.begin_tick;
						}
						return a.begin_query < b.begin_query;
					});

					std::unordered_map<std::string, u32> label_instance_counter = {};
					label_instance_counter.reserve(label_count.size());
					for (const auto& resolved : resolved_samples)
					{
						std::string unique_label = resolved.label;
						const auto count_it = label_count.find(resolved.label);
						if (count_it != label_count.end() && count_it->second > 1u)
						{
							const u32 instance_index = label_instance_counter[resolved.label]++;
							unique_label += "[";
							unique_label += std::to_string(instance_index);
							unique_label += "]";
						}

						UpdateScopeStat(
							CalcGpuScopeId(unique_label.c_str()),
							unique_label.c_str(),
							resolved.duration_ms,
							slot.frame_id,
							resolved.begin_tick);
					}
				}

				slot.collected = true;
			}

			readback_buffer_->Unmap();
		}

		bool TryGetLatestByLabel(const char* label, GpuScopeStatLatestDep& out_stat) const
		{
			if (!label)
			{
				return false;
			}
			std::scoped_lock lock(mutex_);
			const u32 scope_id = CalcGpuScopeId(label);
			const auto it = scope_stats_.find(scope_id);
			if (it == scope_stats_.end() || it->second.label != label)
			{
				return false;
			}
			out_stat = it->second.latest;
			return out_stat.valid;
		}

		void EnumerateLatest(const std::function<void(const GpuScopeStatEntryDep&)>& fn) const
		{
			if (!fn)
			{
				return;
			}
			std::scoped_lock lock(mutex_);
			for (const auto& [scope_id, scope_stat] : scope_stats_)
			{
				if (!scope_stat.latest.valid)
				{
					continue;
				}
				GpuScopeStatEntryDep e = {};
				e.label = scope_stat.label.c_str();
				e.stat = scope_stat.latest;
				fn(e);
			}
		}

		void RegisterCommandListContextPrefixForCurrentFrame(CommandListBaseDep* p_command_list, const char* prefix)
		{
			if (!p_command_list || !prefix || prefix[0] == '\0')
			{
				return;
			}
			std::scoped_lock lock(mutex_);
			if (current_slot_index_ == k_gpu_profiler_invalid_index)
			{
				return;
			}
			command_list_context_prefix_by_slot_[current_slot_index_][p_command_list] = prefix;
		}

		void BeginScope(CommandListBaseDep* p_command_list, const char* label, GpuProfileScopeToken& out_token) override
		{
			out_token.frame_local_scope_index = k_gpu_profiler_invalid_index;
			if (!p_command_list || !label)
			{
				return;
			}
			if (p_command_list->GetDesc().type != D3D12_COMMAND_LIST_TYPE_DIRECT)
			{
				return;
			}

			std::scoped_lock lock(mutex_);
			if (current_slot_index_ == k_gpu_profiler_invalid_index)
			{
				return;
			}
			auto& slot = frame_slots_[current_slot_index_];
			if (slot.sample_count >= k_gpu_profiler_max_scope_count_per_frame || (slot.query_count + 1u) > k_gpu_profiler_max_query_count_per_frame)
			{
				return;
			}

			const u32 sample_absolute_index = slot.sample_base + slot.sample_count;
			auto& sample = frame_samples_[sample_absolute_index];
			const std::string full_path_label = BuildFullPathLabelForBegin(p_command_list, label);
			sample.label = full_path_label;
			sample.stack_owner_command_list = p_command_list;
			sample.begin_query = slot.query_base + slot.query_count;
			sample.end_query = k_gpu_profiler_invalid_index;

			slot.sample_count += 1u;
			slot.query_count += 1u;

			p_command_list->FlushPendingBarriers();
			p_command_list->GetD3D12GraphicsCommandList()->EndQuery(query_heap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, sample.begin_query);
			out_token.frame_local_scope_index = sample_absolute_index;
		}

		void EndScope(CommandListBaseDep* p_command_list, const GpuProfileScopeToken& token) override
		{
			if (!p_command_list)
			{
				return;
			}
			if (p_command_list->GetDesc().type != D3D12_COMMAND_LIST_TYPE_DIRECT)
			{
				return;
			}

			std::scoped_lock lock(mutex_);
			if (token.frame_local_scope_index == k_gpu_profiler_invalid_index)
			{
				return;
			}
			if (token.frame_local_scope_index >= frame_samples_.size())
			{
				return;
			}
			auto& sample = frame_samples_[token.frame_local_scope_index];
			PopScopeLabelStack(sample.stack_owner_command_list);

			if (current_slot_index_ == k_gpu_profiler_invalid_index)
			{
				return;
			}
			auto& slot = frame_slots_[current_slot_index_];
			if (slot.query_count >= k_gpu_profiler_max_query_count_per_frame)
			{
				return;
			}

			if (sample.end_query != k_gpu_profiler_invalid_index)
			{
				return;
			}

			sample.end_query = slot.query_base + slot.query_count;
			slot.query_count += 1u;

			p_command_list->FlushPendingBarriers();
			p_command_list->GetD3D12GraphicsCommandList()->EndQuery(query_heap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, sample.end_query);
		}

	private:
		struct FrameSample
		{
			std::string label{};
			CommandListBaseDep* stack_owner_command_list = nullptr;
			u32 begin_query = k_gpu_profiler_invalid_index;
			u32 end_query = k_gpu_profiler_invalid_index;
		};
		struct FrameSlot
		{
			u64 frame_id = 0;
			u32 query_base = 0;
			u32 query_count = 0;
			u32 sample_base = 0;
			u32 sample_count = 0;
			bool submitted = false;
			bool collected = true;
		};
		struct ScopeStatInternal
		{
			std::string label{};
			std::deque<double> history_ms{};
			GpuScopeStatLatestDep latest{};
		};

		void UpdateScopeStat(u32 scope_id, const char* label, double duration_ms, u64 frame_id, u64 begin_tick)
		{
			auto& scope_stat = scope_stats_[scope_id];
			if (scope_stat.label.empty() && label)
			{
				scope_stat.label = label;
			}
			scope_stat.history_ms.push_back(duration_ms);
			while (scope_stat.history_ms.size() > k_gpu_profiler_history_size)
			{
				scope_stat.history_ms.pop_front();
			}

			GpuScopeStatLatestDep latest = {};
			latest.valid = true;
			latest.last_ms = duration_ms;
			latest.frame_id = frame_id;
			latest.gpu_begin_tick = begin_tick;
			if (!scope_stat.history_ms.empty())
			{
				const double sum = std::accumulate(scope_stat.history_ms.begin(), scope_stat.history_ms.end(), 0.0);
				latest.avg_ms = sum / static_cast<double>(scope_stat.history_ms.size());
				latest.max_ms = *std::max_element(scope_stat.history_ms.begin(), scope_stat.history_ms.end());

				std::vector<double> sorted_hist(scope_stat.history_ms.begin(), scope_stat.history_ms.end());
				std::sort(sorted_hist.begin(), sorted_hist.end());
				const double p95_position = std::ceil(static_cast<double>(sorted_hist.size()) * 0.95) - 1.0;
				const size_t p95_index = static_cast<size_t>(std::clamp(p95_position, 0.0, static_cast<double>(sorted_hist.size() - 1)));
				latest.p95_ms = sorted_hist[p95_index];
			}
			scope_stat.latest = latest;
		}

		std::string BuildFullPathLabelForBegin(CommandListBaseDep* p_command_list, const char* label)
		{
			auto& label_stack = command_list_scope_stack_[p_command_list];
			label_stack.emplace_back(label ? label : "");
			if (label_stack.empty())
			{
				return std::string(label ? label : "");
			}

			std::string full_path = label_stack[0];
			for (size_t i = 1; i < label_stack.size(); ++i)
			{
				full_path += "/";
				full_path += label_stack[i];
			}
			return full_path;
		}

		void PopScopeLabelStack(CommandListBaseDep* p_command_list)
		{
			auto it = command_list_scope_stack_.find(p_command_list);
			if (it == command_list_scope_stack_.end())
			{
				return;
			}
			if (!it->second.empty())
			{
				it->second.pop_back();
			}
			if (it->second.empty())
			{
				command_list_scope_stack_.erase(it);
			}
		}

	private:
		DeviceDep* p_device_ = nullptr;
		Microsoft::WRL::ComPtr<ID3D12QueryHeap> query_heap_{};
		RefBufferDep readback_buffer_ = {};
		double timestamp_to_millisec_ = 0.0;
		u64 current_frame_id_ = 0;
		u32 current_slot_index_ = k_gpu_profiler_invalid_index;
		std::array<FrameSlot, k_gpu_profiler_buffered_frame_count> frame_slots_ = {};
		std::vector<FrameSample> frame_samples_ = {};
		std::unordered_map<u32, ScopeStatInternal> scope_stats_ = {};
		std::unordered_map<CommandListBaseDep*, std::vector<std::string>> command_list_scope_stack_ = {};
		// frame slotごとに、RTG submit時に登録した親コンテキストを保持する.
		std::array<std::unordered_map<CommandListBaseDep*, std::string>, k_gpu_profiler_buffered_frame_count> command_list_context_prefix_by_slot_ = {};
		mutable std::mutex mutex_{};
	};

	GpuScopeProfilerDep::GpuScopeProfilerDep()
		: p_impl_(std::make_unique<Impl>())
	{
	}

	GpuScopeProfilerDep::~GpuScopeProfilerDep() = default;

	bool GpuScopeProfilerDep::Initialize(DeviceDep* p_device, GraphicsCommandQueueDep* p_graphics_queue)
	{
		return p_impl_ && p_impl_->Initialize(p_device, p_graphics_queue);
	}

	void GpuScopeProfilerDep::Finalize()
	{
		if (p_impl_)
		{
			p_impl_->Finalize();
		}
	}

	void GpuScopeProfilerDep::BeginFrame(u64 frame_id)
	{
		if (p_impl_)
		{
			p_impl_->BeginFrame(frame_id);
		}
	}

	void GpuScopeProfilerDep::ResolveCurrentFrame(GraphicsCommandListDep* p_command_list)
	{
		if (p_impl_)
		{
			p_impl_->ResolveCurrentFrame(p_command_list);
		}
	}

	void GpuScopeProfilerDep::CollectCompleted(u64 completed_frame_id)
	{
		if (p_impl_)
		{
			p_impl_->CollectCompleted(completed_frame_id);
		}
	}

	bool GpuScopeProfilerDep::TryGetLatestByLabel(const char* label, GpuScopeStatLatestDep& out_stat) const
	{
		return p_impl_ && p_impl_->TryGetLatestByLabel(label, out_stat);
	}

	void GpuScopeProfilerDep::EnumerateLatest(const std::function<void(const GpuScopeStatEntryDep&)>& fn) const
	{
		if (p_impl_)
		{
			p_impl_->EnumerateLatest(fn);
		}
	}

	void GpuScopeProfilerDep::BeginScope(CommandListBaseDep* p_command_list, const char* label, GpuProfileScopeToken& out_token)
	{
		if (p_impl_)
		{
			p_impl_->BeginScope(p_command_list, label, out_token);
		}
	}

	void GpuScopeProfilerDep::RegisterCommandListContextPrefixForCurrentFrame(CommandListBaseDep* p_command_list, const char* prefix)
	{
		if (p_impl_)
		{
			p_impl_->RegisterCommandListContextPrefixForCurrentFrame(p_command_list, prefix);
		}
	}

	void GpuScopeProfilerDep::EndScope(CommandListBaseDep* p_command_list, const GpuProfileScopeToken& token)
	{
		if (p_impl_)
		{
			p_impl_->EndScope(p_command_list, token);
		}
	}
}
