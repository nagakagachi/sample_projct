﻿#pragma once

// for mbstowcs, wcstombs
#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <vector>
#include <unordered_map>
#include <memory>

#include "ngl/platform/win/window.win.h"

#include "ngl/rhi/rhi.h"
#include "ngl/rhi/rhi_object_garbage_collect.h"

#include "rhi_util.d3d12.h"
#include "descriptor.d3d12.h"



namespace ngl
{
	namespace rhi
	{
		class PersistentDescriptorAllocator;
		struct PersistentDescriptorInfo;

		class DynamicDescriptorManager;
		class FrameDescriptorHeapPagePool;
		
		class DescriptorSetDep;

		class SwapChainDep;
		class RenderTargetViewDep;
		class FenceDep;

		class GraphicsCommandListDep;
		class ComputeCommandListDep;
		class GraphicsCommandQueueDep;
		class ComputeCommandQueueDep;

		class DeviceDep;


		namespace helper
		{
			bool SerializeAndCreateRootSignature(DeviceDep* p_device, const D3D12_ROOT_SIGNATURE_DESC& desc, CComPtr<ID3D12RootSignature>& out_root_signature);

			bool SerializeAndCreateRootSignature(CComPtr<ID3D12RootSignature>& out_root_signature, DeviceDep* p_device, D3D12_ROOT_PARAMETER* p_param_array, uint32_t num_param, D3D12_ROOT_SIGNATURE_FLAGS flag = D3D12_ROOT_SIGNATURE_FLAGS::D3D12_ROOT_SIGNATURE_FLAG_NONE);
			bool SerializeAndCreateLocalRootSignature(CComPtr<ID3D12RootSignature>& out_root_signature, DeviceDep* p_device, D3D12_ROOT_PARAMETER* p_param_array, uint32_t num_param, D3D12_ROOT_SIGNATURE_FLAGS flag = D3D12_ROOT_SIGNATURE_FLAGS::D3D12_ROOT_SIGNATURE_FLAG_NONE);
		}



		// Device
		class DeviceDep : public IDevice
		{
		public:
			using DXGI_FACTORY_TYPE = IDXGIFactory6;

			struct Desc
			{
				// swapchainバッファ数. 根幹のリソースバッファリング数に関わるのでDeviceのDescに設定する.
				u32		swapchain_buffer_count = 3;

				// リソースとペアで生成されるViewを保持するバッファのサイズ
				u32		persistent_descriptor_size	= 500000;
				// フレームで連続Descriptorを確保するためのバッファのサイズ
				u32		frame_descriptor_size		= 500000;
				bool	enable_debug_layer			= false;
			};

			DeviceDep();
			~DeviceDep();

			bool Initialize(ngl::platform::CoreWindow* window, const Desc& desc);
			void Finalize();

			const Desc& GetDesc() const { return desc_; }

			ngl::platform::CoreWindow* GetWindow();

			// DeviceのDxr対応Interfaceを取得.
			ID3D12Device* GetD3D12Device();
			// DeviceのDxr対応Interfaceを取得.
			ID3D12Device5* GetD3D12DeviceForDxr();
			// 
			DXGI_FACTORY_TYPE* GetDxgiFactory();
			// Dxrサポートの取得.
			bool IsSupportDxr() const;

			PersistentDescriptorAllocator* GetPersistentDescriptorAllocator()
			{
				return p_persistent_descriptor_allocator_.get();
			}
			PersistentDescriptorAllocator* GetPersistentSamplerDescriptorAllocator()
			{
				return p_persistent_sampler_descriptor_allocator_.get();
			}
			DynamicDescriptorManager* GeDynamicDescriptorManager()
			{
				return p_dynamic_descriptor_manager_.get();
			}
			FrameDescriptorHeapPagePool* GetFrameDescriptorHeapPagePool()
			{
				return p_frame_descriptor_page_pool_.get();
			}
		public:
			// フレーム関連.
			
			// Appによってフレームの開始同期タイミングで呼び出す関数.
			void ReadyToNewFrame();
			
			// Deviceが管理するグローバルなフレームインデックスを取得.
			u64	 GetDeviceFrameIndex() const { return frame_index_; }

			const u32 GetFrameBufferIndex() const { return buffer_index_; }
		public:
			// RHIオブジェクトガベージコレクト関連.

			// RHIオブジェクトの参照ハンドルの破棄で呼び出されるオブジェクト破棄依頼関数.
			void DestroyRhiObject(IRhiObject* p) override;

		private:
			Desc	desc_ = {};

			ngl::platform::CoreWindow* p_window_ = nullptr;

			// Feature Level.
			D3D_FEATURE_LEVEL device_feature_level_ = {};
			// Raytracing DXR Tier.
			D3D12_RAYTRACING_TIER device_dxr_tier_ = {};
			
			// base device.
			CComPtr<ID3D12Device> p_device_;
			// For Dxr Interface.
			CComPtr<ID3D12Device5> p_device5_;

			CComPtr<DXGI_FACTORY_TYPE> p_factory_;

			// テスト中. フレームインデックス処理用.
			u64 frame_index_ = 0;

			u32	buffer_index_ = 0;

			// リソース用Descriptor確保用( CBV, SRV, UAV 用)
			std::unique_ptr <PersistentDescriptorAllocator>	p_persistent_descriptor_allocator_;

			// 生成したSampler用Descriptor確保用( Sampler 用)
			std::unique_ptr<PersistentDescriptorAllocator>	p_persistent_sampler_descriptor_allocator_;

			// フレームでのDescriptor確保用( CBV, SRV, UAV 用). 巨大な単一Heap管理..
			std::unique_ptr<DynamicDescriptorManager>		p_dynamic_descriptor_manager_;

			// フレームでのDescriptor確保用. こちらはPage単位で拡張していく. CBV,SRV,UAVおよびSamplerすべてで利用可能.
			std::unique_ptr<FrameDescriptorHeapPagePool>		p_frame_descriptor_page_pool_;


			GabageCollector			gb_;
		};



		// Command Queue Base. 基本ロジックのみ.
		class CommandQueueBaseDep : public RhiObjectBase
		{
		public:
			CommandQueueBaseDep();
			virtual ~CommandQueueBaseDep();
			
			// MEMO. ここでCommandQueue生成時に IGIESW .exe found in whitelist: NO というメッセージがVSログに出力される. 意味と副作用は現状不明.
			bool Initialize(DeviceDep* p_device, D3D12_COMMAND_LIST_TYPE type);

			// Fenceに対してSignal発行..
			void Signal(FenceDep* p_fence, ngl::types::u64 fence_value);
			// Fenceに対してSignal発行.
			// 内部でp_fenceのFenceValueでSignalを発行してからFenceValueを加算し, wait対象のFenceValue(加算前野値)を返す.
			ngl::types::u64 SignalAndIncrement(FenceDep* p_fence);
			// FenceでWait. 待機するFenceValueを指定する.
			void Wait(FenceDep* p_fence, ngl::types::u64 wait_value);
			
			ID3D12CommandQueue* GetD3D12CommandQueue();
		protected:
			CComPtr<ID3D12CommandQueue> p_command_queue_;
		};
		
		// Graphics Command Queue.
		class GraphicsCommandQueueDep : public CommandQueueBaseDep
		{
		public:
			GraphicsCommandQueueDep();
			~GraphicsCommandQueueDep();

			// MEMO. ここでCommandQueue生成時に IGIESW .exe found in whitelist: NO というメッセージがVSログに出力される. 意味と副作用は現状不明.
			bool Initialize(DeviceDep* p_device);
			void Finalize();

			void ExecuteCommandLists(unsigned int num_command_list, GraphicsCommandListDep** p_command_lists);
		private:
		};
		
		// Compute Command Queue.
		// for AsyncCompute.
		class ComputeCommandQueueDep : public CommandQueueBaseDep
		{
		public:
			ComputeCommandQueueDep();
			~ComputeCommandQueueDep();

			// MEMO. ここでCommandQueue生成時に IGIESW .exe found in whitelist: NO というメッセージがVSログに出力される. 意味と副作用は現状不明.
			bool Initialize(DeviceDep* p_device);
			void Finalize();

			void ExecuteCommandLists(unsigned int num_command_list, ComputeCommandListDep** p_command_lists);
		private:
		};
		
		
		// フェンス
		class FenceDep : public RhiObjectBase
		{
		public:
			FenceDep();
			~FenceDep();

			bool Initialize(DeviceDep* p_device);
			void Finalize();

			ID3D12Fence* GetD3D12Fence();

			ngl::types::u64 GetFenceValue() const {return fence_value_;}
			
			// Increment
			// return prev FenceValue.
			ngl::types::u64 IncrementFenceValue()
			{
				const auto tmp = fence_value_;
				++fence_value_;
				return tmp;
			}
		private:
			CComPtr<ID3D12Fence> p_fence_;
			ngl::types::u64 fence_value_ = 1;// FenceValue初期値が0であるため意図通りにSignalWaitするために1開始.
		};


		// CommandQueue実行完了待機用オブジェクト.
		class WaitOnFenceSignalDep
		{
		public:
			WaitOnFenceSignalDep();
			~WaitOnFenceSignalDep();

			// 発行したSignalによるFence値が指定した値になるまで待機. プラットフォーム毎に異なる実装. WindowsではEventを利用.
			void Wait(FenceDep* p_fence, ngl::types::u64 complete_fence_value);

		private:
			HANDLE win_event_;
		};


		// スワップチェイン
		class SwapChainDep : public RhiObjectBase
		{
		public:
			using DXGI_SWAPCHAIN_TYPE = IDXGISwapChain4;

			struct Desc
			{
				ResourceFormat		format = ResourceFormat::Format_R10G10B10A2_UNORM;
			};

			SwapChainDep();
			~SwapChainDep();
			bool Initialize(DeviceDep* p_device, GraphicsCommandQueueDep* p_graphics_command_queu, const Desc& desc);
			void Finalize();

			unsigned int GetCurrentBufferIndex() const;

			const Desc& GetDesc() const { return desc_; }
			uint32_t GetWidth() const { return width_; }
			uint32_t GetHeight() const { return height_; }

		public:
			DXGI_SWAPCHAIN_TYPE* GetDxgiSwapChain();
			unsigned int NumResource() const;
			ID3D12Resource* GetD3D12Resource(unsigned int index);

		private:
			Desc	desc_ = {};
			uint32_t width_ = 0;
			uint32_t height_ = 0;

			CComPtr<DXGI_SWAPCHAIN_TYPE> p_swapchain_;

			CComPtr<ID3D12Resource>* p_resources_ = nullptr;
			unsigned int num_resource_ = 0;
		};

	}
}