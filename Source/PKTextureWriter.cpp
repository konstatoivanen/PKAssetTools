#pragma once
#define KHRONOS_STATIC
#include <KTX/ktx.h>
#include <KTX/vulkan_format.h>
#include "PKStringUtilities.h"
#include "PKAssetWriter.h"
#include "PKFileVersionUtilities.h"

namespace PKAssets::Texture
{
    // Same as main repo. Unfortunate but whatever.
    PKTextureFormat VkFormatToPKTextureFormat(VkFormat format)
    {
        switch (format)
        {
            case VK_FORMAT_R8_UNORM:                  return PKTextureFormat::R8;
            case VK_FORMAT_R8_SNORM:                  return PKTextureFormat::R8_SNORM;
            case VK_FORMAT_R8_UINT:                   return PKTextureFormat::R8UI;
            case VK_FORMAT_R8_SINT:                   return PKTextureFormat::R8I;
            case VK_FORMAT_S8_UINT:                   return PKTextureFormat::Stencil8;
            case VK_FORMAT_R16_SFLOAT:                return PKTextureFormat::R16F;
            case VK_FORMAT_R16_UINT:                  return PKTextureFormat::R16UI;
            case VK_FORMAT_R16_SINT:                  return PKTextureFormat::R16I;
            case VK_FORMAT_R8G8_UNORM:                return PKTextureFormat::RG8;
            case VK_FORMAT_R8G8_SNORM:                return PKTextureFormat::RG8_SNORM;
            case VK_FORMAT_R8G8_UINT:                 return PKTextureFormat::RG8UI;
            case VK_FORMAT_R8G8_SINT:                 return PKTextureFormat::RG8I;
            case VK_FORMAT_R5G6B5_UNORM_PACK16:       return PKTextureFormat::RGB565;
            case VK_FORMAT_R5G5B5A1_UNORM_PACK16:     return PKTextureFormat::RGB5A1;
            case VK_FORMAT_R4G4B4A4_UNORM_PACK16:     return PKTextureFormat::RGBA4;
            case VK_FORMAT_D16_UNORM:                 return PKTextureFormat::Depth16;
            case VK_FORMAT_R8G8B8_UNORM:              return PKTextureFormat::RGB8;
            case VK_FORMAT_R8G8B8_SRGB:               return PKTextureFormat::RGB8_SRGB;
            case VK_FORMAT_R8G8B8_SNORM:              return PKTextureFormat::RGB8_SNORM;
            case VK_FORMAT_R8G8B8_UINT:               return PKTextureFormat::RGB8UI;
            case VK_FORMAT_R8G8B8_SINT:               return PKTextureFormat::RGB8I;
            case VK_FORMAT_R32_SFLOAT:                return PKTextureFormat::R32F;
            case VK_FORMAT_R32_UINT:                  return PKTextureFormat::R32UI;
            case VK_FORMAT_R32_SINT:                  return PKTextureFormat::R32I;
            case VK_FORMAT_R16G16_SFLOAT:             return PKTextureFormat::RG16F;
            case VK_FORMAT_R16G16_UINT:               return PKTextureFormat::RG16UI;
            case VK_FORMAT_R16G16_SINT:               return PKTextureFormat::RG16I;
            case VK_FORMAT_B10G11R11_UFLOAT_PACK32:   return PKTextureFormat::B10G11R11UF;
            case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:    return PKTextureFormat::RGB9E5;
            case VK_FORMAT_R8G8B8A8_UNORM:            return PKTextureFormat::RGBA8;
            case VK_FORMAT_R8G8B8A8_SRGB:             return PKTextureFormat::RGBA8_SRGB;
            case VK_FORMAT_R8G8B8A8_SNORM:            return PKTextureFormat::RGBA8_SNORM;
            case VK_FORMAT_B8G8R8A8_UNORM:            return PKTextureFormat::BGRA8;
            case VK_FORMAT_B8G8R8A8_SRGB:             return PKTextureFormat::BGRA8_SRGB;
            case VK_FORMAT_A2B10G10R10_UNORM_PACK32:  return PKTextureFormat::RGB10A2;
            case VK_FORMAT_R8G8B8A8_UINT:             return PKTextureFormat::RGBA8UI;
            case VK_FORMAT_R8G8B8A8_SINT:             return PKTextureFormat::RGBA8I;
            case VK_FORMAT_D32_SFLOAT:                return PKTextureFormat::Depth32F;
            case VK_FORMAT_D24_UNORM_S8_UINT:         return PKTextureFormat::Depth24_Stencil8;
            case VK_FORMAT_D32_SFLOAT_S8_UINT:        return PKTextureFormat::Depth32F_Stencil8;
            case VK_FORMAT_R16G16B16_SFLOAT:          return PKTextureFormat::RGB16F;
            case VK_FORMAT_R16G16B16_UINT:            return PKTextureFormat::RGB16UI;
            case VK_FORMAT_R16G16B16_SINT:            return PKTextureFormat::RGB16I;
            case VK_FORMAT_R32G32_SFLOAT:             return PKTextureFormat::RG32F;
            case VK_FORMAT_R32G32_UINT:               return PKTextureFormat::RG32UI;
            case VK_FORMAT_R32G32_SINT:               return PKTextureFormat::RG32I;
            case VK_FORMAT_R16G16B16A16_UNORM:        return PKTextureFormat::RGBA16;
            case VK_FORMAT_R16G16B16A16_SFLOAT:       return PKTextureFormat::RGBA16F;
            case VK_FORMAT_R16G16B16A16_UINT:         return PKTextureFormat::RGBA16UI;
            case VK_FORMAT_R16G16B16A16_SINT:         return PKTextureFormat::RGBA16I;
            case VK_FORMAT_R32G32B32_SFLOAT:          return PKTextureFormat::RGB32F;
            case VK_FORMAT_R32G32B32_UINT:            return PKTextureFormat::RGB32UI;
            case VK_FORMAT_R32G32B32_SINT:            return PKTextureFormat::RGB32I;
            case VK_FORMAT_R32G32B32A32_SFLOAT:       return PKTextureFormat::RGBA32F;
            case VK_FORMAT_R32G32B32A32_UINT:         return PKTextureFormat::RGBA32UI;
            case VK_FORMAT_R32G32B32A32_SINT:         return PKTextureFormat::RGBA32I;
            case VK_FORMAT_R64G64B64A64_UINT:         return PKTextureFormat::RGBA64UI;
            case VK_FORMAT_BC1_RGB_UNORM_BLOCK:       return PKTextureFormat::BC1_RGB;
            case VK_FORMAT_BC1_RGB_SRGB_BLOCK:        return PKTextureFormat::BC1_SRGB;
            case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:      return PKTextureFormat::BC1_RGBA;
            case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:       return PKTextureFormat::BC1_SRGBA;
            case VK_FORMAT_BC2_UNORM_BLOCK:           return PKTextureFormat::BC2_RGBA;
            case VK_FORMAT_BC2_SRGB_BLOCK:            return PKTextureFormat::BC2_SRGBA;
            case VK_FORMAT_BC3_UNORM_BLOCK:           return PKTextureFormat::BC3_RGBA;
            case VK_FORMAT_BC3_SRGB_BLOCK:            return PKTextureFormat::BC3_SRGBA;
            case VK_FORMAT_BC4_UNORM_BLOCK:           return PKTextureFormat::BC4;
            case VK_FORMAT_BC6H_UFLOAT_BLOCK:         return PKTextureFormat::BC6H_RGBUF;
            case VK_FORMAT_BC6H_SFLOAT_BLOCK:         return PKTextureFormat::BC6H_RGBF;
            case VK_FORMAT_BC7_UNORM_BLOCK:           return PKTextureFormat::BC7_UNORM;
            default:                                  return PKTextureFormat::Invalid;
        }
    }

    int WriteTexture(const char* pathSrc, const char* pathDst, const size_t pathStemOffset)
    {
        if (!PKVersionUtilities::IsFileOutOfDate(pathSrc, pathDst))
        {
            return 1;
        }

        auto filename = StringUtilities::ReadFileName(pathSrc);

        ktxTexture2* ktxTex2;

        auto result = ktxTexture2_CreateFromNamedFile(pathSrc, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTex2);

        if (result != KTX_SUCCESS)
        {
            printf("Failed to load KTX texture: %s", ktxErrorString(result));
            return -1;
        }

        ktx_uint8_t* ktxTextureData = ktxTexture_GetData(ktxTexture(ktxTex2));
        ktx_size_t ktxTextureSize = ktxTex2->dataSize;

        auto buffer = PKAssetBuffer();
        buffer.header->type = PKAssetType::Texture;
        WriteName(buffer.header->name, filename.c_str());

        auto pkTexture = buffer.Allocate<PKTexture>();
        pkTexture->resolution[0] = (uint16_t)ktxTex2->baseWidth;
        pkTexture->resolution[1] = (uint16_t)ktxTex2->baseHeight;
        pkTexture->resolution[2] = (uint16_t)ktxTex2->baseDepth;
        pkTexture->layers = ktxTex2->numLayers;
        pkTexture->levels = ktxTex2->numLevels;
        pkTexture->anisotropy = 16.0f;
        pkTexture->filterMin = ktxTex2->numLevels > 1 ? PKFilterMode::Trilinear : PKFilterMode::Bilinear;
        pkTexture->filterMag = ktxTex2->numLevels > 1 ? PKFilterMode::Trilinear : PKFilterMode::Bilinear;
        pkTexture->wrap[0] = PKWrapMode::Repeat;
        pkTexture->wrap[1] = PKWrapMode::Repeat;
        pkTexture->wrap[2] = PKWrapMode::Repeat;
        pkTexture->borderColor = PKBorderColor::FloatClear;
        pkTexture->format = VkFormatToPKTextureFormat((VkFormat)ktxTex2->vkFormat);
        pkTexture->type = PKTextureType::Texture2D;
        pkTexture->dataSize = (uint32_t)ktxTextureSize;

        if (ktxTex2->isCubemap && ktxTex2->isArray)
        {
            pkTexture->type = PKTextureType::CubemapArray;
        }
        else if (ktxTex2->isCubemap)
        {
            pkTexture->type = PKTextureType::Cubemap;
        }
        else if (ktxTex2->isArray)
        {
            pkTexture->type = PKTextureType::Texture2DArray;
        }
        else if (ktxTex2->baseDepth > 1)
        {
            pkTexture->type = PKTextureType::Texture3D;
        }


        std::vector<uint32_t> levelOffsets;
        levelOffsets.resize(ktxTex2->numLevels);

        // KTX 2 stores all levels in tightly packed form. no need to iterate on other data.
        for (auto level = 0u; level < ktxTex2->numLevels; ++level)
        {
            size_t offset = 0ull;
            auto result = ktxTexture_GetImageOffset(ktxTexture(ktxTex2), level, 0, 0, &offset);
            levelOffsets[level] = (uint32_t)offset;
            
            if (result != KTX_SUCCESS)
            {
                printf("Failed to get image buffer offset");
                return -1;
            }
        }

        auto pData = buffer.Write(ktxTextureData, ktxTextureSize);
        auto pLevels = buffer.Write(levelOffsets.data(), levelOffsets.size());
        pkTexture->data.Set(buffer.data(), pData.get());
        pkTexture->levelOffsets.Set(buffer.data(), pLevels.get());

        ktxTexture_Destroy(ktxTexture(ktxTex2));

        return WriteAsset(pathDst, pathStemOffset, buffer, true);
    }
}