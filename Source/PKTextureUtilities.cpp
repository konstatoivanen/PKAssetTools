#include <malloc.h>
#include "PKTextureUtilities.h"

#define RGBCX_IMPLEMENTATION
#include <bc7enc_rdo/rgbcx.h>

namespace PKAssets::Texture
{
    PKTextureFormat VkFormatToPKTextureFormat(VkFormat format)
    {
        switch (format)
        {
            case VK_FORMAT_R8_UNORM: return PKTextureFormat::R8_Unorm;
            case VK_FORMAT_R8_SNORM: return PKTextureFormat::R8_Snorm;
            case VK_FORMAT_R8_UINT: return PKTextureFormat::R8_Uint;
            case VK_FORMAT_R8_SINT: return PKTextureFormat::R8_Int;
            case VK_FORMAT_R16_SFLOAT: return PKTextureFormat::R16_Float;
            case VK_FORMAT_R16_UINT: return PKTextureFormat::R16_Uint;
            case VK_FORMAT_R16_SINT: return PKTextureFormat::R16_Int;
            case VK_FORMAT_R8G8_UNORM: return PKTextureFormat::RG8_Unorm;
            case VK_FORMAT_R8G8_SNORM: return PKTextureFormat::RG8_Snorm;
            case VK_FORMAT_R8G8_UINT: return PKTextureFormat::RG8_Uint;
            case VK_FORMAT_R8G8_SINT: return PKTextureFormat::RG8_Int;
            case VK_FORMAT_R8G8B8_UNORM: return PKTextureFormat::RGB8_Unorm;
            case VK_FORMAT_R8G8B8_SNORM: return PKTextureFormat::RGB8_Snorm;
            case VK_FORMAT_R8G8B8_SRGB: return PKTextureFormat::RGB8_Srgb;
            case VK_FORMAT_R8G8B8_UINT: return PKTextureFormat::RGB8_Uint;
            case VK_FORMAT_R8G8B8_SINT: return PKTextureFormat::RGB8_Int;
            case VK_FORMAT_R32_SFLOAT: return PKTextureFormat::R32_Float;
            case VK_FORMAT_R32_UINT: return PKTextureFormat::R32_Uint;
            case VK_FORMAT_R32_SINT: return PKTextureFormat::R32_Int;
            case VK_FORMAT_R16G16_SFLOAT: return PKTextureFormat::RG16_Float;
            case VK_FORMAT_R16G16_UINT: return PKTextureFormat::RG16_Uint;
            case VK_FORMAT_R16G16_SINT: return PKTextureFormat::RG16_Int;
            case VK_FORMAT_R8G8B8A8_UNORM: return PKTextureFormat::RGBA8_Unorm;
            case VK_FORMAT_R8G8B8A8_SNORM: return PKTextureFormat::RGBA8_Snorm;
            case VK_FORMAT_R8G8B8A8_SRGB: return PKTextureFormat::RGBA8_Srgb;
            case VK_FORMAT_B8G8R8A8_UINT: return PKTextureFormat::RGBA8_Uint;
            case VK_FORMAT_B8G8R8A8_SINT: return PKTextureFormat::RGBA8_Int;
            case VK_FORMAT_B8G8R8A8_UNORM: return PKTextureFormat::BGRA8_Unorm;
            case VK_FORMAT_B8G8R8A8_SNORM: return PKTextureFormat::BGRA8_Srgb;
            case VK_FORMAT_R16G16B16_SFLOAT: return PKTextureFormat::RGB16_Float;
            case VK_FORMAT_R16G16B16_UINT: return PKTextureFormat::RGB16_Uint;
            case VK_FORMAT_R16G16B16_SINT: return PKTextureFormat::RGB16_Int;
            case VK_FORMAT_R32G32_SFLOAT: return PKTextureFormat::RG32_Float;
            case VK_FORMAT_R32G32_UINT: return PKTextureFormat::RG32_Uint;
            case VK_FORMAT_R32G32_SINT: return PKTextureFormat::RG32_Int;
            case VK_FORMAT_R16G16B16A16_UNORM: return PKTextureFormat::RGBA16_Unorm;
            case VK_FORMAT_R16G16B16A16_SFLOAT: return PKTextureFormat::RGBA16_Float;
            case VK_FORMAT_R16G16B16A16_UINT: return PKTextureFormat::RGBA16_Uint;
            case VK_FORMAT_R16G16B16A16_SINT: return PKTextureFormat::RGBA16_Int;
            case VK_FORMAT_R32G32B32_SFLOAT: return PKTextureFormat::RGB32_Float;
            case VK_FORMAT_R32G32B32_UINT: return PKTextureFormat::RGB32_Uint;
            case VK_FORMAT_R32G32B32_SINT: return PKTextureFormat::RGB32_Int;
            case VK_FORMAT_R32G32B32A32_SFLOAT: return PKTextureFormat::RGBA32_Float;
            case VK_FORMAT_R32G32B32A32_UINT: return PKTextureFormat::RGBA32_Uint;
            case VK_FORMAT_R32G32B32A32_SINT: return PKTextureFormat::RGBA32_Int;
            case VK_FORMAT_R64G64B64A64_UINT: return PKTextureFormat::RGBA64_Uint;
            case VK_FORMAT_R5G6B5_UNORM_PACK16: return PKTextureFormat::RGB565_Unorm;
            case VK_FORMAT_R4G4B4A4_UNORM_PACK16: return PKTextureFormat::RGBA4_Unorm;
            case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return PKTextureFormat::RGB10A2_Unorm;
            case VK_FORMAT_R5G5B5A1_UNORM_PACK16: return PKTextureFormat::RGB5A1_Unorm;
            case VK_FORMAT_B10G11R11_UFLOAT_PACK32: return PKTextureFormat::B10G11R11U_Float;
            case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: return PKTextureFormat::RGB9E5_Float;
            case VK_FORMAT_S8_UINT: return PKTextureFormat::Stencil8;
            case VK_FORMAT_D16_UNORM: return PKTextureFormat::Depth16;
            case VK_FORMAT_D32_SFLOAT: return PKTextureFormat::Depth32_Float;
            case VK_FORMAT_D24_UNORM_S8_UINT: return PKTextureFormat::Depth24_Stencil8;
            case VK_FORMAT_D32_SFLOAT_S8_UINT: return PKTextureFormat::Depth32_Float_Stencil8;
            case VK_FORMAT_BC1_RGB_UNORM_BLOCK: return PKTextureFormat::BC1_RGB;
            case VK_FORMAT_BC1_RGB_SRGB_BLOCK: return PKTextureFormat::BC1_RGB_Srgb;
            case VK_FORMAT_BC1_RGBA_UNORM_BLOCK: return PKTextureFormat::BC1_RGBA;
            case VK_FORMAT_BC1_RGBA_SRGB_BLOCK: return PKTextureFormat::BC1_RGBA_Srgb;
            case VK_FORMAT_BC3_UNORM_BLOCK: return PKTextureFormat::BC3_RGBA;
            case VK_FORMAT_BC3_SRGB_BLOCK: return PKTextureFormat::BC3_RGBA_Srgb;
            case VK_FORMAT_BC4_UNORM_BLOCK: return PKTextureFormat::BC4_R_Unorm;
            case VK_FORMAT_BC4_SNORM_BLOCK: return PKTextureFormat::BC4_R_Snorm;
            case VK_FORMAT_BC5_UNORM_BLOCK: return PKTextureFormat::BC5_RG_Unorm;
            case VK_FORMAT_BC5_SNORM_BLOCK: return PKTextureFormat::BC5_RG_Snorm;
            case VK_FORMAT_BC6H_UFLOAT_BLOCK: return PKTextureFormat::BC6H_RGB_Ufloat;
            case VK_FORMAT_BC6H_SFLOAT_BLOCK: return PKTextureFormat::BC6H_RGB_Sfloat;
            case VK_FORMAT_BC7_UNORM_BLOCK: return PKTextureFormat::BC7_RGBA;
            default: return PKTextureFormat::Invalid;
        }
    }

    int BlockCompressBCUnorm(BlockCompressContext* ctx, void* dstData, size_t* dstSize)
    {
        if (!ctx || ctx->width == 0u || ctx->height == 0u || ctx->width % 4u != 0u || ctx->height % 4u != 0u || !ctx->src_data)
        {
            return -1;
        }

        size_t src_bpp = 0u;
        size_t dst_bpp = 0u;

        switch (ctx->src_format)
        {
            case PKTextureFormat::R8_Unorm:
            case PKTextureFormat::R8_Snorm:
                src_bpp = 1ull;
                break;
            case PKTextureFormat::RG8_Unorm:
            case PKTextureFormat::RG8_Snorm:
                src_bpp = 2ull;
                break;
            case PKTextureFormat::RGB8_Unorm:
            case PKTextureFormat::RGB8_Srgb:
            case PKTextureFormat::RGB8_Snorm:
                src_bpp = 3ull;
                break;
            case PKTextureFormat::RGBA8_Unorm:
            case PKTextureFormat::RGBA8_Srgb:
            case PKTextureFormat::RGBA8_Snorm:
            case PKTextureFormat::BGRA8_Unorm:
            case PKTextureFormat::BGRA8_Srgb:
                src_bpp = 4ull;
                break;
            default:
                printf("Unsupported source format!\n");
                return -1;
        }

        switch (ctx->dst_format)
        {
            case PKTextureFormat::BC1_RGB:
            case PKTextureFormat::BC1_RGBA:
            case PKTextureFormat::BC1_RGB_Srgb:
            case PKTextureFormat::BC1_RGBA_Srgb:
            case PKTextureFormat::BC4_R_Unorm:
            case PKTextureFormat::BC4_R_Snorm:
                dst_bpp = 4ull;
                break;
            case PKTextureFormat::BC3_RGBA: 
            case PKTextureFormat::BC3_RGBA_Srgb:
            case PKTextureFormat::BC5_RG_Unorm:
            case PKTextureFormat::BC5_RG_Snorm:
            case PKTextureFormat::BC7_RGBA:
                dst_bpp = 8ull;
                break;
            default:
                printf("Unsupported destination format!\n");
                return -1;
        }

        auto bytes_per_block = (16ull * dst_bpp) / 8ull;
        auto blocks_x = ctx->width / 4ull;
        auto blocks_y = ctx->height / 4ull;
        auto total_blocks = blocks_x * blocks_y;
        auto data_size = total_blocks * bytes_per_block;

        if (!dstData)
        {
            *dstSize = data_size;
            return 0;
        }

        if (*dstSize != data_size)
        {
            printf("Destination data size missmatch!\n");
            return -1;
        }
        
        rgbcx::init(ctx->bc1_mode);
        bc7enc_compress_block_init();

        bc7enc_compress_block_params block_params{};
        bc7enc_compress_block_params_init(&block_params);
        
        if (!ctx->perceptual)
        {
            bc7enc_compress_block_params_init_linear_weights(&block_params);
        }

        block_params.m_max_partitions = ctx->bc7enc_max_partitions_to_scan;
        block_params.m_uber_level = ctx->bc7_uber_level > BC7ENC_MAX_UBER_LEVEL ? BC7ENC_MAX_UBER_LEVEL : ctx->bc7_uber_level;

        if (ctx->bc7enc_mode6_only)
        {
            block_params.m_mode_mask = 1 << 6;
        }

        auto head = reinterpret_cast<uint8_t*>(dstData);

        for (auto by = 0u; by < blocks_y; ++by)
        for (auto bx = 0u; bx < blocks_x; ++bx)
        {
            uint8_t pixels[4ull * 16ull]{};

            for (auto yy = 0u; yy < 4u; ++yy)
            for (auto xx = 0u; xx < 4u; ++xx)
            {
                auto index_src = (by * 4u + yy) * ctx->width + (bx * 4u + xx);
                auto index_dst = yy * 4u + xx;
                memcpy(&pixels[0] + index_dst * 4ull, ctx->src_data + index_src * src_bpp, src_bpp);
            }

            switch (ctx->dst_format)
            {
                case PKTextureFormat::BC1_RGB:
                case PKTextureFormat::BC1_RGBA:
                case PKTextureFormat::BC1_RGB_Srgb:
                case PKTextureFormat::BC1_RGBA_Srgb:
                {
                    rgbcx::encode_bc1(ctx->bc1_quality_level, head, pixels, ctx->use_bc1_3color_mode, ctx->use_bc1_3color_mode_for_black);
                }
                break;
                case PKTextureFormat::BC3_RGBA: 
                case PKTextureFormat::BC3_RGBA_Srgb: 
                {
                    if (ctx->use_hq_bc345)
                    {
                        rgbcx::encode_bc3_hq(ctx->bc1_quality_level, head, pixels, ctx->bc345_search_rad, ctx->bc345_mode_mask);
                    }
                    else
                    {
                        rgbcx::encode_bc3(ctx->bc1_quality_level, head, pixels);
                    }
                }
                break;
                case PKTextureFormat::BC4_R_Unorm:
                case PKTextureFormat::BC4_R_Snorm:
                {
                    if (ctx->use_hq_bc345)
                    {
                        rgbcx::encode_bc4_hq(head, pixels, 4, ctx->bc345_search_rad, ctx->bc345_mode_mask);
                    }
                    else
                    {
                        rgbcx::encode_bc4(head, pixels, 4);
                    }
                }
                break;
                case PKTextureFormat::BC5_RG_Unorm:
                case PKTextureFormat::BC5_RG_Snorm:
                {
                    if (ctx->use_hq_bc345)
                    {
                        rgbcx::encode_bc5_hq(head, pixels, 0u, 1u, 4, ctx->bc345_search_rad, ctx->bc345_mode_mask);
                    }
                    else
                    {
                        rgbcx::encode_bc5(head, pixels, 0u, 1u, 4);
                    }
                }
                break;
                case PKTextureFormat::BC7_RGBA:
                {
                    bc7enc_compress_block(head, pixels, &block_params);
                }
                break;

                default: break;
            }

            head += bytes_per_block;
        }

        return 0;
    }
}
