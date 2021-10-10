#pragma once


#include <iostream>

#include "ngl/rhi/rhi.h"
#include "ngl/rhi/rhi_resource.h"
#include "rhi_util.d3d12.h"


#include "ngl/util/types.h"
#include "ngl/util/unique_ptr.h"
#include "ngl/text/hash_text.h"

namespace ngl
{
	namespace rhi
	{
		class DeviceDep;

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
			struct Desc
			{
				ResourceFormat		format = ResourceFormat::NGL_FORMAT_UNKNOWN;
				ngl::u32			width = 1;
				ngl::u32			height = 1;
				ngl::u32			depth = 1;
				ngl::u32			mip_count = 1;
				ngl::u32			sample_count = 1;
				ngl::u32			array_size = 1;

				TextureType			type = TextureType::Texture2D;

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





	}
}