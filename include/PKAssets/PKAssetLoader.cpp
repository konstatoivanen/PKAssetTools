#pragma once
#include "PKAssetLoader.h"
#include <stdio.h>
#include <stdlib.h>

namespace PK::Assets
{
    PKAsset OpenAsset(const char* filepath)
    {
        PKAsset asset{};
        FILE* file = nullptr;

#if _WIN32
        auto error = fopen_s(&file, filepath, "rb");
        
        if (error != 0)
        {
            return asset;
        }
#else
        file = fopen(filepath, "rb");
#endif

        if (file == nullptr)
        {
            return asset;
        }

        fseek(file, 0, SEEK_END);
        auto size = ftell(file);
        rewind(file);

        if (size == 0)
        {
            fclose(file);
            return asset;
        }

        auto buffer = malloc(size);

        if (buffer == nullptr)
        {
            fclose(file);
            return asset;
        }

        asset.rawData = buffer;
        fread(buffer, sizeof(char), size, file);
        fclose(file);

        asset.header = reinterpret_cast<PKAssetHeader*>(asset.rawData);
        return asset;
    }

    void CloseAsset(PKAsset* asset)
    {
        if (asset->rawData == nullptr)
        {
            return;
        }
        
        asset->header = nullptr;
        free(asset->rawData);
    }

    Shader::PKShader* ReadAsShader(PKAsset* asset)
    {
        if (asset->header == nullptr || asset->header->type != PKAssetType::Shader)
        {
            return nullptr;
        }

        auto assetPtr = reinterpret_cast<char*>(asset->rawData) + sizeof(PKAssetHeader);
        return reinterpret_cast<Shader::PKShader*>(assetPtr);
    }

    Mesh::PKMesh* ReadAsMesh(PKAsset* asset)
    {
        if (asset->header == nullptr || asset->header->type != PKAssetType::Mesh)
        {
            return nullptr;
        }

        auto assetPtr = reinterpret_cast<char*>(asset->rawData) + sizeof(PKAssetHeader);
        return reinterpret_cast<Mesh::PKMesh*>(assetPtr);
    }
}