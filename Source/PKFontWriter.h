#pragma once
namespace PKAssets::Font
{
    constexpr static const char* PK_ASSET_FONT_SRC_EXTENSION = ".ttf";

    int WriteFont(const char* pathSrc, const char* pathDst, const size_t pathStemOffset);
}