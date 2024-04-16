﻿
#include "texture_loader_directxtex.h"

#include <numeric>

#include "ngl/math/math.h"

// rhi
#include "ngl/rhi/d3d12/resource.d3d12.h"
#include "ngl/rhi/d3d12/resource_view.d3d12.h"



namespace ngl
{
namespace directxtex
{
    namespace
    {
        // wchar_t to char.
        // ビルドを通すために _CRT_SECURE_NO_WARNINGS が必要.
        int mbs_to_wcs(wchar_t* dst, int dst_len, const char* src)
        {
            size_t cnt = 0;
            mbstowcs_s(&cnt, dst, dst_len, src, dst_len);
            return static_cast<int>(cnt);
        }
    }
    
    bool LoadImageData(DirectX::ScratchImage& image_data, DirectX::TexMetadata& meta_data, rhi::DeviceDep* p_device, const char* filename)
    {
        constexpr int k_temporal_name_buffer_len = 256;
        wchar_t temporal_name_buffer[k_temporal_name_buffer_len];

        const auto filename_len = strlen(filename);
        assert(filename_len < k_temporal_name_buffer_len);
        mbs_to_wcs(temporal_name_buffer, k_temporal_name_buffer_len, filename);
        
        DirectX::WIC_FLAGS flags = DirectX::WIC_FLAGS_NONE;
        image_data = {};
        meta_data = {};
        if( FAILED(DirectX::LoadFromWICFile(temporal_name_buffer, flags, &meta_data, image_data)))
        {
            assert(false);
            return false;
        }
        
        return true;
    }
    
}
}
