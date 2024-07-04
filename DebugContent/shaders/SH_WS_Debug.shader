#version 450

#PK_Cull Back
#PK_ZTest LEqual
#PK_ZWrite True
#PK_MaterialProperty float4 _Color
#PK_MaterialProperty float _Roughness
#PK_MaterialProperty float _Metallic
#PK_MaterialProperty texture2D _AlbedoTex
#PK_MaterialProperty texture3D _LutTex
#PK_WithAtomicCounter

#include "includes/DebugUtilities.glsl"

PK_DECLARE_LOCAL_CBUFFER(MyPushConstantBlock)
{
    float4 offset_x;
    float4 unused_x;
};

#pragma PROGRAM_VERTEX

PK_DECLARE_CBUFFER(UniformBufferObject, 0)
{
    float4x4 model;
    float4x4 viewproj;
};

PK_DECLARE_CBUFFER(UniformBufferObject2, 10)
{
    float4x4 model;
    float4x4 viewproj;
} named;

in float3 in_POSITION;
in float2 in_TEXCOORD0;
in float3 in_COLOR;
out float3 vs_COLOR;
out float2 vs_TEXCOORD0;

void main()
{
    gl_Position = mul(mul(viewproj, model), float4(in_POSITION + offset_x.xyz, 1.0f));
    vs_COLOR = in_COLOR;
    vs_TEXCOORD0 = in_TEXCOORD0;
    vs_TEXCOORD0.x += gl_VertexIndex;
}

#pragma PROGRAM_FRAGMENT

layout(rg16f, set = PK_SET_SHADER) uniform image2D pk_DebugImage;

PK_DECLARE_BUFFER(float4, _WriteBuffer, PK_SET_DRAW);

layout(set = 3) uniform sampler2D tex1;
layout(set = 3) uniform sampler smp;

in float3 vs_COLOR;
in float2 vs_TEXCOORD0;
out float4 outColor;

void main()
{
    float3 albedo = texture(sampler2D(_AlbedoTex, smp), vs_TEXCOORD0).xyz;
    float3 texcolor = texture(tex1, vs_TEXCOORD0).xyz;
    outColor = float4(albedo * texcolor * vs_COLOR * _Color.rgb, 1.0);
    imageStore(pk_DebugImage, int2(outColor.xy * 1024), float4(outColor.x, 0, 0, 0));
    PK_BUFFER_DATA(_WriteBuffer, uint(outColor.x * 1024)) = outColor;
}