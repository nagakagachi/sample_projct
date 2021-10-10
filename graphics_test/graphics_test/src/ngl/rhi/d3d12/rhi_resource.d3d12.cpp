
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
			if (0 == desc.bind_flag)
			{
				assert(false);
				return false;
			}

			if (0 >= desc.element_byte_size || 0 >= desc.element_count)
				return false;

			desc_ = desc;
			p_parent_device_ = p_device;

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
			if (ResourceBindFlag::ConstantBuffer & desc_.bind_flag)
			{
				need_alignment = static_cast<u32>(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
			}
			if (ResourceBindFlag::UnorderedAccess & desc.bind_flag)
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
		void* BufferDep::Map()
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
				return map_ptr_;
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
			return map_ptr_;
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
			if (0 == desc.bind_flag)
			{
				assert(false);
				return false;
			}

			if (0 >= desc.width || 0 >= desc.height)
				return false;
			if (ResourceFormat::NGL_FORMAT_UNKNOWN == desc.format)
				return false;

			desc_ = desc;
			p_parent_device_ = p_device;

			if (ResourceHeapType::UPLOAD == desc_.heap_type || ResourceHeapType::READBACK == desc_.heap_type)
			{
				// TODO. DefaultHeap以外のTextureは生成できないので, 内部的にはUpload/READBACKなBufferを生成してそちらにCPUアクセスし,DefaultHeapなTextureにコピーする.
				// 未実装.
				std::cout << "[ERROR] Upload or Readback Texture is Unimplemented." << std::endl;
				assert(false);
				return false;
			}

			const bool is_depth_stencil = ResourceBindFlag::DepthStencil & desc_.bind_flag;
			const bool is_render_target = ResourceBindFlag::RenderTarget & desc_.bind_flag;
			const bool is_uav = ResourceBindFlag::UnorderedAccess & desc_.bind_flag;


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
			if (is_uav)
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
		void* TextureDep::Map()
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
				return map_ptr_;
			}
			if (FAILED(resource_->Map(0, nullptr, &map_ptr_)))
			{
				std::cout << "ERROR: Resouce Map" << std::endl;
				map_ptr_ = nullptr;
				return nullptr;
			}
			return map_ptr_;
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
			if (!(ResourceBindFlag::ConstantBuffer & buffer->GetDesc().bind_flag))
			{
				assert(false);
				return false;
			}

			auto&& p_device = buffer->GetParentDevice();
			
			auto&& descriptor_allocator = p_device->GetPersistentDescriptorAllocator();
			view_ = descriptor_allocator->Allocate();
			if (!view_.IsValid())
			{
				std::cout << "[ERROR] ConstantBufferViewDep::Initialize" << std::endl;
				assert(false);
				return false;
			}

			D3D12_CONSTANT_BUFFER_VIEW_DESC view_desc = {};
			view_desc.BufferLocation = buffer->GetD3D12Resource()->GetGPUVirtualAddress();
			view_desc.SizeInBytes = buffer->GetAlignedBufferSize();// アライメント考慮サイズを指定している.
			auto handle = view_.cpu_handle;
			p_device->GetD3D12Device()->CreateConstantBufferView(&view_desc, handle);

			return true;
		}
		void ConstantBufferViewDep::Finalize()
		{
			auto&& descriptor_allocator = view_.allocator;
			if (descriptor_allocator)
			{
				descriptor_allocator->Deallocate(view_);
			}

			view_ = {};
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
			if (!(ResourceBindFlag::VertexBuffer & buffer_desc.bind_flag))
			{
				assert(false);
				return false;
			}

			auto&& p_device = buffer->GetParentDevice();

			view_ = {};
			view_.SizeInBytes = buffer_desc.element_count * buffer_desc.element_byte_size;
			view_.StrideInBytes = buffer_desc.element_byte_size;
			view_.BufferLocation = buffer->GetD3D12Resource()->GetGPUVirtualAddress();

			return true;
		}
		void VertexBufferViewDep::Finalize()
		{
			view_ = {};
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
			if (!(ResourceBindFlag::IndexBuffer & buffer_desc.bind_flag))
			{
				assert(false);
				return false;
			}

			auto&& p_device = buffer->GetParentDevice();

			view_ = {};
			view_.SizeInBytes = buffer_desc.element_count * buffer_desc.element_byte_size;
			view_.BufferLocation = buffer->GetD3D12Resource()->GetGPUVirtualAddress();
			view_.Format = (buffer_desc.element_byte_size == 4) ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;

			return true;
		}
		void IndexBufferViewDep::Finalize()
		{
			view_ = {};
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

		// -------------------------------------------------------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------------------------------------------------------
		RenderTargetViewDep::RenderTargetViewDep()
		{
		}
		RenderTargetViewDep::~RenderTargetViewDep()
		{
			Finalize();
		}
		// SwapChainからRTV作成.
		bool RenderTargetViewDep::Initialize(DeviceDep* p_device, SwapChainDep* p_swapchain, unsigned int buffer_index)
		{
			if (!p_device || !p_swapchain)
				return false;

			{
				D3D12_DESCRIPTOR_HEAP_DESC desc = {};
				desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
				desc.NumDescriptors = 1;
				desc.NodeMask = 0;
				desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

				if (FAILED(p_device->GetD3D12Device()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&p_heap_))))
				{
					std::cout << "[ERROR] Create DescriptorHeap" << std::endl;
					return false;
				}
			}
			{
				auto* buffer = p_swapchain->GetD3D12Resource(buffer_index);
				if (!buffer)
				{
					std::cout << "[ERROR] Invalid Buffer Index" << std::endl;
					return false;
				}

				auto handle_head = p_heap_->GetCPUDescriptorHandleForHeapStart();
				p_device->GetD3D12Device()->CreateRenderTargetView(buffer, nullptr, handle_head);
			}

			return true;
		}

		void RenderTargetViewDep::Finalize()
		{
			p_heap_ = nullptr;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetViewDep::GetD3D12DescriptorHandle() const
		{
			return p_heap_->GetCPUDescriptorHandleForHeapStart();
		}
		// -------------------------------------------------------------------------------------------------------------------------------------------------


		// -------------------------------------------------------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------------------------------------------------------

		ShaderResourceViewDep::ShaderResourceViewDep()
		{
		}
		ShaderResourceViewDep::~ShaderResourceViewDep()
		{
			Finalize();
		}

		// SwapChainからRTV作成.
		bool ShaderResourceViewDep::Initialize(DeviceDep* p_device, TextureDep* p_texture, u32 firstMip, u32 mipCount, u32 firstArray, u32 arraySize)
		{
			assert(p_device && p_texture);
			if (!p_device || !p_texture)
				return false;

			const D3D12_RESOURCE_DESC resDesc = p_texture->GetD3D12Resource()->GetDesc();

			if (firstMip >= resDesc.MipLevels)
			{
				firstMip = resDesc.MipLevels - 1;
				mipCount = 1;
			}
			else if ((mipCount == 0) || (mipCount > resDesc.MipLevels - firstMip))
			{
				mipCount = resDesc.MipLevels - firstMip;
			}
			if (firstArray >= resDesc.DepthOrArraySize)
			{
				firstArray = resDesc.DepthOrArraySize - 1;
				arraySize = 1;
			}
			else if ((arraySize == 0) || (arraySize > resDesc.DepthOrArraySize - firstArray))
			{
				arraySize = resDesc.DepthOrArraySize - firstArray;
			}

			D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc{};
			viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			viewDesc.Format = resDesc.Format;// pTex->GetTextureDesc().format;
			switch (viewDesc.Format)
			{
			case DXGI_FORMAT_D32_FLOAT:
				viewDesc.Format = DXGI_FORMAT_R32_FLOAT; break;
			case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
				viewDesc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS; break;
			case DXGI_FORMAT_D24_UNORM_S8_UINT:
				viewDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS; break;
			case DXGI_FORMAT_D16_UNORM:
				viewDesc.Format = DXGI_FORMAT_R16_UNORM; break;
			}
			if (resDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D)
			{
				if (resDesc.DepthOrArraySize == 1)
				{
					viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
					viewDesc.Texture1D.MipLevels = mipCount;
					viewDesc.Texture1D.MostDetailedMip = firstMip;
					viewDesc.Texture1D.ResourceMinLODClamp = 0.0f;
				}
				else
				{
					viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
					viewDesc.Texture1DArray.MipLevels = mipCount;
					viewDesc.Texture1DArray.MostDetailedMip = firstMip;
					viewDesc.Texture1DArray.ResourceMinLODClamp = 0.0f;
					viewDesc.Texture1DArray.FirstArraySlice = firstArray;
					viewDesc.Texture1DArray.ArraySize = arraySize;
				}
			}
			else if (resDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
			{
				if (resDesc.SampleDesc.Count == 1)
				{
					if (resDesc.DepthOrArraySize == 1)
					{
						viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
						viewDesc.Texture2D.MipLevels = mipCount;
						viewDesc.Texture2D.MostDetailedMip = firstMip;
						viewDesc.Texture2D.PlaneSlice = 0;
						viewDesc.Texture2D.ResourceMinLODClamp = 0.0f;
					}
					else
					{
						viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
						viewDesc.Texture2DArray.MipLevels = mipCount;
						viewDesc.Texture2DArray.MostDetailedMip = firstMip;
						viewDesc.Texture2DArray.PlaneSlice = 0;
						viewDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
						viewDesc.Texture2DArray.FirstArraySlice = firstArray;
						viewDesc.Texture2DArray.ArraySize = arraySize;
					}
				}
				else
				{
					if (resDesc.DepthOrArraySize == 1)
					{
						viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
					}
					else
					{
						viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
						viewDesc.Texture2DMSArray.FirstArraySlice = firstArray;
						viewDesc.Texture2DMSArray.ArraySize = arraySize;
					}
				}
			}
			else
			{
				viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
				viewDesc.Texture3D.MipLevels = mipCount;
				viewDesc.Texture3D.MostDetailedMip = firstMip;
				viewDesc.Texture3D.ResourceMinLODClamp = 0.0f;
			}

			auto&& descriptor_allocator = p_device->GetPersistentDescriptorAllocator();
			view_ = descriptor_allocator->Allocate();
			if (!view_.IsValid())
			{
				std::cout << "[ERROR] ConstantBufferViewDep::Initialize" << std::endl;
				assert(false);
				return false;
			}

			p_device->GetD3D12Device()->CreateShaderResourceView(p_texture->GetD3D12Resource(), &viewDesc, view_.cpu_handle);

			return true;
		}

		void ShaderResourceViewDep::Finalize()
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