#include <malloc.h>
#include "PKTextureUtilities.h"

namespace PKAssets::Texture
{
    // stb_dxt.h - v1.12 - DXT1/DXT5 compressor - public domain
    // original by fabian "ryg" giesen - ported to C by stb
    /*
        MIT License
        Copyright (c) 2017 Sean Barrett
        Permission is hereby granted, free of charge, to any person obtaining a copy of
        this software and associated documentation files (the "Software"), to deal in
        the Software without restriction, including without limitation the rights to
        use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
        of the Software, and to permit persons to whom the Software is furnished to do
        so, subject to the following conditions:
        The above copyright notice and this permission notice shall be included in all
        copies or substantial portions of the Software.
        THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
        IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
        FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
        AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
        LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
        OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
        SOFTWARE.
    */
    static void CompressBlockBC4(unsigned char* dest, unsigned char* src, int stride)
    {
        int i, dist, bias, dist4, dist2, bits, mask;

        // find min/max color
        int mn, mx;
        mn = mx = src[0];

        for (i = 1; i < 16; i++)
        {
            if (src[i * stride] < mn)
            {
                mn = src[i * stride];

            }
            else if (src[i * stride] > mx)
            {
                mx = src[i * stride];
            }
        }

        // encode them
        dest[0] = (unsigned char)mx;
        dest[1] = (unsigned char)mn;
        dest += 2;

        // determine bias and emit color indices
        // given the choice of mx/mn, these indices are optimal:
        // http://fgiesen.wordpress.com/2009/12/15/dxt5-alpha-block-index-determination/
        dist = mx - mn;
        dist4 = dist * 4;
        dist2 = dist * 2;
        bias = (dist < 8) ? (dist - 1) : (dist / 2 + 2);
        bias -= mn * 7;
        bits = 0, mask = 0;

        for (i = 0; i < 16; i++) 
        {
            int a = src[i * stride] * 7 + bias;
            int ind, t;

            // select index. this is a "linear scale" lerp factor between 0 (val=min) and 7 (val=max).
            t = (a >= dist4) ? -1 : 0; ind = t & 4; a -= dist4 & t;
            t = (a >= dist2) ? -1 : 0; ind += t & 2; a -= dist2 & t;
            ind += (a >= dist);

            // turn linear scale into DXT index (0/1 are extremal pts)
            ind = -ind & 7;
            ind ^= (2 > ind);

            // write index
            mask |= ind << bits;

            if ((bits += 3) >= 8) 
            {
                *dest++ = (unsigned char)mask;
                mask >>= 8;
                bits -= 8;
            }
        }
    }

    int BlockCompressBC4(float* src, uint32_t w, uint32_t h, uint8_t* dst, size_t* dstSize)
    {
        if (w == 0u || h == 0u || w % 4u != 0u || h % 4u != 0u || !src)
        {
            return -1;
        }

        auto wb = w / 4ull;
        auto hb = h / 4ull;
        auto size = wb * hb * 8ull;
        *dstSize = size;

        if (!dst)
        {
            return 0;
        }

        for (auto yy = 0u; yy < hb; ++yy)
        for (auto xx = 0u; xx < wb; ++xx)
        {
            uint8_t block[16];

            for (auto by = 0u; by < 4u; ++by)
            for (auto bx = 0u; bx < 4u; ++bx)
            {
                auto index = (yy * 4u + by) * w + (xx * 4u + bx);
                auto value = src[index];
                auto quantized32 = static_cast<uint32_t>(value * 255.0f);
                auto quantized8 = static_cast<uint8_t>(quantized32 > 255u ? 255u : quantized32);
                block[by * 4u + bx] = quantized8;
            }

            CompressBlockBC4(dst, block, 1u);
            dst += 8ull;
        }

        return 0;
    }
}