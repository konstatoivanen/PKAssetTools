#version 450

#Cull Back
#ZTest LEqual
#ZWrite True

#include includes/HLSLSupport.glsl

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

layout(push_constant) uniform offset{ float4 offset_x; };

layout(location = 0) in float3 in_POSITION;
layout(location = 1) in float2 in_TEXCOORD0;
layout(location = 2) in float3 in_COLOR;

layout(location = 0) out float3 vs_COLOR;
layout(location = 1) out float2 vs_TEXCOORD0;

void main()
{
    gl_Position = mul(mul(viewproj, model), float4(in_POSITION + offset_x.xyz, 1.0f));
    vs_COLOR = in_COLOR;
    vs_TEXCOORD0 = in_TEXCOORD0;
    vs_TEXCOORD0.x += gl_VertexIndex;
}

#pragma PROGRAM_FRAGMENT

layout(set = 3) uniform sampler2D tex1;

layout(location = 0) in float3 vs_COLOR;
layout(location = 1) in float2 vs_TEXCOORD0;
layout(location = 0) out float4 outColor;

void main()
{
    outColor = float4(tex2D(tex1, vs_TEXCOORD0).xyz * vs_COLOR, 1.0);
}