
#include "rhi_resource_view.d3d12.h"

#include "rhi.d3d12.h"

#include <array>
#include <algorithm>

namespace ngl
{
	namespace rhi
	{


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
			if (!check_bits(ResourceBindFlag::ConstantBuffer, buffer->GetDesc().bind_flag))
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
			if (!check_bits(ResourceBindFlag::VertexBuffer, buffer_desc.bind_flag))
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
			if (!check_bits(ResourceBindFlag::IndexBuffer, buffer_desc.bind_flag))
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




			// -------------------------------------------------------------------------------------------------------------------------------------------------
			// Texture Dsv, Rtv, Uavの設定共通化.
			template<typename DescType>
			DescType createDsvRtvUavDescCommon(const TextureDep* p_texture, uint32_t mip_slice, uint32_t first_array_slice, uint32_t array_size)
			{
				assert(p_texture);   // Buffers should not get here

				uint32_t arrayMultiplier = (p_texture->GetType() == TextureType::TextureCube) ? 6 : 1;

				if (array_size + first_array_slice > p_texture->GetArraySize())
				{
					array_size = p_texture->GetArraySize() - first_array_slice;
				}

				DescType desc = {};
				desc.Format = ConvertResourceFormat(p_texture->GetFormat());
				desc.ViewDimension = getTextureViewDimension<decltype(desc.ViewDimension)>(getTextureDimension(p_texture->GetType(), p_texture->GetArraySize() > 1));

				switch (p_texture->GetType())
				{
				case TextureType::Texture1D:
					if (p_texture->GetArraySize() > 1)
					{
						desc.Texture1DArray.ArraySize = array_size;
						desc.Texture1DArray.FirstArraySlice = first_array_slice;
						desc.Texture1DArray.MipSlice = mip_slice;
					}
					else
					{
						desc.Texture1D.MipSlice = mip_slice;
					}
					break;
				case TextureType::Texture2D:
				case TextureType::TextureCube:
					if (p_texture->GetArraySize() * arrayMultiplier > 1)
					{
						desc.Texture2DArray.ArraySize = array_size * arrayMultiplier;
						desc.Texture2DArray.FirstArraySlice = first_array_slice * arrayMultiplier;
						desc.Texture2DArray.MipSlice = mip_slice;
					}
					else
					{
						desc.Texture2D.MipSlice = mip_slice;
					}
					break;
				case TextureType::Texture2DMultisample:
					if constexpr (std::is_same_v<DescType, D3D12_DEPTH_STENCIL_VIEW_DESC> || std::is_same_v<DescType, D3D12_RENDER_TARGET_VIEW_DESC>)
					{
						if (p_texture->GetArraySize() > 1)
						{
							desc.Texture2DMSArray.ArraySize = array_size;
							desc.Texture2DMSArray.FirstArraySlice = first_array_slice;
						}
					}
					else
					{
						throw std::exception("Texture2DMultisample does not support UAV views");
					}
					break;
				case TextureType::Texture3D:
					if constexpr (std::is_same_v<DescType, D3D12_UNORDERED_ACCESS_VIEW_DESC> || std::is_same_v<DescType, D3D12_RENDER_TARGET_VIEW_DESC>)
					{
						assert(p_texture->GetArraySize() == 1);
						desc.Texture3D.MipSlice = mip_slice;
						desc.Texture3D.FirstWSlice = 0;
						desc.Texture3D.WSize = p_texture->GetDepth(mip_slice);
					}
					else
					{
						throw std::exception("Texture3D does not support DSV views");
					}
					break;
				default:
					assert(false);
				}

				return desc;
			}
			// Texture Dsv用.
			D3D12_DEPTH_STENCIL_VIEW_DESC createDsvDesc(const TextureDep* p_texture, uint32_t mip_slice, uint32_t first_array_slice, uint32_t array_size)
			{
				return createDsvRtvUavDescCommon<D3D12_DEPTH_STENCIL_VIEW_DESC>(p_texture, mip_slice, first_array_slice, array_size);
			}
			// Texture Rtv用.
			D3D12_RENDER_TARGET_VIEW_DESC createRtvDesc(const TextureDep* p_texture, uint32_t mip_slice, uint32_t first_array_slice, uint32_t array_size)
			{
				return createDsvRtvUavDescCommon<D3D12_RENDER_TARGET_VIEW_DESC>(p_texture, mip_slice, first_array_slice, array_size);
			}
			// Texture Uav用.
			D3D12_UNORDERED_ACCESS_VIEW_DESC createUavDesc(const TextureDep* p_texture, uint32_t mip_slice, uint32_t first_array_slice, uint32_t array_size)
			{
				return createDsvRtvUavDescCommon<D3D12_UNORDERED_ACCESS_VIEW_DESC>(p_texture, mip_slice, first_array_slice, array_size);
			}

			// Texture Srv用.
			D3D12_SHADER_RESOURCE_VIEW_DESC createTextureSrvDesc(const TextureDep* p_texture, uint32_t mip_slice, uint32_t mip_count, uint32_t first_array_slice, uint32_t array_size)
			{
				assert(p_texture);
				D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};

				//If not depth, returns input format
				desc.Format = ConvertResourceFormat(depthToColorFormat(p_texture->GetFormat()));

				bool isTextureArray = p_texture->GetArraySize() > 1;
				desc.ViewDimension = getTextureViewDimension<D3D12_SRV_DIMENSION>(getTextureDimension(p_texture->GetType(), isTextureArray));

				switch (p_texture->GetType())
				{
				case TextureType::Texture1D:
					if (isTextureArray)
					{
						desc.Texture1DArray.MipLevels = mip_count;
						desc.Texture1DArray.MostDetailedMip = mip_slice;
						desc.Texture1DArray.ArraySize = array_size;
						desc.Texture1DArray.FirstArraySlice = first_array_slice;
					}
					else
					{
						desc.Texture1D.MipLevels = mip_count;
						desc.Texture1D.MostDetailedMip = mip_slice;
					}
					break;
				case TextureType::Texture2D:
					if (isTextureArray)
					{
						desc.Texture2DArray.MipLevels = mip_count;
						desc.Texture2DArray.MostDetailedMip = mip_slice;
						desc.Texture2DArray.ArraySize = array_size;
						desc.Texture2DArray.FirstArraySlice = first_array_slice;
					}
					else
					{
						desc.Texture2D.MipLevels = mip_count;
						desc.Texture2D.MostDetailedMip = mip_slice;
					}
					break;
				case TextureType::Texture2DMultisample:
					if (array_size > 1)
					{
						desc.Texture2DMSArray.ArraySize = array_size;
						desc.Texture2DMSArray.FirstArraySlice = first_array_slice;
					}
					break;
				case TextureType::Texture3D:
					assert(array_size == 1);
					desc.Texture3D.MipLevels = mip_count;
					desc.Texture3D.MostDetailedMip = mip_slice;
					break;
				case TextureType::TextureCube:
					if (array_size > 1)
					{
						desc.TextureCubeArray.First2DArrayFace = 0;
						desc.TextureCubeArray.NumCubes = array_size;
						desc.TextureCubeArray.MipLevels = mip_count;
						desc.TextureCubeArray.MostDetailedMip = mip_slice;
					}
					else
					{
						desc.TextureCube.MipLevels = mip_count;
						desc.TextureCube.MostDetailedMip = mip_slice;
					}
					break;
				default:
					assert(false);
				}

				desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				return desc;
			}

			/*
			// TODO.
			// Buffer Srv.
			D3D12_SHADER_RESOURCE_VIEW_DESC createBufferSrvDesc(const BufferDep* pBuffer, uint32_t firstElement, uint32_t elementCount)
			{
				assert(pBuffer);
				D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};

				uint32_t bufferElementSize = 0;
				uint32_t bufferElementCount = 0;
				if (pBuffer->isTyped())
				{
					assert(getFormatPixelsPerBlock(pBuffer->getFormat()) == 1);
					bufferElementSize = getFormatBytesPerBlock(pBuffer->getFormat());
					bufferElementCount = pBuffer->getElementCount();
					desc.Format = getDxgiFormat(pBuffer->getFormat());
				}
				else if (pBuffer->isStructured())
				{
					bufferElementSize = pBuffer->getStructSize();
					bufferElementCount = pBuffer->getElementCount();
					desc.Format = DXGI_FORMAT_UNKNOWN;
					desc.Buffer.StructureByteStride = pBuffer->getStructSize();
				}
				else
				{
					// ByteAddress.
					bufferElementSize = sizeof(uint32_t);
					bufferElementCount = (uint32_t)(pBuffer->getSize() / sizeof(uint32_t));
					desc.Format = DXGI_FORMAT_R32_TYPELESS;
					desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
				}

				bool useDefaultCount = (elementCount == ShaderResourceView::kMaxPossible);
				assert(useDefaultCount || (firstElement + elementCount) <= bufferElementCount); // Check range
				desc.Buffer.FirstElement = firstElement;
				desc.Buffer.NumElements = useDefaultCount ? (bufferElementCount - firstElement) : elementCount;

				desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;

				// D3D12 doesn't currently handle views that extend to close to 4GB or beyond the base address.
				// TODO: Revisit this check in the future.
				assert(bufferElementSize > 0);
				if (desc.Buffer.FirstElement + desc.Buffer.NumElements > ((1ull << 32) / bufferElementSize - 8))
				{
					throw std::exception("Buffer SRV exceeds the maximum supported size");
				}

				return desc;
			}

			// TODO.
			// Buffer Uav.
			D3D12_UNORDERED_ACCESS_VIEW_DESC createBufferUavDesc(const Buffer* pBuffer, uint32_t firstElement, uint32_t elementCount)
			{
				assert(pBuffer);
				D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};

				uint32_t bufferElementSize = 0;
				uint32_t bufferElementCount = 0;
				if (pBuffer->isTyped())
				{
					assert(getFormatPixelsPerBlock(pBuffer->getFormat()) == 1);
					bufferElementSize = getFormatBytesPerBlock(pBuffer->getFormat());
					bufferElementCount = pBuffer->getElementCount();
					desc.Format = getDxgiFormat(pBuffer->getFormat());
				}
				else if (pBuffer->isStructured())
				{
					bufferElementSize = pBuffer->getStructSize();
					bufferElementCount = pBuffer->getElementCount();
					desc.Format = DXGI_FORMAT_UNKNOWN;
					desc.Buffer.StructureByteStride = pBuffer->getStructSize();
				}
				else
				{
					bufferElementSize = sizeof(uint32_t);
					bufferElementCount = (uint32_t)(pBuffer->getSize() / sizeof(uint32_t));
					desc.Format = DXGI_FORMAT_R32_TYPELESS;
					desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
				}

				bool useDefaultCount = (elementCount == UnorderedAccessView::kMaxPossible);
				assert(useDefaultCount || (firstElement + elementCount) <= bufferElementCount); // Check range
				desc.Buffer.FirstElement = firstElement;
				desc.Buffer.NumElements = useDefaultCount ? bufferElementCount - firstElement : elementCount;

				desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

				// D3D12 doesn't currently handle views that extend to close to 4GB or beyond the base address.
				// TODO: Revisit this check in the future.
				assert(bufferElementSize > 0);
				if (desc.Buffer.FirstElement + desc.Buffer.NumElements > ((1ull << 32) / bufferElementSize - 8))
				{
					throw std::exception("Buffer UAV exceeds the maximum supported size");
				}

				return desc;
			}
			*/
		}


		// -------------------------------------------------------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------------------------------------------------------
		DepthStencilViewDep::DepthStencilViewDep()
		{
		}
		DepthStencilViewDep::~DepthStencilViewDep()
		{
			Finalize();
		}
		bool DepthStencilViewDep::Initialize(DeviceDep* p_device, const TextureDep* p_texture, u32 mip_slice, u32 first_array_slice, u32 array_size)
		{
			assert(p_device);
			assert(p_texture);

			if (!check_bits(ResourceBindFlag::DepthStencil, p_texture->GetBindFlag()))
			{
				assert(false);
				return false;
			}

			// 専有Heap確保.
			{
				D3D12_DESCRIPTOR_HEAP_DESC desc = {};
				desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
				desc.NumDescriptors = 1;
				desc.NodeMask = 0;
				desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

				if (FAILED(p_device->GetD3D12Device()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&p_heap_))))
				{
					std::cout << "[ERROR] Create DescriptorHeap" << std::endl;
					return false;
				}
			}
			// 専有HeapにDescriptor作成.
			{
				D3D12_DEPTH_STENCIL_VIEW_DESC desc = createDsvDesc(p_texture, mip_slice, first_array_slice, array_size);
				p_device->GetD3D12Device()->CreateDepthStencilView(p_texture->GetD3D12Resource(), &desc, p_heap_->GetCPUDescriptorHandleForHeapStart());
			}

			return true;
		}
		void DepthStencilViewDep::Finalize()
		{
			p_heap_.Release();
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
		bool RenderTargetViewDep::Initialize(DeviceDep* p_device, const TextureDep* p_texture, u32 mip_slice, u32 first_array_slice, u32 array_size)
		{
			assert(p_device);
			assert(p_texture);

			if (!check_bits(ResourceBindFlag::RenderTarget, p_texture->GetBindFlag()))
			{
				assert(false);
				return false;
			}

			// 専有Heap確保.
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
			// 専有HeapにDescriptor作成.
			{
				D3D12_RENDER_TARGET_VIEW_DESC desc = createRtvDesc(p_texture, mip_slice, first_array_slice, array_size);
				p_device->GetD3D12Device()->CreateRenderTargetView(p_texture->GetD3D12Resource(), &desc, p_heap_->GetCPUDescriptorHandleForHeapStart());
			}

			return true;
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
			p_heap_.Release();
		}

		// -------------------------------------------------------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------------------------------------------------------
		UnorderedAccessView::UnorderedAccessView()
		{
		}
		UnorderedAccessView::~UnorderedAccessView()
		{
			Finalize();
		}
		bool UnorderedAccessView::Initialize(DeviceDep* p_device, const TextureDep* p_texture, u32 mip_slice, u32 first_array_slice, u32 array_size)
		{
			assert(p_device);
			assert(p_texture);

			if (!check_bits(ResourceBindFlag::UnorderedAccess, p_texture->GetBindFlag()))
			{
				assert(false);
				return false;
			}

			// Descriptor確保.
			auto&& descriptor_allocator = p_device->GetPersistentDescriptorAllocator();
			view_ = descriptor_allocator->Allocate();
			if (!view_.IsValid())
			{
				std::cout << "[ERROR] UnorderedAccessView::Initialize" << std::endl;
				assert(false);
				return false;
			}

			{
				D3D12_UNORDERED_ACCESS_VIEW_DESC desc = createUavDesc(p_texture, mip_slice, first_array_slice, array_size);
				constexpr ID3D12Resource* p_counter_resource = nullptr;
				p_device->GetD3D12Device()->CreateUnorderedAccessView(p_texture->GetD3D12Resource(), p_counter_resource, &desc, view_.cpu_handle);
			}

			return true;
		}
		void UnorderedAccessView::Finalize()
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
		ShaderResourceViewDep::ShaderResourceViewDep()
		{
		}
		ShaderResourceViewDep::~ShaderResourceViewDep()
		{
			Finalize();
		}
		bool ShaderResourceViewDep::Initialize(DeviceDep* p_device, const TextureDep* p_texture, u32 mip_slice, u32 mip_count, u32 first_array_slice, u32 array_size)
		{
			assert(p_device && p_texture);
			if (!p_device || !p_texture)
				return false;

			if (!check_bits(ResourceBindFlag::ShaderResource, p_texture->GetBindFlag()))
			{
				assert(false);
				return false;
			}

			// clamp mip, array range.
			if (mip_slice >= p_texture->GetMipCount())
			{
				mip_slice = p_texture->GetMipCount() - 1;
				mip_count = 1;
			}
			else if ((mip_count == 0) || (mip_count > p_texture->GetMipCount() - mip_slice))
			{
				mip_count = p_texture->GetMipCount() - mip_slice;
			}
			if (first_array_slice >= p_texture->GetArraySize())
			{
				first_array_slice = p_texture->GetArraySize() - 1;
				array_size = 1;
			}
			else if ((array_size == 0) || (array_size > p_texture->GetArraySize() - first_array_slice))
			{
				array_size = p_texture->GetArraySize() - first_array_slice;
			}

			// Desc生成.
			auto desc = createTextureSrvDesc(p_texture, mip_slice, mip_count, first_array_slice, array_size);

			// Descriptor確保.
			auto&& descriptor_allocator = p_device->GetPersistentDescriptorAllocator();
			view_ = descriptor_allocator->Allocate();
			if (!view_.IsValid())
			{
				std::cout << "[ERROR] ShaderResourceViewDep::Initialize" << std::endl;
				assert(false);
				return false;
			}
			// View生成.
			p_device->GetD3D12Device()->CreateShaderResourceView(p_texture->GetD3D12Resource(), &desc, view_.cpu_handle);

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

		// -------------------------------------------------------------------------------------------------------------------------------------------------

	}
}