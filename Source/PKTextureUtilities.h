#pragma once
#include <stdint.h>

namespace PKAssets::Texture
{
    int BlockCompressBC4(float* src, uint32_t w, uint32_t h, uint8_t* dst, size_t* dstSize);
}