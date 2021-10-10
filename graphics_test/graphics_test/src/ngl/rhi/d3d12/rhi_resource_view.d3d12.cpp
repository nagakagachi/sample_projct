
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

		bool ShaderResourceViewDep::Initialize(DeviceDep* p_device, TextureDep* p_texture, u32 first_mip, u32 mip_count, u32 first_array, u32 array_size)
		{
			assert(p_device && p_texture);
			if (!p_device || !p_texture)
				return false;

			//const auto& res_desc = p_texture->GetDesc();

			// MipやArrayの値の範囲クランプ他.
			if (first_mip >= p_texture->GetMipCount())
			{
				first_mip = p_texture->GetMipCount() - 1;
				mip_count = 1;
			}
			else if ((mip_count == 0) || (mip_count > p_texture->GetMipCount() - first_mip))
			{
				mip_count = p_texture->GetMipCount() - first_mip;
			}
			if (first_array >= p_texture->GetArraySize())
			{
				first_array = p_texture->GetArraySize() - 1;
				array_size = 1;
			}
			else if ((array_size == 0) || (array_size > p_texture->GetArraySize() - first_array))
			{
				array_size = p_texture->GetArraySize() - first_array;
			}

			bool isTextureArray = p_texture->GetArraySize() > 1;

			D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc{};
			viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			viewDesc.Format = ConvertResourceFormat(depthToColorFormat(p_texture->GetDesc().format));
			viewDesc.ViewDimension = getTextureViewDimension<D3D12_SRV_DIMENSION>(getTextureDimension(p_texture->GetType(), isTextureArray));

			switch (p_texture->GetType())
			{
			case TextureType::Texture1D:
				if (isTextureArray)
				{
					viewDesc.Texture1DArray.MipLevels = mip_count;
					viewDesc.Texture1DArray.MostDetailedMip = first_mip;
					viewDesc.Texture1DArray.ArraySize = array_size;
					viewDesc.Texture1DArray.FirstArraySlice = first_array;
				}
				else
				{
					viewDesc.Texture1D.MipLevels = mip_count;
					viewDesc.Texture1D.MostDetailedMip = first_mip;
				}
				break;
			case TextureType::Texture2D:
				if (isTextureArray)
				{
					viewDesc.Texture2DArray.MipLevels = mip_count;
					viewDesc.Texture2DArray.MostDetailedMip = first_mip;
					viewDesc.Texture2DArray.ArraySize = array_size;
					viewDesc.Texture2DArray.FirstArraySlice = first_array;
				}
				else
				{
					viewDesc.Texture2D.MipLevels = mip_count;
					viewDesc.Texture2D.MostDetailedMip = first_mip;
				}
				break;
			case TextureType::Texture2DMultisample:
				if (array_size > 1)
				{
					viewDesc.Texture2DMSArray.ArraySize = array_size;
					viewDesc.Texture2DMSArray.FirstArraySlice = first_array;
				}
				break;
			case TextureType::Texture3D:
				assert(array_size == 1);
				viewDesc.Texture3D.MipLevels = mip_count;
				viewDesc.Texture3D.MostDetailedMip = first_mip;
				break;
			case TextureType::TextureCube:
				if (array_size > 1)
				{
					viewDesc.TextureCubeArray.First2DArrayFace = 0;
					viewDesc.TextureCubeArray.NumCubes = array_size;
					viewDesc.TextureCubeArray.MipLevels = mip_count;
					viewDesc.TextureCubeArray.MostDetailedMip = first_mip;
				}
				else
				{
					viewDesc.TextureCube.MipLevels = mip_count;
					viewDesc.TextureCube.MostDetailedMip = first_mip;
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

		// -------------------------------------------------------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------------------------------------------------------
		template<typename DescType>
		DescType createDsvRtvUavDescCommon(const TextureDep* p_texture, uint32_t mip_level, uint32_t first_array_slice, uint32_t array_size)
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
					desc.Texture1DArray.MipSlice = mip_level;
				}
				else
				{
					desc.Texture1D.MipSlice = mip_level;
				}
				break;
			case TextureType::Texture2D:
			case TextureType::TextureCube:
				if (p_texture->GetArraySize() * arrayMultiplier > 1)
				{
					desc.Texture2DArray.ArraySize = array_size * arrayMultiplier;
					desc.Texture2DArray.FirstArraySlice = first_array_slice * arrayMultiplier;
					desc.Texture2DArray.MipSlice = mip_level;
				}
				else
				{
					desc.Texture2D.MipSlice = mip_level;
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
					desc.Texture3D.MipSlice = mip_level;
					desc.Texture3D.FirstWSlice = 0;
					desc.Texture3D.WSize = p_texture->GetDepth(mip_level);
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

		D3D12_DEPTH_STENCIL_VIEW_DESC createDsvDesc(const TextureDep* p_texture, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize)
		{
			return createDsvRtvUavDescCommon<D3D12_DEPTH_STENCIL_VIEW_DESC>(p_texture, mipLevel, firstArraySlice, arraySize);
		}

		D3D12_RENDER_TARGET_VIEW_DESC createRtvDesc(const TextureDep* p_texture, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize)
		{
			return createDsvRtvUavDescCommon<D3D12_RENDER_TARGET_VIEW_DESC>(p_texture, mipLevel, firstArraySlice, arraySize);
		}


		DepthStencilViewDep::DepthStencilViewDep()
		{
		}
		DepthStencilViewDep::~DepthStencilViewDep()
		{
			Finalize();
		}
		bool DepthStencilViewDep::Initialize(DeviceDep* p_device, TextureDep* p_texture, u32 first_mip, u32 first_array, u32 array_size)
		{
			assert(p_device);
			assert(p_texture);

			D3D12_DEPTH_STENCIL_VIEW_DESC desc = createDsvDesc(p_texture, first_mip, first_array, array_size);

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
				p_device->GetD3D12Device()->CreateDepthStencilView(p_texture->GetD3D12Resource(), &desc, p_heap_->GetCPUDescriptorHandleForHeapStart());
			}

			return true;
		}
		void DepthStencilViewDep::Finalize()
		{
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
		// -------------------------------------------------------------------------------------------------------------------------------------------------

	}
}