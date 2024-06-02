#pragma once
#include <cstdint>

namespace PKAssets::Mesh
{
    uint16_t PackHalf(float v);

    uint8_t PackUnorm8(float v);

    uint32_t PackUnorm12(float v);

    void OctaEncode(const float* n, float* outuv);

    inline uint32_t asuint(float v) { return *reinterpret_cast<uint32_t*>(&v); }
    inline float asfloat(uint32_t u) { return *reinterpret_cast<float*>(&u); }
}
