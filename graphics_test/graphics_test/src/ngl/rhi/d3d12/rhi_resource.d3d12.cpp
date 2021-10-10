
#include "rhi_resource.d3d12.h"

#include "rhi.d3d12.h"

#include <array>
#include <algorithm>

namespace ngl
{
	namespace rhi
	{


		D3D12_RESOURCE_DIMENSION getD3D12ResourceDimension(TextureType type)
		{
			switch (type)
			{
			case TextureType::Texture1D:
				return D3D12_RESOURCE_DIMENSION_TEXTURE1D;

			case TextureType::Texture2D:
			//case TextureType::Texture2DMultisample:
			case TextureType::TextureCube:
				return D3D12_RESOURCE_DIMENSION_TEXTURE2D;

			case TextureType::Texture3D:
				return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
			default:
				assert(false);
				return D3D12_RESOURCE_DIMENSION_UNKNOWN;
			}
		}

		D3D12_RESOURCE_FLAGS getD3D12ResourceFlags(u32 flags)
		{
			D3D12_RESOURCE_FLAGS d3d = D3D12_RESOURCE_FLAG_NONE;

			bool uavRequired = 0;
			uavRequired |= and_nonzero(ResourceBindFlag::UnorderedAccess, flags);
			//uavRequired |= and_nonzero(ResourceBindFlag::AccelerationStructure, flags);
			
			if (uavRequired)
			{
				d3d |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			}

			if (and_nonzero(ResourceBindFlag::DepthStencil, flags))
			{
				if (and_nonzero(ResourceBindFlag::ShaderResource, flags) == false)
				{
					d3d |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
				}
				d3d |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			}

			if (and_nonzero(ResourceBindFlag::RenderTarget, flags))
			{
				d3d |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
			}

			return d3d;
		}

		D3D12_HEAP_TYPE getD3D12HeapType(ResourceHeapType type)
		{
			switch (type)
			{
			case ResourceHeapType::DEFAULT:		return D3D12_HEAP_TYPE_DEFAULT;
			case ResourceHeapType::UPLOAD:		return D3D12_HEAP_TYPE_UPLOAD;
			case ResourceHeapType::READBACK:	return D3D12_HEAP_TYPE_READBACK;
			default:							return D3D12_HEAP_TYPE_DEFAULT;
			}
		}

		// Srv Uav利用するDepthBufferのフォーマットはTypelessとする必要があるため変換.
		inline DXGI_FORMAT getTypelessFormatFromDepthFormat(ResourceFormat format)
		{
			switch (format)
			{
			case ResourceFormat::NGL_FORMAT_D16_UNORM:
				return DXGI_FORMAT_R16_TYPELESS;
			case ResourceFormat::NGL_FORMAT_D32_FLOAT_S8X24_UINT:
				return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
			case ResourceFormat::NGL_FORMAT_D24_UNORM_S8_UINT:
				return DXGI_FORMAT_R24G8_TYPELESS;
			case ResourceFormat::NGL_FORMAT_D32_FLOAT:
				return DXGI_FORMAT_R32_TYPELESS;
			default:
				assert(isDepthFormat(format) == false);
				return ConvertResourceFormat(format);
			}
		}

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


			// 用途によるアライメント.
			u32 need_alignment = 16;
			if (and_nonzero(ResourceBindFlag::ConstantBuffer, desc_.bind_flag))
			{
				need_alignment = static_cast<u32>(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
			}
			if (and_nonzero(ResourceBindFlag::UnorderedAccess, desc.bind_flag))
			{
				need_alignment = static_cast<u32>(D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT);
			}
			// 現状はとりあえずConstantBufferのアラインメントに従ってみる
			// 確保するサイズはDirectX12のConstantBufferサイズAlignmentにしたがう(256)
			const auto minimum_size = (desc_.element_byte_size) * (desc_.element_count);
			allocated_byte_size_ = need_alignment * ((minimum_size + need_alignment - 1) / need_alignment);

			D3D12_HEAP_FLAGS heap_flag = D3D12_HEAP_FLAG_NONE;
			D3D12_RESOURCE_STATES initial_state = ConvertResourceState(desc_.initial_state);
			D3D12_HEAP_PROPERTIES heap_prop = {};
			{
				heap_prop.Type = getD3D12HeapType(desc_.heap_type);
				heap_prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
				heap_prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
				heap_prop.VisibleNodeMask = 0;
			}

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
				resource_desc.Flags = getD3D12ResourceFlags(desc_.bind_flag);
			}

			// パラメータチェック
			{
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
			}

			// 生成.
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

			// Heapタイプ
			D3D12_HEAP_FLAGS heap_flag = D3D12_HEAP_FLAG_NONE;
			D3D12_RESOURCE_STATES initial_state = ConvertResourceState(desc_.initial_state);
			D3D12_HEAP_PROPERTIES heap_prop = {};
			{
				heap_prop.Type = getD3D12HeapType(desc_.heap_type);
				heap_prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
				heap_prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
				heap_prop.VisibleNodeMask = 0;
			}
			// リソースDesc
			D3D12_RESOURCE_DESC resource_desc = {};
			{
				resource_desc.Dimension = getD3D12ResourceDimension(desc_.type);
				resource_desc.Alignment = 0u;
				resource_desc.Width = static_cast<UINT64>(desc_.width);
				resource_desc.Height = static_cast<UINT64>(desc_.height);
				resource_desc.MipLevels = desc_.mip_level;
				resource_desc.SampleDesc.Count = (desc_.type == TextureType::Texture2DMultisample)? desc_.sample_count : 1;// Texture2DMultisample以外では1固定.
				resource_desc.SampleDesc.Quality = 0;
				resource_desc.Format = ConvertResourceFormat(desc_.format);
				resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
				resource_desc.Flags = getD3D12ResourceFlags(desc_.bind_flag);
				if (desc_.type == TextureType::TextureCube)
				{
					resource_desc.DepthOrArraySize = desc_.array_size * 6;
				}
				else if (desc_.type == TextureType::Texture3D)
				{
					resource_desc.DepthOrArraySize = desc_.depth;
				}
				else
				{
					resource_desc.DepthOrArraySize = desc_.array_size;
				}
			}
			assert(resource_desc.Width > 0 && resource_desc.Height > 0);
			assert(resource_desc.MipLevels > 0 && resource_desc.DepthOrArraySize > 0 && resource_desc.SampleDesc.Count > 0);
			
			// クリア値
			D3D12_CLEAR_VALUE clearValue = {};
			D3D12_CLEAR_VALUE* pClearVal = nullptr;
			if (and_nonzero(ResourceBindFlag::RenderTarget | ResourceBindFlag::DepthStencil, desc_.bind_flag))
			{
				clearValue.Format = resource_desc.Format;
				if (and_nonzero(ResourceBindFlag::DepthStencil, desc_.bind_flag))
				{
					clearValue.DepthStencil.Depth = 1.0f;
				}
				pClearVal = &clearValue;
			}
			if (isDepthFormat(desc_.format) && (and_nonzero(ResourceBindFlag::ShaderResource | ResourceBindFlag::UnorderedAccess, desc_.bind_flag)) )
			{
				// Depthフォーマット且つ用途がSrvまたはUavの場合はフォーマット変換.
				resource_desc.Format = getTypelessFormatFromDepthFormat(desc_.format);
			}

			// パラメータチェック
			{
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
			}

			// 生成.
			if (FAILED(p_device->GetD3D12Device()->CreateCommittedResource(&heap_prop, heap_flag, &resource_desc, initial_state, pClearVal, IID_PPV_ARGS(&resource_))))
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
			if (!and_nonzero(ResourceBindFlag::ConstantBuffer, buffer->GetDesc().bind_flag))
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
			if (!and_nonzero(ResourceBindFlag::VertexBuffer, buffer_desc.bind_flag))
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
			if (!and_nonzero(ResourceBindFlag::IndexBuffer, buffer_desc.bind_flag))
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

		namespace
		{
			/** texture dimension -> view dimension.
			*/
			ResourceDimension getTextureDimension(TextureType type, bool isTextureArray)
			{
				switch (type)
				{
				//case TextureType::Buffer:
				//	assert(isTextureArray == false);
				//	return ResourceDimension::Buffer;
				case TextureType::Texture1D:
					return (isTextureArray) ? ResourceDimension::Texture1DArray : ResourceDimension::Texture1D;
				case TextureType::Texture2D:
					return (isTextureArray) ? ResourceDimension::Texture2DArray : ResourceDimension::Texture2D;
				case TextureType::Texture2DMultisample:
					return (isTextureArray) ? ResourceDimension::Texture2DMSArray : ResourceDimension::Texture2DMS;
				case TextureType::Texture3D:
					assert(isTextureArray == false);
					return ResourceDimension::Texture3D;
				case TextureType::TextureCube:
					return (isTextureArray) ? ResourceDimension::TextureCubeArray : ResourceDimension::TextureCube;
				default:
					assert(false);
					return ResourceDimension::Unknown;
				}
			}

			/** view dimension to D3D12 view dimension.
			*/
			template<typename ViewType>
			ViewType getTextureViewDimension(ResourceDimension dimension);

			// for srv.
			template<>
			D3D12_SRV_DIMENSION getTextureViewDimension<D3D12_SRV_DIMENSION>(ResourceDimension dimension)
			{
				switch (dimension)
				{
				case ResourceDimension::Texture1D: return D3D12_SRV_DIMENSION_TEXTURE1D;
				case ResourceDimension::Texture1DArray: return D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
				case ResourceDimension::Texture2D: return D3D12_SRV_DIMENSION_TEXTURE2D;
				case ResourceDimension::Texture2DArray: return D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
				case ResourceDimension::Texture2DMS: return D3D12_SRV_DIMENSION_TEXTURE2DMS;
				case ResourceDimension::Texture2DMSArray: return D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
				case ResourceDimension::Texture3D: return D3D12_SRV_DIMENSION_TEXTURE3D;
				case ResourceDimension::TextureCube: return D3D12_SRV_DIMENSION_TEXTURECUBE;
				case ResourceDimension::TextureCubeArray: return D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
				case ResourceDimension::AccelerationStructure: return D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
				default:
					assert(false);
					return D3D12_SRV_DIMENSION_UNKNOWN;
				}
			}
			// for uav.
			template<>
			D3D12_UAV_DIMENSION getTextureViewDimension<D3D12_UAV_DIMENSION>(ResourceDimension dimension)
			{
				switch (dimension)
				{
				case ResourceDimension::Texture1D: return D3D12_UAV_DIMENSION_TEXTURE1D;
				case ResourceDimension::Texture1DArray: return D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
				case ResourceDimension::Texture2D: return D3D12_UAV_DIMENSION_TEXTURE2D;
				case ResourceDimension::Texture2DArray: return D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
				case ResourceDimension::Texture3D: return D3D12_UAV_DIMENSION_TEXTURE3D;
				default:
					assert(false);
					return D3D12_UAV_DIMENSION_UNKNOWN;
				}
			}
			// for dsv.
			template<>
			D3D12_DSV_DIMENSION getTextureViewDimension<D3D12_DSV_DIMENSION>(ResourceDimension dimension)
			{
				switch (dimension)
				{
				case ResourceDimension::Texture1D: return D3D12_DSV_DIMENSION_TEXTURE1D;
				case ResourceDimension::Texture1DArray: return D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
				case ResourceDimension::Texture2D: return D3D12_DSV_DIMENSION_TEXTURE2D;
				case ResourceDimension::Texture2DArray: return D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
				case ResourceDimension::Texture2DMS: return D3D12_DSV_DIMENSION_TEXTURE2DMS;
				case ResourceDimension::Texture2DMSArray: return D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
					// TODO: Falcor previously mapped cube to 2D array. Not sure if needed anymore.
					//case ReflectionResourceType::Dimensions::TextureCube: return D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
				default:
					assert(false);
					return D3D12_DSV_DIMENSION_UNKNOWN;
				}
			}
			// for rtv.
			template<>
			D3D12_RTV_DIMENSION getTextureViewDimension<D3D12_RTV_DIMENSION>(ResourceDimension dimension)
			{
				switch (dimension)
				{
				case ResourceDimension::Texture1D: return D3D12_RTV_DIMENSION_TEXTURE1D;
				case ResourceDimension::Texture1DArray: return D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
				case ResourceDimension::Texture2D: return D3D12_RTV_DIMENSION_TEXTURE2D;
				case ResourceDimension::Texture2DArray: return D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
				case ResourceDimension::Texture2DMS: return D3D12_RTV_DIMENSION_TEXTURE2DMS;
				case ResourceDimension::Texture2DMSArray: return D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
				case ResourceDimension::Texture3D: return D3D12_RTV_DIMENSION_TEXTURE3D;
				default:
					assert(false);
					return D3D12_RTV_DIMENSION_UNKNOWN;
				}
			}
		}

		ShaderResourceViewDep::ShaderResourceViewDep()
		{
		}
		ShaderResourceViewDep::~ShaderResourceViewDep()
		{
			Finalize();
		}

		bool ShaderResourceViewDep::Initialize(DeviceDep* p_device, TextureDep* p_texture, u32 firstMip, u32 mipCount, u32 firstArray, u32 arraySize)
		{
			assert(p_device && p_texture);
			if (!p_device || !p_texture)
				return false;

			const auto& res_desc = p_texture->GetDesc();

			// MipやArrayの値の範囲クランプ他.
			if (firstMip >= res_desc.mip_level)
			{
				firstMip = res_desc.mip_level - 1;
				mipCount = 1;
			}
			else if ((mipCount == 0) || (mipCount > res_desc.mip_level - firstMip))
			{
				mipCount = res_desc.mip_level - firstMip;
			}
			if (firstArray >= res_desc.array_size)
			{
				firstArray = res_desc.array_size - 1;
				arraySize = 1;
			}
			else if ((arraySize == 0) || (arraySize > res_desc.array_size - firstArray))
			{
				arraySize = res_desc.array_size - firstArray;
			}

			bool isTextureArray = res_desc.array_size > 1;

			D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc{};
			viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			viewDesc.Format = ConvertResourceFormat(depthToColorFormat(p_texture->GetDesc().format));
			viewDesc.ViewDimension = getTextureViewDimension<D3D12_SRV_DIMENSION>(getTextureDimension(res_desc.type, isTextureArray));

			switch (res_desc.type)
			{
			case TextureType::Texture1D:
				if (isTextureArray)
				{
					viewDesc.Texture1DArray.MipLevels = mipCount;
					viewDesc.Texture1DArray.MostDetailedMip = firstMip;
					viewDesc.Texture1DArray.ArraySize = arraySize;
					viewDesc.Texture1DArray.FirstArraySlice = firstArray;
				}
				else
				{
					viewDesc.Texture1D.MipLevels = mipCount;
					viewDesc.Texture1D.MostDetailedMip = firstMip;
				}
				break;
			case TextureType::Texture2D:
				if (isTextureArray)
				{
					viewDesc.Texture2DArray.MipLevels = mipCount;
					viewDesc.Texture2DArray.MostDetailedMip = firstMip;
					viewDesc.Texture2DArray.ArraySize = arraySize;
					viewDesc.Texture2DArray.FirstArraySlice = firstArray;
				}
				else
				{
					viewDesc.Texture2D.MipLevels = mipCount;
					viewDesc.Texture2D.MostDetailedMip = firstMip;
				}
				break;
			case TextureType::Texture2DMultisample:
				if (arraySize > 1)
				{
					viewDesc.Texture2DMSArray.ArraySize = arraySize;
					viewDesc.Texture2DMSArray.FirstArraySlice = firstArray;
				}
				break;
			case TextureType::Texture3D:
				assert(arraySize == 1);
				viewDesc.Texture3D.MipLevels = mipCount;
				viewDesc.Texture3D.MostDetailedMip = firstMip;
				break;
			case TextureType::TextureCube:
				if (arraySize > 1)
				{
					viewDesc.TextureCubeArray.First2DArrayFace = 0;
					viewDesc.TextureCubeArray.NumCubes = arraySize;
					viewDesc.TextureCubeArray.MipLevels = mipCount;
					viewDesc.TextureCubeArray.MostDetailedMip = firstMip;
				}
				else
				{
					viewDesc.TextureCube.MipLevels = mipCount;
					viewDesc.TextureCube.MostDetailedMip = firstMip;
				}
				break;
			default:
				assert(false);
			}

			// Descriptor確保.
			auto&& descriptor_allocator = p_device->GetPersistentDescriptorAllocator();
			view_ = descriptor_allocator->Allocate();
			if (!view_.IsValid())
			{
				std::cout << "[ERROR] ConstantBufferViewDep::Initialize" << std::endl;
				assert(false);
				return false;
			}
			// View生成.
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