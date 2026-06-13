#pragma once
#include <stdint.h>
#include <bc7enc_rdo/rgbcx.h>
#include <bc7enc_rdo/bc7enc.h>
#include <KTX/vulkan_format.h>
#include <PKAsset.h>

namespace PKAssets::Texture
{
    struct BlockCompressContext
    {
        const uint8_t* src_data = nullptr;
        uint32_t width = 0u;
        uint32_t height = 0u;

        PKTextureFormat src_format = PKTextureFormat::RGBA8_Unorm;
        PKTextureFormat dst_format = PKTextureFormat::BC7_RGBA;
        rgbcx::bc1_approx_mode bc1_mode = rgbcx::bc1_approx_mode::cBC1Ideal;
        int32_t bc7enc_max_partitions_to_scan = BC7ENC_MAX_PARTITIONS;
        int32_t bc7_uber_level = 6;
        uint32_t bc345_mode_mask = rgbcx::BC4_USE_ALL_MODES;
        int32_t bc1_quality_level = rgbcx::MAX_LEVEL;
        int32_t bc345_search_rad = 5;
        bool perceptual = false;
        bool use_bc1_3color_mode = true;
        bool use_bc1_3color_mode_for_black = true;
        bool use_hq_bc345 = true;
        bool bc7enc_mode6_only = false;
    };

    PKTextureFormat VkFormatToPKTextureFormat(VkFormat format);

    int BlockCompressBCUnorm(BlockCompressContext* ctx, void* dstData, size_t* dstSize);
}
