
#pragma pk_program Compute main
#pragma pk_log_verbose

#include "includes/DebugUtilities.glsl"

uniform RWTexture2DRef<float4> g_Textures[4];

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
void main(uint groupIndex : SV_GroupIndex, uint3 groupId : SV_GroupID, uint3 globalId : SV_DispatchThreadID)
{
    if (groupIndex == 0u)
    {
        g_ModTime = mod(float(groupId.x), 10.0f);
    }

    GroupMemoryBarrierWithGroupSync();

    int2 coord = int2(globalId.xy);
    int2 size = int2(g_Textures[0].GetSize());
    float4 value = float4((float2(coord) + 0.5f) / float2(size) * g_ModTime * 10.0f + uTranslate, 1.0f, 1.0f) * uScale.xyxy;
    g_Textures[0][coord] = value;
    g_Textures[1][coord] = value;
    g_Textures[2][coord] = value;
    g_Textures[3][coord] = value;

    printf("Debug text: %f", uScale.x);
}
