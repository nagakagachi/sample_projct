
#include "rhi_resource.d3d12.h"

#include "rhi.d3d12.h"

#include <array>
#include <algorithm>

namespace ngl
{
	namespace rhi
	{
		// -------------------------------------------------------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------------------------------------------------------
		BufferDep::BufferDep()
		{
		}
		BufferDep::~BufferDep()
		{
			Finalize();
		}

		bool BufferDep::Initialize(DeviceDep* p_device, const Desc& desc)
		{
			if (!p_device)
				return false;
			if (0 == desc.usage_flag)
			{
				assert(false);
				return false;
			}

			desc_ = desc;
			p_parent_device_ = p_device;

			if (0 >= desc_.element_byte_size || 0 >= desc_.element_count)
				return false;

			D3D12_HEAP_FLAGS heap_flag = D3D12_HEAP_FLAG_NONE;
			D3D12_RESOURCE_STATES initial_state = ConvertResourceState(desc_.initial_state);
			D3D12_HEAP_PROPERTIES heap_prop = {};
			{
				switch (desc_.heap_type)
				{
				case ResourceHeapType::DEFAULT:
				{
					heap_prop.Type = D3D12_HEAP_TYPE_DEFAULT;
					break;
				}
				case ResourceHeapType::UPLOAD:
				{
					heap_prop.Type = D3D12_HEAP_TYPE_UPLOAD;
					break;
				}
				case ResourceHeapType::READBACK:
				{
					heap_prop.Type = D3D12_HEAP_TYPE_READBACK;
					break;
				}
				default:
				{
					heap_prop.Type = D3D12_HEAP_TYPE_DEFAULT;
				}
				}
				heap_prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
				heap_prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
				heap_prop.VisibleNodeMask = 0;
			}

			D3D12_RESOURCE_FLAGS need_flags = D3D12_RESOURCE_FLAG_NONE;
			// 用途によるアライメント.
			u32 need_alignment = 16;
			if ((int)BufferUsage::ConstantBuffer & desc_.usage_flag)
			{
				need_alignment = static_cast<u32>(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
			}
			if (desc.allow_uav)
			{
				need_alignment = static_cast<u32>(D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT);
				need_flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			}

			// 現状はとりあえずConstantBufferのアラインメントに従ってみる
			// 確保するサイズはDirectX12のConstantBufferサイズAlignmentにしたがう(256)
			const auto minimum_size = (desc_.element_byte_size) * (desc_.element_count);
			allocated_byte_size_ = need_alignment * ((minimum_size + need_alignment - 1) / need_alignment);

			D3D12_RESOURCE_DESC resource_desc = {};
			{
				resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
				resource_desc.Alignment = 0;
				resource_desc.Width = static_cast<UINT64>(allocated_byte_size_);
				resource_desc.Height = 1u;
				resource_desc.DepthOrArraySize = 1u;
				resource_desc.MipLevels = 1u;
				resource_desc.Format = DXGI_FORMAT_UNKNOWN;
				resource_desc.SampleDesc.Count = 1;
				resource_desc.SampleDesc.Quality = 0;
				resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
				resource_desc.Flags = need_flags;
			}

			// パラメータチェック
			if ((D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS & resource_desc.Flags) && (D3D12_HEAP_TYPE_DEFAULT != heap_prop.Type))
			{
				// UAVリソースはDefaultHeap以外許可されない.
				std::cout << "[ERROR] Heap of UAV Resource must be ResourceHeapType::DEFAULT" << std::endl;
				return false;
			}

			/*
				ステート制限
					Upload -> D3D12_RESOURCE_STATE_GENERIC_READ
					Readback -> D3D12_RESOURCE_STATE_COPY_DEST
					テクスチャ配置専用予約バッファ -> D3D12_RESOURCE_STATE_COMMON
			*/
			if (D3D12_HEAP_TYPE_UPLOAD == heap_prop.Type)
			{
				if (D3D12_RESOURCE_STATE_GENERIC_READ != initial_state)
				{
					std::cout << "[ERROR] State of Upload Buffer must be ResourceState::General" << std::endl;
					return false;
				}
			}
			else if (D3D12_HEAP_TYPE_READBACK == heap_prop.Type)
			{
				if (D3D12_RESOURCE_STATE_COPY_DEST != initial_state)
				{
					std::cout << "[ERROR] State of Readback Buffer must be ResourceState::CopyDst" << std::endl;
					return false;
				}
			}

			if (FAILED(p_device->GetD3D12Device()->CreateCommittedResource(&heap_prop, heap_flag, &resource_desc, initial_state, nullptr, IID_PPV_ARGS(&resource_))))
			{
				std::cout << "[ERROR] CreateCommittedResource" << std::endl;
				return false;
			}

			return true;
		}
		void BufferDep::Finalize()
		{
			resource_ = nullptr;
		}
		void BufferDep::Unmap()
		{
			if (!map_ptr_)
				return;

			// Uploadバッファ以外の場合はUnmap時に書き戻さないのでZero-Range指定.
			D3D12_RANGE write_range = {0, 0};
			if (ResourceHeapType::UPLOAD == desc_.heap_type)
			{
				write_range = { 0, static_cast<SIZE_T>(desc_.element_byte_size) * static_cast<SIZE_T>(desc_.element_count) };
			}

			resource_->Unmap(0, &write_range);
			map_ptr_ = nullptr;
		}
		ID3D12Resource* BufferDep::GetD3D12Resource()
		{
			return resource_;
		}
		// -------------------------------------------------------------------------------------------------------------------------------------------------


		// -------------------------------------------------------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------------------------------------------------------
		ConstantBufferViewDep::ConstantBufferViewDep()
		{
		}
		ConstantBufferViewDep::~ConstantBufferViewDep()
		{
			Finalize();
		}
		bool ConstantBufferViewDep::Initialize(BufferDep* buffer, const Desc& desc)
		{
			if (!buffer || !buffer->GetParentDevice())
			{
				assert(false);
				return false;
			}
			if ((int)BufferUsage::ConstantBuffer != buffer->GetDesc().usage_flag)
			{
				assert(false);
				return false;
			}

			parent_buffer_ = buffer;

			auto&& p_device = parent_buffer_->GetParentDevice();

			
			auto&& descriptor_allocator = p_device->GetPersistentDescriptorAllocator();
			view_ = descriptor_allocator->Allocate();

			D3D12_CONSTANT_BUFFER_VIEW_DESC view_desc = {};
			view_desc.BufferLocation = parent_buffer_->GetD3D12Resource()->GetGPUVirtualAddress();
			view_desc.SizeInBytes = parent_buffer_->GetAlignedBufferSize();// アライメント考慮サイズを指定している.
			auto handle = view_.cpu_handle;
			p_device->GetD3D12Device()->CreateConstantBufferView(&view_desc, handle);

			return true;
		}
		void ConstantBufferViewDep::Finalize()
		{
			auto&& descriptor_allocator = parent_buffer_->GetParentDevice()->GetPersistentDescriptorAllocator();
			descriptor_allocator->Deallocate(view_);

			view_ = {};
			parent_buffer_ = nullptr;
		}
		// -------------------------------------------------------------------------------------------------------------------------------------------------


		// -------------------------------------------------------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------------------------------------------------------
		VertexBufferViewDep::VertexBufferViewDep()
		{
		}
		VertexBufferViewDep::~VertexBufferViewDep()
		{
			Finalize();
		}
		bool VertexBufferViewDep::Initialize(BufferDep* buffer, const Desc& desc)
		{
			if (!buffer || !buffer->GetParentDevice())
			{
				assert(false);
				return false;
			}

			const auto& buffer_desc = buffer->GetDesc();
			if ((int)BufferUsage::VertexBuffer != buffer_desc.usage_flag)
			{
				assert(false);
				return false;
			}

			parent_buffer_ = buffer;

			auto&& p_device = parent_buffer_->GetParentDevice();

			view_ = {};
			view_.SizeInBytes = buffer_desc.element_count * buffer_desc.element_byte_size;
			view_.StrideInBytes = buffer_desc.element_byte_size;
			view_.BufferLocation = buffer->GetD3D12Resource()->GetGPUVirtualAddress();

			return true;
		}
		void VertexBufferViewDep::Finalize()
		{
			view_ = {};
			parent_buffer_ = nullptr;
		}
		// -------------------------------------------------------------------------------------------------------------------------------------------------


		// -------------------------------------------------------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------------------------------------------------------
		IndexBufferViewDep::IndexBufferViewDep()
		{
		}
		IndexBufferViewDep::~IndexBufferViewDep()
		{
			Finalize();
		}
		bool IndexBufferViewDep::Initialize(BufferDep* buffer, const Desc& desc)
		{
			if (!buffer || !buffer->GetParentDevice())
			{
				assert(false);
				return false;
			}

			const auto& buffer_desc = buffer->GetDesc();
			if ((int)BufferUsage::IndexBuffer != buffer_desc.usage_flag)
			{
				assert(false);
				return false;
			}

			parent_buffer_ = buffer;

			auto&& p_device = parent_buffer_->GetParentDevice();


			view_ = {};
			view_.SizeInBytes = buffer_desc.element_count * buffer_desc.element_byte_size;
			view_.BufferLocation = buffer->GetD3D12Resource()->GetGPUVirtualAddress();
			view_.Format = (buffer_desc.element_byte_size == 4) ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;

			return true;
		}
		void IndexBufferViewDep::Finalize()
		{
			view_ = {};
			parent_buffer_ = nullptr;
		}
		// -------------------------------------------------------------------------------------------------------------------------------------------------


		// -------------------------------------------------------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------------------------------------------------------
		TextureDep::TextureDep()
		{
		}
		TextureDep::~TextureDep()
		{
			Finalize();
		}

		bool TextureDep::Initialize(DeviceDep* p_device, const Desc& desc)
		{
			if (!p_device)
				return false;
			if (0 == desc.usage_flag)
			{
				assert(false);
				return false;
			}

			desc_ = desc;
			p_parent_device_ = p_device;

			if (0 >= desc_.width || 0 >= desc_.height)
				return false;
			if (ResourceFormat::NGL_FORMAT_UNKNOWN == desc_.format)
				return false;


			const bool is_depth_stencil = (int)BufferUsage::DepthStencil & desc_.usage_flag;
			const bool is_render_target = (int)BufferUsage::RenderTarget & desc_.usage_flag;


			// 深度バッファ用にフォーマット変換
			if (is_depth_stencil)
			{
				switch (desc_.format)
				{
				case ResourceFormat::NGL_FORMAT_D32_FLOAT:
					desc_.format = ResourceFormat::NGL_FORMAT_R32_TYPELESS; break;
				case  ResourceFormat::NGL_FORMAT_D32_FLOAT_S8X24_UINT:
					desc_.format = ResourceFormat::NGL_FORMAT_R32G8X24_TYPELESS; break;
				case  ResourceFormat::NGL_FORMAT_D24_UNORM_S8_UINT:
					desc_.format = ResourceFormat::NGL_FORMAT_R24G8_TYPELESS; break;
				case  ResourceFormat::NGL_FORMAT_D16_UNORM:
					desc_.format = ResourceFormat::NGL_FORMAT_R16_TYPELESS; break;
				}
			}

			// Heapタイプ
			D3D12_HEAP_FLAGS heap_flag = D3D12_HEAP_FLAG_NONE;
			D3D12_RESOURCE_STATES initial_state = ConvertResourceState(desc_.initial_state);
			D3D12_HEAP_PROPERTIES heap_prop = {};
			{
				switch (desc_.heap_type)
				{
				case ResourceHeapType::DEFAULT:
				{
					heap_prop.Type = D3D12_HEAP_TYPE_DEFAULT;
					break;
				}
				case ResourceHeapType::UPLOAD:
				{
					heap_prop.Type = D3D12_HEAP_TYPE_UPLOAD;
					break;
				}
				case ResourceHeapType::READBACK:
				{
					heap_prop.Type = D3D12_HEAP_TYPE_READBACK;
					break;
				}
				default:
				{
					heap_prop.Type = D3D12_HEAP_TYPE_DEFAULT;
				}
				}
				heap_prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
				heap_prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
				heap_prop.VisibleNodeMask = 0;
			}

			// Usageフラグ
			D3D12_RESOURCE_FLAGS need_flags = D3D12_RESOURCE_FLAG_NONE;
			if (is_render_target)
			{
				need_flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
			}
			if (is_depth_stencil)
			{
				need_flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			}
			if (desc_.allow_uav)
			{
				need_flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			}


			D3D12_RESOURCE_DESC resource_desc = {};
			{
				const D3D12_RESOURCE_DIMENSION kNativeDimensionTable[] = 
				{
					D3D12_RESOURCE_DIMENSION_TEXTURE1D,
					D3D12_RESOURCE_DIMENSION_TEXTURE2D,
					D3D12_RESOURCE_DIMENSION_TEXTURE3D,
				};

				resource_desc.Dimension = kNativeDimensionTable[static_cast<int>(desc_.dimension)];
				resource_desc.Alignment = 0u;
				resource_desc.Width = static_cast<UINT64>(desc_.width);
				resource_desc.Height = static_cast<UINT64>(desc_.height);
				resource_desc.DepthOrArraySize = static_cast<UINT16>(desc_.depth);
				resource_desc.MipLevels = desc_.mip_level;
				resource_desc.SampleDesc.Count = desc_.sample_count;
				resource_desc.SampleDesc.Quality = 0;
				resource_desc.Format = ConvertResourceFormat(desc_.format);
				resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
				resource_desc.Flags = need_flags;
			}


			// パラメータチェック
			if ((D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS & resource_desc.Flags) && (D3D12_HEAP_TYPE_DEFAULT != heap_prop.Type))
			{
				// UAVリソースはDefaultHeap以外許可されない.
				std::cout << "[ERROR] Heap of UAV Resource must be ResourceHeapType::DEFAULT" << std::endl;
				return false;
			}

			/*
				ステート制限
					Upload -> D3D12_RESOURCE_STATE_GENERIC_READ
					Readback -> D3D12_RESOURCE_STATE_COPY_DEST
					テクスチャ配置専用予約バッファ -> D3D12_RESOURCE_STATE_COMMON
			*/
			if (D3D12_HEAP_TYPE_UPLOAD == heap_prop.Type)
			{
				if (D3D12_RESOURCE_STATE_GENERIC_READ != initial_state)
				{
					std::cout << "[ERROR] State of Upload Buffer must be ResourceState::General" << std::endl;
					return false;
				}
			}
			else if (D3D12_HEAP_TYPE_READBACK == heap_prop.Type)
			{
				if (D3D12_RESOURCE_STATE_COPY_DEST != initial_state)
				{
					std::cout << "[ERROR] State of Readback Buffer must be ResourceState::CopyDst" << std::endl;
					return false;
				}
			}

			if (FAILED(p_device->GetD3D12Device()->CreateCommittedResource(&heap_prop, heap_flag, &resource_desc, initial_state, nullptr, IID_PPV_ARGS(&resource_))))
			{
				std::cout << "[ERROR] CreateCommittedResource" << std::endl;
				return false;
			}

			return true;
		}
		void TextureDep::Finalize()
		{
			resource_ = nullptr;
		}
		void TextureDep::Unmap()
		{
			if (!map_ptr_)
				return;

			resource_->Unmap(0, nullptr);
			map_ptr_ = nullptr;
		}
		ID3D12Resource* TextureDep::GetD3D12Resource()
		{
			return resource_;
		}
		// -------------------------------------------------------------------------------------------------------------------------------------------------


		// -------------------------------------------------------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------------------------------------------------------
		SamplerDep::SamplerDep()
		{
		}
		SamplerDep::~SamplerDep()
		{
			Finalize();
		}
		bool SamplerDep::Initialize(DeviceDep* p_device, const Desc& desc)
		{
			if (!p_device)
				return false;

			auto&& descriptor_allocator = p_device->GetPersistentSamplerDescriptorAllocator();// Samplerは専用のAllocatorを利用.
			view_ = descriptor_allocator->Allocate();
			if (!view_.IsValid())
				return false;

			// Persistent上に作成.
			p_device->GetD3D12Device()->CreateSampler(&(desc.desc), view_.cpu_handle);
			return true;
		}
		void SamplerDep::Finalize()
		{
			auto&& descriptor_allocator = view_.allocator;
			if (descriptor_allocator)
			{
				descriptor_allocator->Deallocate(view_);
			}
			view_ = {};
		}
	}
}