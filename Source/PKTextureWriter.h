#pragma once
namespace PKAssets::Texture
{
    constexpr static const char* PK_ASSET_TEXTURE_SRC_EXTENSION = ".ktx2";

    int WriteTexture(const char* pathSrc, const char* pathDst);
}