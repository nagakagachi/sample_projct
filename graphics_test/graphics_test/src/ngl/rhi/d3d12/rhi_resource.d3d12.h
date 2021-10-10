#pragma once


#include <iostream>

#include "ngl/rhi/rhi.h"
#include "rhi_util.d3d12.h"
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


		enum class ResourceType : u32
		{
			Buffer,                 ///< Buffer. Can be bound to all shader-stages
			Texture1D,              ///< 1D texture. Can be bound as render-target, shader-resource and UAV
			Texture2D,              ///< 2D texture. Can be bound as render-target, shader-resource and UAV
			Texture3D,              ///< 3D texture. Can be bound as render-target, shader-resource and UAV
			TextureCube,            ///< Texture-cube. Can be bound as render-target, shader-resource and UAV
			Texture2DMultisample,   ///< 2D multi-sampled texture. Can be bound as render-target, shader-resource and UAV
		};

		struct ResourceBindFlag
		{
			static constexpr u32 None				= (1 << 0);
			static constexpr u32 ConstantBuffer		= (1 << 1);
			static constexpr u32 VertexBuffer		= (1 << 2);
			static constexpr u32 IndexBuffer		= (1 << 3);
			static constexpr u32 ShaderResource		= (1 << 4);
			static constexpr u32 UnorderedAccess	= (1 << 5);
			static constexpr u32 RenderTarget		= (1 << 6);
			static constexpr u32 DepthStencil		= (1 << 7);
			static constexpr u32 IndirectArg		= (1 << 8);

			//AccelerationStructure = 0x80000000,  ///< The resource will be bound as an acceleration structure
		};

		// Buffer
		class BufferDep
		{
		public:
			struct Desc
			{
				ngl::u32			element_byte_size = 0;
				ngl::u32			element_count = 0;
				// bitmask of ngl::rhi::ResourceBindFlag.
				u32					bind_flag = 0;
				ResourceHeapType	heap_type = ResourceHeapType::DEFAULT;
				ResourceState		initial_state = ResourceState::GENERAL;
			};

			BufferDep();
			~BufferDep();

			bool Initialize(DeviceDep* p_device, const Desc& desc);
			void Finalize();

			void* Map();
			void Unmap();

			const u32 GetAlignedBufferSize() const { return allocated_byte_size_; }
			const Desc& GetDesc() const { return desc_; }
			DeviceDep* GetParentDevice() { return p_parent_device_; }

			ID3D12Resource* GetD3D12Resource();

		private:
			DeviceDep* p_parent_device_ = nullptr;

			Desc	desc_ = {};
			u32		allocated_byte_size_ = 0;

			void*		map_ptr_ = nullptr;

			CComPtr<ID3D12Resource> resource_;
		};


		// Texture
		// TODO. Array対応, Cubemap対応.
		class TextureDep
		{
		public:
			enum class Dimension
			{
				Texture1D,
				Texture2D,
				Texture3D,
			};

			struct Desc
			{
				ResourceFormat		format = ResourceFormat::NGL_FORMAT_UNKNOWN;
				ngl::u32			width = 1;
				ngl::u32			height = 1;
				// Depth of Texture3D or ArraySize of TextureArray
				ngl::u32			depth = 1;
				ngl::u32			mip_level = 1;
				ngl::u32			sample_count = 1;

				Dimension			dimension = Dimension::Texture2D;

				// bitmask of ngl::rhi::ResourceBindFlag.
				u32					bind_flag = 0;
				ResourceHeapType	heap_type = ResourceHeapType::DEFAULT;
				ResourceState		initial_state = ResourceState::GENERAL;
			};

			TextureDep();
			~TextureDep();

			bool Initialize(DeviceDep* p_device, const Desc& desc);
			void Finalize();

			void* Map();
			void Unmap();

			const u32 GetAlignedBufferSize() const { return allocated_byte_size_; }
			const Desc& GetDesc() const { return desc_; }
			DeviceDep* GetParentDevice() { return p_parent_device_; }

			ID3D12Resource* GetD3D12Resource();

		private:
			DeviceDep* p_parent_device_ = nullptr;

			Desc	desc_ = {};
			u32		allocated_byte_size_ = 0;

			void* map_ptr_ = nullptr;

			CComPtr<ID3D12Resource> resource_;
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
		// TODO. 現状はView一つに付きHeap一つを確保している. 最終的にはHeapプールから確保するようにしたい.
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

		class ShaderResourceViewDep
		{
		public:
			ShaderResourceViewDep();
			~ShaderResourceViewDep();

			// SwapChainからRTV作成.
			bool Initialize(DeviceDep* p_device, TextureDep* p_texture, u32 firstMip, u32 mipCount, u32 firstArray, u32 arraySize);

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