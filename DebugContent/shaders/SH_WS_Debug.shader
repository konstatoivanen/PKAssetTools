#pragma pk_log_verbose

#pragma pk_cull Back
#pragma pk_ztest LessEqual
#pragma pk_zwrite True

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
uniform ConstantBufferRef<UniformBufferObject> unused_ubo;
uniform ConstantBufferRef<UniformBufferObject> named;

uniform RWTexture2DRef<float4> pk_DebugImage;
uniform RWStructuredBufferRef<float4> _WriteBuffer;
uniform StructuredBufferRef<float4> _ReadVariable;
uniform Texture2DRef<float4> tex1;
uniform SamplerRef smp;

#if defined(SHADER_FRAGMENT)
uniform float MyUnusedStagePushConstant;
#endif

struct VsIn
{
    float3 in_POSITION : in_POSITION;
    float2 in_TEXCOORD0 : in_TEXCOORD0;
    float3 in_COLOR : in_COLOR;
};

struct VsOut
{
    float4 sv_Position : SV_Position;
    float3 vs_COLOR : COLOR0;
    float2 vs_TEXCOORD0 : TEXCOORD0;
    PK_INSTANCING_VARYING_ID
};

struct TestStruct
{
    float3 testfield;
};

void UnusedTestFunc(inout VsOut o, float3 color)
{
    o.vs_COLOR = color;
}

VsOut MainVs(VsIn vin)
{
    VsOut vout;
    PK_INSTANCING_ASSIGN_VARYING_ID(vout, gl_InstanceIndex);
    vout.sv_Position = mul(mul(named.viewproj, named.model), float4(vin.in_POSITION + offset_x.xyz, 1.0f));
    vout.vs_COLOR = vin.in_COLOR;
    vout.vs_TEXCOORD0 = vin.in_TEXCOORD0;
    vout.vs_TEXCOORD0.x += gl_VertexIndex;
    return vout;
}

[earlydepthstencil]
float4 MainFs(VsOut vout) : SV_Target0
{
    PK_INSTANCING_ASSIGN_FRAGMENT_LOCALS(vout);
    float3 albedo = _AlbedoTex.Sample(smp, vout.vs_TEXCOORD0).xyz;
    float3 texcolor = tex1.Sample(smp, vout.vs_TEXCOORD0).xyz + _ReadVariable[0].xyz;
    float4 outColor = float4(albedo * texcolor * vout.vs_COLOR * _Color.rgb, 1.0f);
    pk_DebugImage[int2(outColor.xy * 1024.0f)] = float4(outColor.x, 0.0f, 0.0f, 0.0f);
    _WriteBuffer[uint(outColor.x * 1024.0f)] = outColor;
    return outColor;
}

[earlydepthstencil]
float4 ColorFs(VsOut vout) : SV_Target0
{
    return float4(vout.vs_COLOR, 1.0f);
}