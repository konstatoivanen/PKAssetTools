#include <stdio.h>
#include <stdlib.h>
#include <filesystem>
#include <queue>
#include <map>
#if PK_DEBUG
#include <PKAssetLoader.h>
#endif
#include <PKAssetEncoding.h>
#include "PKAssetWriter.h"

namespace PKAssets
{
    constexpr static const float MIN_COMPRESSION_RATIO = 0.75f;

    void WriteName(char* dst, const char* src)
    {
        auto c = strlen(src);

        if (c >= PK_ASSET_NAME_MAX_LENGTH)
        {
            c = PK_ASSET_NAME_MAX_LENGTH - 1;
        }

        memcpy(dst, src, c);
        memset(dst + c, '\0', PK_ASSET_NAME_MAX_LENGTH - c);
    }

    int WriteAsset(const char* filepath, PKAssetBuffer& buffer, bool forceNoCompression)
    {
        printf("Writing asset: %s ", filepath);

        FILE* file = nullptr;

        auto path = std::filesystem::path(filepath).remove_filename().string();
        path = path.substr(0, path.length() - 1);

        if (!std::filesystem::exists(path))
        {
            try
            {
                std::filesystem::create_directories(path);
            }
            catch (std::exception& e)
            {
                printf(" Failed: \n %s", e.what());
            }
        }

#if _WIN32
        auto error = fopen_s(&file, filepath, "wb");

        if (error != 0)
        {
            printf(" Failed: \n Failed to open/create file! \n");
            return -1;
        }
#else
        file = fopen(filepath, "wb");
#endif

        if (file == nullptr)
        {
            printf(" Failed: \n Failed to open/create file! \n");
            return -1;
        }

        // Add Padding to 64 bit boundary for more optimal reads.
        while ((buffer.size() % 8ull) != 0ull)
        {
            uint8_t padding = 0u;
            buffer.Write<uint8_t>(&padding, 1u);
        }

        buffer.header->uncompressedSize = buffer.size();

        auto useCompression = !forceNoCompression;
        auto compressionRatio = 1.0;

        if (useCompression)
        {
            auto* srcData = buffer.data() + sizeof(PKAssetHeader);
            auto srcSize = buffer.header->uncompressedSize - sizeof(PKAssetHeader);
            
            PKEncodeTable table{};
            EncodeBuffer(srcData, srcSize, &table, nullptr);

            compressionRatio = (double)(table.size + sizeof(PKAssetHeader)) / (double)buffer.size();
            useCompression &= compressionRatio <= MIN_COMPRESSION_RATIO;

            if (useCompression)
            {
                PKAssetBuffer compact;
                compact.header[0] = buffer.header[0];
                compact.header->isCompressed = true;
                auto pData = compact.Allocate<uint8_t>(table.size);
                EncodeBuffer(srcData, srcSize, &table, pData.get());
                fwrite(compact.data(), sizeof(char), compact.size(), file);
            }
        }
        
        if (!useCompression)
        {
            fwrite(buffer.data(), sizeof(char), buffer.size(), file);
        }

        if (fclose(file) != 0)
        {
            printf(" Failed: \n Failed to close file! \n");
            return -1;
        }

#if PK_DEBUG
        PKAsset asset;
        PKAssets::OpenAsset(filepath, &asset);

        auto charData = reinterpret_cast<char*>(asset.rawData);

        for (auto i = 0ull; i < buffer.size(); ++i)
        {
            if (charData[i] != buffer.data()[i])
            {
                printf(" Failed: \n Read Write missmatch at byte index: %lli \n", i);
            }
        }

        PKAssets::CloseAsset(&asset);
#endif

        if (useCompression)
        {
            printf(" Success: compression ratio %4.2f \n", (float)compressionRatio * 100.0f);
        }
        else
        {
            printf(" Success \n");
        }

        return 0;
    }
}
