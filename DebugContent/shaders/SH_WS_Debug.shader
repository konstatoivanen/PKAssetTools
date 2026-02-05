#version 450

#pragma pk_cull Back
#pragma pk_ztest LessEqual
#pragma pk_zwrite True

#pragma pk_material_property float4 _Color
#pragma pk_material_property float _Roughness
#pragma pk_material_property float _Metallic
#pragma pk_material_property texture2D _AlbedoTex
#pragma pk_material_property texture3D _LutTex

#pragma pk_multi_compile _ PASS_COLOR
#pragma pk_program SHADER_STAGE_VERTEX MainVs 
#pragma pk_program SHADER_STAGE_FRAGMENT MainFs
#pragma pk_program SHADER_STAGE_FRAGMENT ColorFs PASS_COLOR

#include "includes/DebugUtilities.glsl"

PK_DECLARE_LOCAL_CBUFFER(MyPushConstantBlock)
{
    float4 offset_x;
    float4 unused_x;
};

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

[pk_local(MainVs)] in float3 in_POSITION;
[pk_local(MainVs)] in float2 in_TEXCOORD0;
[pk_local(MainVs)] in float3 in_COLOR;
[pk_local(MainVs)] out float3 vs_COLOR;
[pk_local(MainVs)] out float2 vs_TEXCOORD0;

[pk_local(MainVs, MainFs)] uniform image2D pk_DebugImage;
[pk_local(MainFs)] uniform RWBuffer<float4> _WriteBuffer;
[pk_local(MainFs)] uniform Buffer<float4,1u> _ReadVariable;
[pk_local(MainFs)] uniform sampler2D tex1;
[pk_local(MainFs)] uniform sampler smp;
[pk_local(STAGE_FRAGMENT)] in float3 vs_COLOR;
[pk_local(STAGE_FRAGMENT)] in float2 vs_TEXCOORD0;
[pk_local(STAGE_FRAGMENT)] out float4 outColor;

[pk_local(MainVs)]
void TestFunc()
{
    vs_COLOR = in_COLOR;
}

[pk_local(MainVs)]
struct TestStruct
{
    float3 testfield;
};

void MainVs()
{
    gl_Position = mul(mul(viewproj, model), float4(in_POSITION + offset_x.xyz, 1.0f));
    vs_COLOR = in_COLOR;
    vs_TEXCOORD0 = in_TEXCOORD0;
    vs_TEXCOORD0.x += gl_VertexIndex;
}

void MainFs()
{
    float3 albedo = texture(sampler2D(_AlbedoTex, smp), vs_TEXCOORD0).xyz;
    float3 texcolor = texture(tex1, vs_TEXCOORD0).xyz + _ReadVariable.xyz;
    outColor = float4(albedo * texcolor * vs_COLOR * _Color.rgb, 1.0);
    imageStore(pk_DebugImage, int2(outColor.xy * 1024), float4(outColor.x, 0, 0, 0));
    _WriteBuffer[uint(outColor.x * 1024)] = outColor;
}

void ColorFs() { outColor = float4(vs_COLOR, 1.0f); }

