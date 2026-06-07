#pragma once

namespace PKAssets::IES
{
    constexpr static const char* PK_ASSET_IES_SRC_EXTENSION = ".ies";

    int WriteIESProfile(const char* pathSrc, const char* pathDst, const size_t pathStemOffset);
}