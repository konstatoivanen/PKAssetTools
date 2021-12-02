#pragma once
#include "PKAssetWriter.h"
#include <stdio.h>
#include <stdlib.h>
#include <filesystem>

namespace PK::Assets
{
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

    int WriteAsset(const char* filepath, const PKAssetBuffer& buffer)
    {
        printf("writing file: %s \n", filepath);

        FILE* file = nullptr;

        auto path = std::filesystem::path(filepath).remove_filename();

        if (!std::filesystem::exists(path))
        {
            std::filesystem::create_directory(path);
        }

#if _WIN32
        auto error = fopen_s(&file, filepath, "wb");

        if (error != 0)
        {
            printf("Failed to open/create file! \n");
            return -1;
        }
#else
        file = fopen(filepath, "wb");
#endif

        if (file == nullptr)
        {
            printf("Failed to open/create file! \n");
            return -1;
        }

        fwrite(buffer.data(), sizeof(char), buffer.size(), file);

        if (fclose(file) != 0)
        {
            printf("Failed to close file! \n");
            return -1;
        }

        return 0;
    }
}