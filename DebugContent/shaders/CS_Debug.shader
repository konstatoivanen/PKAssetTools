#version 450
#pragma pk_program SHADER_STAGE_COMPUTE main

#include "includes/DebugUtilities.glsl"

layout(rgba8, set = 4) uniform image2D g_Texture;

uniform vec2 unused0;
uniform vec2 unused1;
uniform vec2 uScale;
uniform vec2 uTranslate;

PK_DECLARE_LOCAL_CBUFFER(MyPushConstantBlock)
{
    float4 offset_x;
    float4 unused_x;
};

hitAttributeEXT float2 invalidAttributeDeclaration;

uniform vec2 unused3;
shared float g_test;
shared float g_ModTime;

[pk_numthreads(16,4,1)]
void main()
{
    if (gl_LocalInvocationIndex == 0)
    {
        g_ModTime = mod(float(gl_WorkGroupID.x), 10.0f);
    }

    barrier();

    int2 coord = int2(gl_GlobalInvocationID.xy);
    int2 size = imageSize(g_Texture).xy;
    float4 value = float4(float2(coord + 0.5f.xx) / float2(size) * g_ModTime * 10.0f + uTranslate, 1.0f, 1.0f) * uScale.xyxy;
    imageStore(g_Texture, coord, value);

    printf("Debug text: %f", uscale.x);
}
