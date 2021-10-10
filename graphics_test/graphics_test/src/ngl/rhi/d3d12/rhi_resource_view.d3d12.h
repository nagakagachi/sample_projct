#pragma once

#include <iostream>

#include "ngl/rhi/rhi.h"
#include "ngl/rhi/rhi_resource.h"

#include "rhi_util.d3d12.h"
#include "rhi_resource.d3d12.h"
#include "rhi_descriptor.d3d12.h"


#include "ngl/util/types.h"
#include "ngl/util/unique_ptr.h"
#include "ngl/text/hash_text.h"


namespace ngl
{
	namespace rhi
	{
		class DeviceDep;
		class PersistentDescriptorAllocator;
		struct PersistentDescriptorInfo;


		// VertexBufferView
		class VertexBufferViewDep
		{
		public:
			struct Desc
			{
				ngl::u32			dummy;
			};

			VertexBufferViewDep();
			~VertexBufferViewDep();

			bool Initialize(BufferDep* buffer, const Desc& desc);
			void Finalize();

			const D3D12_VERTEX_BUFFER_VIEW& GetView() const
			{
				return view_;
			}

		private:
			D3D12_VERTEX_BUFFER_VIEW	view_ = {};
		};

		// IndexBufferView
		class IndexBufferViewDep
		{
		public:
			struct Desc
			{
				ngl::u32			dummy;
			};

			IndexBufferViewDep();
			~IndexBufferViewDep();

			bool Initialize(BufferDep* buffer, const Desc& desc);
			void Finalize();

			const D3D12_INDEX_BUFFER_VIEW& GetView() const
			{
				return view_;
			}

		private:
			D3D12_INDEX_BUFFER_VIEW	view_ = {};
		};

		// ConstantBufferView
		class ConstantBufferViewDep
		{
		public:
			struct Desc
			{
				ngl::u32			dummy;
			};

			ConstantBufferViewDep();
			~ConstantBufferViewDep();

			bool Initialize(BufferDep* buffer, const Desc& desc);
			void Finalize();

			const PersistentDescriptorInfo& GetView() const
			{
				return view_;
			}

		private:
			PersistentDescriptorInfo	view_ = {};
		};

		// SamplerState
		class SamplerDep
		{
		public:
			struct Desc
			{
				// TODO. あとで自前定義に置き換える.
				D3D12_SAMPLER_DESC	desc;
			};
			SamplerDep();
			~SamplerDep();
			bool Initialize(DeviceDep* p_device, const Desc& desc);
			void Finalize();

			const PersistentDescriptorInfo& GetView() const
			{
				return view_;
			}
		private:
			PersistentDescriptorInfo	view_{};
		};



		// RenderTargetView
		class RenderTargetViewDep
		{
		public:
			RenderTargetViewDep();
			~RenderTargetViewDep();

			// SwapChainからRTV作成.
			bool Initialize(DeviceDep* p_device, SwapChainDep* p_swapchain, unsigned int buffer_index);

			// TODO. Initialize from Texture.

			void Finalize();

			D3D12_CPU_DESCRIPTOR_HANDLE GetD3D12DescriptorHandle() const;
		private:
			// 現状のRTVやDSVはHeapをそれぞれ専有する. 問題があればグローバルな専用Heapから確保する.
			CComPtr<ID3D12DescriptorHeap> p_heap_;
		};


		// ShaderResourceView
		class ShaderResourceViewDep
		{
		public:
			ShaderResourceViewDep();
			~ShaderResourceViewDep();

			// SwapChainからRTV作成.
			bool Initialize(DeviceDep* p_device, TextureDep* p_texture, u32 first_mip, u32 mip_count, u32 first_array, u32 array_size);

			void Finalize();

			const PersistentDescriptorInfo& GetView() const
			{
				return view_;
			}
		private:
			PersistentDescriptorInfo	view_ = {};
		};
	}
}