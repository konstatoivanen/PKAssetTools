#pragma pk_cull Back
#pragma pk_ztest LessEqual
#pragma pk_zwrite True
#pragma pk_log_verbose
#pragma pk_generate_debug_info

#pragma pk_material_property float4 _Color
#pragma pk_material_property float _Roughness
#pragma pk_material_property float _Metallic
#pragma pk_material_property Texture2DRef _AlbedoTex
#pragma pk_material_property Texture3DRef _LutTex

#pragma pk_multi_compile _ PASS_COLOR
#pragma pk_program Vertex MainVs
#pragma pk_program Fragment MainFs
#pragma pk_program Fragment ColorFs PASS_COLOR

#include "includes/DebugUtilities.glsl"

struct UniformBufferObject
{
    float4x4 model;
    float4x4 viewproj;
};

uniform float4 offset_x;
uniform float4 unused_x;
uniform float MyUnusedStagePushConstant;

uniform CBufferRef<UniformBufferObject> ubo;
uniform CBufferRef<UniformBufferObject> named;
uniform RWTexture2DRef<float4> pk_DebugImage;
uniform RWStructuredBufferRef<float4> _WriteBuffer;
uniform StructuredBufferRef<float4> _ReadVariable;
uniform Texture2DRef tex1;
uniform SamplerRef smp;

struct VsInput
{
    float3 in_POSITION : POSITION;
    float2 in_TEXCOORD0 : TEXCOORD0;
    float3 in_COLOR : COLOR;
};

struct Varyings
{
    float4 sv_Position : SV_Position;
    float3 vs_COLOR : COLOR0;
    float2 vs_TEXCOORD0 : TEXCOORD0;
};

struct TestStruct
{
    float3 testfield;
};

float3 TestFunc(float3 color)
{
    return color;
}

Varyings MainVs(VsInput input, uint vertexId : SV_VertexID)
{
    Varyings output;
    output.sv_Position = mul(mul(ubo.viewproj, ubo.model), float4(input.in_POSITION + offset_x.xyz, 1.0f));
    output.vs_COLOR = TestFunc(input.in_COLOR);
    output.vs_TEXCOORD0 = input.in_TEXCOORD0;
    output.vs_TEXCOORD0.x += float(vertexId);
    return output;
}

float4 MainFs(Varyings input) : SV_Target0
{
    float3 albedo = _AlbedoTex.Sample(smp, input.vs_TEXCOORD0).xyz;
    float3 texcolor = tex1.Sample(smp, input.vs_TEXCOORD0).xyz + _ReadVariable[0].xyz;
    float4 outColor = float4(albedo * texcolor * input.vs_COLOR * _Color.rgb, 1.0f);
    pk_DebugImage[int2(outColor.xy * 1024.0f)] = float4(outColor.x, 0.0f, 0.0f, 0.0f);
    _WriteBuffer[uint(outColor.x * 1024.0f)] = outColor;
    return outColor;
}

float4 ColorFs(Varyings input) : SV_Target0
{
    return float4(input.vs_COLOR, 1.0f);
}