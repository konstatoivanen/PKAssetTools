#version 450

#Cull Back
#ZTest LEqual
#ZWrite True

#MaterialProperty float4 _Color
#MaterialProperty float _Roughness
#MaterialProperty float _Metallic
#MaterialProperty texture2D _AlbedoTex
#MaterialProperty texture3D _LutTex
#WithAtomicCounter

#include includes/Utilities.glsl

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

layout(push_constant) uniform offset
{
    float4 offset_x;
    float4 unused_x;
};

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

in float3 vs_COLOR;
in float2 vs_TEXCOORD0;
out float4 outColor;

void main()
{
    outColor = float4(tex2D(_AlbedoTex, vs_TEXCOORD0).xyz * tex2D(tex1, vs_TEXCOORD0).xyz * vs_COLOR * _Color.rgb, 1.0);
    imageStore(pk_DebugImage, int2(outColor.xy * 1024), float4(outColor.x, 0, 0, 0));
    PK_BUFFER_DATA(_WriteBuffer, uint(outColor.x * 1024)) = outColor;
}