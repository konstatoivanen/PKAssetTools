#version 450

#pragma pk_cull Back
#pragma pk_ztest LEqual
#pragma pk_zwrite True
#pragma pk_with_atomic_counter

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

[[pk_restrict MainVs]] in float3 in_POSITION;
[[pk_restrict MainVs]] in float2 in_TEXCOORD0;
[[pk_restrict MainVs]] in float3 in_COLOR;
[[pk_restrict MainVs]] out float3 vs_COLOR;
[[pk_restrict MainVs]] out float2 vs_TEXCOORD0;

[[pk_restrict MainFs]] layout(rg16f, set = PK_SET_SHADER) uniform image2D pk_DebugImage;
[[pk_restrict MainFs]] PK_DECLARE_BUFFER(float4, _WriteBuffer, PK_SET_DRAW);
[[pk_restrict MainFs]] layout(set = 3) uniform sampler2D tex1;
[[pk_restrict MainFs]] layout(set = 3) uniform sampler smp;
[[pk_restrict MainFs ColorFs]] in float3 vs_COLOR;
[[pk_restrict MainFs ColorFs]] in float2 vs_TEXCOORD0;
[[pk_restrict MainFs ColorFs]] out float4 outColor;

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
    float3 texcolor = texture(tex1, vs_TEXCOORD0).xyz;
    outColor = float4(albedo * texcolor * vs_COLOR * _Color.rgb, 1.0);
    imageStore(pk_DebugImage, int2(outColor.xy * 1024), float4(outColor.x, 0, 0, 0));
    PK_BUFFER_DATA(_WriteBuffer, uint(outColor.x * 1024)) = outColor;
}
void ColorFs() { outColor = float4(vs_COLOR, 1.0f); }
