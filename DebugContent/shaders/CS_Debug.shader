#pragma pk_program Compute main

#include "includes/DebugUtilities.glsl"

uniform RWTexture2D<float4> g_Texture;

uniform float2 unused0;
uniform float2 unused1;
uniform float2 uScale;
uniform float2 uTranslate;
uniform float4 offset_x;
uniform float4 unused_x;
uniform float2 unused3;

groupshared float g_test;
groupshared float g_ModTime;

[numthreads(16, 4, 1)]
void main(uint thread : SV_GroupIndex, uint3 group_coord : SV_GroupID, int2 coord : SV_DispatchThreadID)
{
    if (thread == 0u)
    {
        g_ModTime = mod(float(group_coord.x), 10.0f);
    }

    GroupMemoryBarrierWithGroupSync();

    int2 size = g_Texture.GetSize().xy;
    float4 value = float4(float2(coord + 0.5f.xx) / float2(size) * g_ModTime * 10.0f + uTranslate, 1.0f, 1.0f) * uScale.xyxy;
    g_Texture[coord] = value;

    printf("Debug text: %f", uscale.x);
}