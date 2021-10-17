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

			bool Initialize(BufferDep* p_buffer, const Desc& desc);
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

			bool Initialize(BufferDep* p_buffer, const Desc& desc);
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

			bool Initialize(BufferDep* p_buffer, const Desc& desc);
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

			// SwapChainから作成.
			bool Initialize(DeviceDep* p_device, SwapChainDep* p_swapchain, unsigned int buffer_index);

			// Textureから生成.
			bool Initialize(DeviceDep* p_device, const TextureDep* p_texture, u32 mip_slice, u32 first_array_slice, u32 array_size);

			void Finalize();

			D3D12_CPU_DESCRIPTOR_HANDLE GetD3D12DescriptorHandle() const
			{
				return p_heap_->GetCPUDescriptorHandleForHeapStart();
			}
		private:
			// 現状のRTVやDSVはHeapをそれぞれ専有する. 問題があればグローバルな専用Heapから確保する.
			CComPtr<ID3D12DescriptorHeap> p_heap_;
		};

		// DepthStencilView
		class DepthStencilViewDep
		{
		public:
			DepthStencilViewDep();
			~DepthStencilViewDep();

			// Textureから生成.
			bool Initialize(DeviceDep* p_device, const TextureDep* p_texture, u32 mip_slice, u32 first_array_slice, u32 array_size);

			void Finalize();

			D3D12_CPU_DESCRIPTOR_HANDLE GetD3D12DescriptorHandle() const
			{
				return p_heap_->GetCPUDescriptorHandleForHeapStart();
			}
		private:
			// 現状のRTVやDSVはHeapをそれぞれ専有する. 問題があればグローバルな専用Heapから確保する.
			CComPtr<ID3D12DescriptorHeap> p_heap_;
		};

		// UnorderedAccessView
		class UnorderedAccessView
		{
		public:
			UnorderedAccessView();
			~UnorderedAccessView();

			// TextureのView.
			bool Initialize(DeviceDep* p_device, const TextureDep* p_texture, u32 mip_slice, u32 first_array_slice, u32 array_size);
			// BufferのStructuredBufferView.
			bool InitializeAsStructured(DeviceDep* p_device, const BufferDep* p_buffer, u32 element_size, u32 element_offset, u32 element_count);
			// BufferのTypedBufferView.
			bool InitializeAsTyped(DeviceDep* p_device, const BufferDep* p_buffer, ResourceFormat format, u32 element_offset, u32 element_count);
			// BufferのByteAddressBufferView.
			bool InitializeAsRaw(DeviceDep* p_device, const BufferDep* p_buffer, u32 element_offset, u32 element_count);

			void Finalize();

			const PersistentDescriptorInfo& GetView() const
			{
				return view_;
			}
		private:
			PersistentDescriptorInfo	view_ = {};
		};

		// ShaderResourceView
		class ShaderResourceViewDep
		{
		public:
			ShaderResourceViewDep();
			~ShaderResourceViewDep();

			// TextureのView.
			bool InitializeAsTexture(DeviceDep* p_device, const TextureDep* p_texture, u32 mip_slice, u32 mip_count, u32 first_array_slice, u32 array_size);
			// BufferのStructuredBufferView.
			bool InitializeAsStructured(DeviceDep* p_device, const BufferDep* p_buffer, u32 element_size, u32 element_offset, u32 element_count);
			// BufferのTypedBufferView.
			bool InitializeAsTyped(DeviceDep* p_device, const BufferDep* p_buffer, ResourceFormat format, u32 element_offset, u32 element_count);
			// BufferのByteAddressBufferView.
			bool InitializeAsRaw(DeviceDep* p_device, const BufferDep* p_buffer, u32 element_offset, u32 element_count);

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