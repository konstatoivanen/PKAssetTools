#version 460

#pragma pk_log_verbose

#pragma pk_cull Back
#pragma pk_ztest LessEqual
#pragma pk_zwrite True

#pragma pk_material_property float4 _Color
#pragma pk_material_property float _Roughness
#pragma pk_material_property float _Metallic
#pragma pk_material_property texture2D _AlbedoTex
#pragma pk_material_property texture3D _LutTex

#pragma pk_multi_compile _ PASS_COLOR
#pragma pk_program Vertex MainVs 
#pragma pk_program Fragment MainFs
#pragma pk_program Fragment ColorFs PASS_COLOR

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
[pk_local(MainFs)] uniform RWStructuredBuffer<float4> _WriteBuffer;
[pk_local(MainFs)] uniform StructuredBuffer<float4> _ReadVariable;
[pk_local(MainFs)] uniform sampler2D tex1;
[pk_local(MainFs)] uniform sampler smp;
[pk_local(Fragment)] in float3 vs_COLOR;
[pk_local(Fragment)] in float2 vs_TEXCOORD0;
[pk_local(Fragment)] out float4 outColor;

#if defined(SHADER_FRAGMENT)
uniform float MyUnusedStagePushConstant;
#endif

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
    gl_Position = mul(mul(named.viewproj, named.model), float4(in_POSITION + offset_x.xyz, 1.0f));
    vs_COLOR = in_COLOR;
    vs_TEXCOORD0 = in_TEXCOORD0;
    vs_TEXCOORD0.x += gl_VertexIndex;
}

void MainFs()
{
    float3 albedo = texture(sampler2D(_AlbedoTex, smp), vs_TEXCOORD0).xyz;
    float3 texcolor = texture(tex1, vs_TEXCOORD0).xyz + _ReadVariable[0].xyz;
    outColor = float4(albedo * texcolor * vs_COLOR * _Color.rgb, 1.0);
    imageStore(pk_DebugImage, int2(outColor.xy * 1024), float4(outColor.x, 0, 0, 0));
    _WriteBuffer[uint(outColor.x * 1024)] = outColor;
}

void ColorFs() { outColor = float4(vs_COLOR, 1.0f); }

