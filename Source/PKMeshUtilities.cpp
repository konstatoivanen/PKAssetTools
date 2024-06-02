#include "PKMeshUtilities.h"
#include <corecrt_math.h>

namespace PKAssets::Mesh
{
    uint16_t PackHalf(float v)
    {
        if (v < -65536.0f)
        {
            v = -65536.0f;
        }
    
        if (v > 65536.0f)
        {
            v = 65536.0f;
        }
    
        v *= 1.925930e-34f;
        int32_t i = *(int*)&v;
        uint32_t ui = (uint32_t)i;
        return ((i >> 16) & (int)0xffff8000) | ((int)(ui >> 13));
    }

    uint8_t PackUnorm8(float v)
    {
        auto i = (int32_t)roundf(v * 255.0f);
        if (i < 0) { i = 0; }
        if (i > 255) { i = 255; }
        return (uint8_t)(i & 0xFFu);
    }

    uint32_t PackUnorm12(float v)
    {
        auto i = (int32_t)roundf(v * 4095.0f);
        if (i < 0) { i = 0; }
        if (i > 4095) { i = 4095; }
        return (uint32_t)(i & 0xFFFu);
    }
    
    float abs(float v) { return v < 0.0f ? -v : v; }

    void OctaEncode(const float* n, float* outuv)
    {
        float t[3] = { n[0], n[1], n[2] };
        float f = abs(n[0]) + abs(n[1]) + abs(n[2]);
        t[0] /= f;
        t[1] /= f;
        t[2] /= f;

        if (t[1] >= 0.0f)
        {
            outuv[0] = t[0];
            outuv[1] = t[2];
        }
        else
        {
            outuv[0] = (1.0f - abs(t[2])) * (t[0] >= 0.0f ? 1.0f : -1.0f);
            outuv[1] = (1.0f - abs(t[0])) * (t[2] >= 0.0f ? 1.0f : -1.0f);
        }

        outuv[0] = outuv[0] * 0.5f + 0.5f;
        outuv[1] = outuv[1] * 0.5f + 0.5f;
    }
}