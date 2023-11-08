#pragma once
#include <cstdint>

namespace PK::Assets::Mesh
{
    uint16_t PackHalf(float v);
    void OctaEncode(const float* n, float* outuv);
}
