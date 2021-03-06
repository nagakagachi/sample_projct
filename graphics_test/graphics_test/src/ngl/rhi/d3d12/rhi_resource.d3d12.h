﻿#pragma once


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

		enum class BufferUsage : int
		{
			ConstantBuffer	= (1 << 0),
			VertexBuffer	= (1 << 1),
			IndexBuffer		= (1 << 2),
			ShaderResource	= (1 << 3),

			RenderTarget	= (1 << 4),
			DepthStencil	= (1 << 5),

			Max
		};

		// Buffer
		class BufferDep
		{
		public:
			struct Desc
			{
				ngl::u32			element_byte_size = 0;
				ngl::u32			element_count = 0;
				u32					usage_flag = 0;	// BufferUsage bitmask.
				ResourceHeapType	heap_type = ResourceHeapType::DEFAULT;
				ResourceState		initial_state = ResourceState::GENERAL;
				bool				allow_uav = false;
			};

			BufferDep();
			~BufferDep();

			bool Initialize(DeviceDep* p_device, const Desc& desc);
			void Finalize();

			template<typename T = void>
			T* Map()
			{
				// DefaultヒープリソースはMap不可
				if (ResourceHeapType::DEFAULT == desc_.heap_type)
				{
					std::cout << "ERROR: Default Buffer can not Mapped" << std::endl;
					return nullptr;
				}
				if (map_ptr_)
				{
					// Map済みの場合はそのまま返す.
					return reinterpret_cast<T*>(map_ptr_);
				}

				// Readbackバッファ以外の場合はMap時に以前のデータを読み取らないようにZero-Range指定.
				D3D12_RANGE read_range = { 0, 0 };
				if (ResourceHeapType::READBACK == desc_.heap_type)
				{
					read_range = { 0, static_cast<SIZE_T>(desc_.element_byte_size) * static_cast<SIZE_T>(desc_.element_count) };
				}

				if (FAILED(resource_->Map(0, &read_range, &map_ptr_)))
				{
					std::cout << "ERROR: Resouce Map" << std::endl;
					map_ptr_ = nullptr;
					return nullptr;
				}
				return reinterpret_cast<T*>(map_ptr_);
			}

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
			BufferDep* parent_buffer_ = nullptr;

			PersistentDescriptorInfo	view_;
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
			BufferDep* parent_buffer_ = nullptr;

			D3D12_VERTEX_BUFFER_VIEW	view_;
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
			BufferDep* parent_buffer_ = nullptr;

			D3D12_INDEX_BUFFER_VIEW	view_;
		};


		// Texture
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
				ngl::u32			depth = 1;
				ngl::u32			mip_level = 1;
				ngl::u32			sample_count = 1;

				Dimension			dimension = Dimension::Texture2D;

				u32					usage_flag = 0;	// BufferUsage bitmask.
				ResourceHeapType	heap_type = ResourceHeapType::DEFAULT;
				ResourceState		initial_state = ResourceState::GENERAL;
				bool				allow_uav = false;
			};

			TextureDep();
			~TextureDep();

			bool Initialize(DeviceDep* p_device, const Desc& desc);
			void Finalize();

			template<typename T = void>
			T* Map()
			{
				// DefaultヒープリソースはMap不可
				if (ResourceHeapType::DEFAULT == desc_.heap_type)
				{
					std::cout << "ERROR: Default Texture can not Mapped" << std::endl;
					return nullptr;
				}
				if (map_ptr_)
				{
					// Map済みの場合はそのまま返す.
					return reinterpret_cast<T*>(map_ptr_);
				}
				if (FAILED(resource_->Map(0, nullptr, &map_ptr_)))
				{
					std::cout << "ERROR: Resouce Map" << std::endl;
					map_ptr_ = nullptr;
					return nullptr;
				}
				return reinterpret_cast<T*>(map_ptr_);
			}

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
	}
}