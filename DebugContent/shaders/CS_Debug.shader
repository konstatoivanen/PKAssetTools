#version 450
#pragma pk_program SHADER_STAGE_COMPUTE main

layout(rgba8, set = 4) uniform image2D g_Texture;

shared float g_test;
shared float g_ModTime;

layout(local_size_x = 16, local_size_y = 4, local_size_z = 1) in;
void main()
{
    if (gl_LocalInvocationIndex == 0)
    {
        g_ModTime = mod(float(gl_WorkGroupID.x), 10.0f);
    }

    barrier();

    int2 coord = int2(gl_GlobalInvocationID.xy);
    int2 size = imageSize(g_Texture).xy;
    float4 value = float4(float2(coord + 0.5f.xx) / float2(size) *g_ModTime * 10.0f, 1.0f, 1.0f);
    imageStore(g_Texture, coord, value);
}