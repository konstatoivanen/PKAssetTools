#pragma once
#include "PKAssetWriter.h"
#include <stdio.h>
#include <stdlib.h>

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
        dst[c] = '\0';
    }

    int WriteAsset(const char* filepath, const PKAssetBuffer& buffer)
    {
        FILE* file = nullptr;

#if _WIN32
        auto error = fopen_s(&file, filepath, "wb");

        if (error != 0)
        {
            return -1;
        }
#else
        file = fopen(filepath, "wb");
#endif

        if (file == nullptr)
        {
            return -1;
        }

        fwrite(buffer.data(), sizeof(char), buffer.size(), file);

        return fclose(file);
    }
}