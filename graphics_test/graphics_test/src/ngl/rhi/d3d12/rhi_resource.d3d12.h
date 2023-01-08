#pragma once


#include <algorithm>
#include <iostream>
#include <memory>

#include "ngl/rhi/rhi.h"
#include "rhi_util.d3d12.h"
#include "rhi.d3d12.h"


#include "ngl/util/types.h"
#include "ngl/text/hash_text.h"

namespace ngl
{
	namespace rhi
	{
		class DeviceDep;

		// Buffer
		class BufferDep : public RhiObjectBase
		{
		public:
			struct Desc
			{
				ngl::u32			element_byte_size = 0;
				ngl::u32			element_count = 0;
				u32					bind_flag = 0;// bitmask of ngl::rhi::ResourceBindFlag.
				ResourceHeapType	heap_type = ResourceHeapType::Default;
				ResourceState		initial_state = ResourceState::General;
			};

			BufferDep();
			~BufferDep();

			bool Initialize(DeviceDep* p_device, const Desc& desc);
			void Finalize();

			void* Map();
			void Unmap();

			const Desc& GetDesc() const { return desc_; }

			ID3D12Resource* GetD3D12Resource() const;

		public:
			u32 GetBufferSize() const { return allocated_byte_size_; }
			u32 GetElementByteSize() const { return desc_.element_byte_size; }
			u32 getElementCount() const { return desc_.element_count; }

		private:
			Desc	desc_ = {};
			u32		allocated_byte_size_ = 0;

			void*		map_ptr_ = nullptr;

			CComPtr<ID3D12Resource> resource_;
		};


		// Texture
		// TODO. Array対応, Cubemap対応.
		class TextureDep : public RhiObjectBase
		{
		public:
			struct Desc
			{
				ResourceFormat		format = ResourceFormat::Format_UNKNOWN;
				ngl::u32			width = 1;
				ngl::u32			height = 1;
				ngl::u32			depth = 1;
				ngl::u32			mip_count = 1;
				ngl::u32			sample_count = 1;
				ngl::u32			array_size = 1;

				TextureType			type = TextureType::Texture2D;

				u32					bind_flag = 0;// bitmask of ngl::rhi::ResourceBindFlag.
				ResourceHeapType	heap_type = ResourceHeapType::Default;// UploadTextureはD3Dで未対応. Bufferを作ってそこからコピーする.
				ResourceState		initial_state = ResourceState::General;
			};

			TextureDep();
			~TextureDep();

			bool Initialize(DeviceDep* p_device, const Desc& desc);
			void Finalize();

			void* Map();
			void Unmap();

			const Desc& GetDesc() const { return desc_; }

			ID3D12Resource* GetD3D12Resource() const;
		public:
			u32 GetBufferSize() const { return allocated_byte_size_; }

		public:
			/** Get a mip-level width
			*/
			uint32_t GetWidth(uint32_t mipLevel = 0) const { return (mipLevel == 0) || (mipLevel < desc_.mip_count) ? std::max<>(1U, desc_.width >> mipLevel) : 0; }
			/** Get a mip-level height
			*/
			uint32_t GetHeight(uint32_t mipLevel = 0) const { return (mipLevel == 0) || (mipLevel < desc_.mip_count) ? std::max<>(1U, desc_.height >> mipLevel) : 0; }
			/** Get a mip-level depth
			*/
			uint32_t GetDepth(uint32_t mipLevel = 0) const { return (mipLevel == 0) || (mipLevel < desc_.mip_count) ? std::max<>(1U, desc_.depth >> mipLevel) : 0; }
			/** Get the number of mip-levels
			*/
			uint32_t GetMipCount() const { return desc_.mip_count; }
			/** Get the sample count
			*/
			uint32_t GetSampleCount() const { return desc_.sample_count; }
			/** Get the array size
			*/
			uint32_t GetArraySize() const { return desc_.array_size; }
			/** Get the array index of a subresource
			*/
			uint32_t GetSubresourceArraySlice(uint32_t subresource) const { return subresource / desc_.mip_count; }
			/** Get the mip-level of a subresource
			*/
			uint32_t GetSubresourceMipLevel(uint32_t subresource) const { return subresource % desc_.mip_count; }
			/** Get the subresource index
			*/
			uint32_t GetSubresourceIndex(uint32_t arraySlice, uint32_t mipLevel) const { return mipLevel + arraySlice * desc_.mip_count; }
			/** Get the resource format
			*/
			ResourceFormat GetFormat() const { return desc_.format; }
			/** Get the resource bind flag
			*/
			u32 GetBindFlag() const { return desc_.bind_flag; }
			/** Get the resource Type
			*/
			TextureType GetType() const { return desc_.type; }

		private:
			Desc	desc_ = {};
			u32		allocated_byte_size_ = 0;

			void* map_ptr_ = nullptr;

			CComPtr<ID3D12Resource> resource_;
		};





	}
}