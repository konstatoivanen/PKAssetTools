#define KHRONOS_STATIC
#include <KTX/ktx.h>
#include <KTX/vulkan_format.h>
#include "PKStringUtilities.h"
#include "PKTextureUtilities.h"
#include "PKAssetWriter.h"
#include "PKFileVersionUtilities.h"

namespace PKAssets::Texture
{
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