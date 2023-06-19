#include "PKShaderUtilities.h"
#include "PKStringUtilities.h"

namespace PK::Assets::Shader
{
    PKElementType GetElementType(SpvReflectFormat format)
    {
        switch (format)
        {
        case SPV_REFLECT_FORMAT_R32_SFLOAT: return PKElementType::Float;
        case SPV_REFLECT_FORMAT_R32G32_SFLOAT: return PKElementType::Float2;
        case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT: return PKElementType::Float3;
        case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT: return PKElementType::Float4;
        case SPV_REFLECT_FORMAT_R64_SFLOAT: return PKElementType::Double;
        case SPV_REFLECT_FORMAT_R64G64_SFLOAT: return PKElementType::Double2;
        case SPV_REFLECT_FORMAT_R64G64B64_SFLOAT: return PKElementType::Double3;
        case SPV_REFLECT_FORMAT_R64G64B64A64_SFLOAT: return PKElementType::Double4;
        case SPV_REFLECT_FORMAT_R32_SINT: return PKElementType::Int;
        case SPV_REFLECT_FORMAT_R32G32_SINT: return PKElementType::Int2;
        case SPV_REFLECT_FORMAT_R32G32B32_SINT: return PKElementType::Int3;
        case SPV_REFLECT_FORMAT_R32G32B32A32_SINT: return PKElementType::Int4;
        case SPV_REFLECT_FORMAT_R32_UINT: return PKElementType::Uint;
        case SPV_REFLECT_FORMAT_R32G32_UINT: return PKElementType::Uint2;
        case SPV_REFLECT_FORMAT_R32G32B32_UINT: return PKElementType::Uint3;
        case SPV_REFLECT_FORMAT_R32G32B32A32_UINT: return PKElementType::Uint4;
        case SPV_REFLECT_FORMAT_R64_SINT: return PKElementType::Long;
        case SPV_REFLECT_FORMAT_R64G64_SINT: return PKElementType::Long2;
        case SPV_REFLECT_FORMAT_R64G64B64_SINT: return PKElementType::Long3;
        case SPV_REFLECT_FORMAT_R64G64B64A64_SINT: return PKElementType::Long4;
        case SPV_REFLECT_FORMAT_R64_UINT: return PKElementType::Ulong;
        case SPV_REFLECT_FORMAT_R64G64_UINT: return PKElementType::Ulong2;
        case SPV_REFLECT_FORMAT_R64G64B64_UINT: return PKElementType::Ulong3;
        case SPV_REFLECT_FORMAT_R64G64B64A64_UINT: return PKElementType::Ulong4;
        }

        return PKElementType::Invalid;
    }

    std::string GetGLSLType(PKElementType type)
    {
        switch (type)
        {
        case PKElementType::Float: return "float";
        case PKElementType::Float2: return "vec2";
        case PKElementType::Float3: return "vec3";
        case PKElementType::Float4: return "vec4";
        case PKElementType::Double: return "float64_t";
        case PKElementType::Double2: return "f64vec2";
        case PKElementType::Double3: return "f64vec3";
        case PKElementType::Double4: return "f64vec4";
        case PKElementType::Half: return "float16_t";
        case PKElementType::Half2: return "f16vec2";
        case PKElementType::Half3: return "f16vec3";
        case PKElementType::Half4: return "f16vec4";
        case PKElementType::Int: return "int";
        case PKElementType::Int2: return "ivec2";
        case PKElementType::Int3: return "ivec3";
        case PKElementType::Int4: return "ivec4";
        case PKElementType::Uint: return "uint";
        case PKElementType::Uint2: return "uvec2";
        case PKElementType::Uint3: return "uvec3";
        case PKElementType::Uint4: return "uvec4";
        case PKElementType::Short: return "int16_t";
        case PKElementType::Short2: return "i16vec2";
        case PKElementType::Short3: return "i16vec3";
        case PKElementType::Short4: return "i16vec4";
        case PKElementType::Ushort: return "u16int";
        case PKElementType::Ushort2: return "u16vec2";
        case PKElementType::Ushort3: return "u16vec3";
        case PKElementType::Ushort4: return "u16vec4";
        case PKElementType::Long: return "int64_t";
        case PKElementType::Long2: return "i64vec2";
        case PKElementType::Long3: return "i64vec3";
        case PKElementType::Long4: return "i64vec4";
        case PKElementType::Ulong: return "uint64_t";
        case PKElementType::Ulong2: return "u64vec2";
        case PKElementType::Ulong3: return "u64vec3";
        case PKElementType::Ulong4: return "u64vec4";
        case PKElementType::Float2x2: return "mat2";
        case PKElementType::Float3x3: return "mat3";
        case PKElementType::Float4x4: return "mat4";
        case PKElementType::Double2x2: return "f64mat2";
        case PKElementType::Double3x3: return "f64mat3";
        case PKElementType::Double4x4: return "f64mat4";
        case PKElementType::Half2x2: return "f16mat2";
        case PKElementType::Half3x3: return "f16mat3";
        case PKElementType::Half4x4: return "f16mat4";
        case PKElementType::Texture2DHandle: return "uint";
        case PKElementType::Texture3DHandle: return "uint";
        case PKElementType::TextureCubeHandle: return "uint";
        }

        return "INVALID";
    }

    PKComparison GetZTestFromString(const std::string& ztest)
    {
        if (ztest == "Off")
        {
            return PKComparison::Off;
        }
        else if (ztest == "Always")
        {
            return PKComparison::Always;
        }
        else if (ztest == "Never")
        {
            return PKComparison::Never;
        }
        else if (ztest == "Less")
        {
            return PKComparison::Less;
        }
        else if (ztest == "LEqual")
        {
            return PKComparison::LessEqual;
        }
        else if (ztest == "Greater")
        {
            return PKComparison::Greater;
        }
        else if (ztest == "NotEqual")
        {
            return PKComparison::NotEqual;
        }
        else if (ztest == "GEqual")
        {
            return PKComparison::GreaterEqual;
        }
        else if (ztest == "Equal")
        {
            return PKComparison::Equal;
        }

        return PKComparison::Off;
    }

    PKBlendFactor GetBlendFactorFromString(const std::string& blendMode)
    {
        if (blendMode == "Zero")
        {
            return PKBlendFactor::Zero;
        }
        else if (blendMode == "One")
        {
            return PKBlendFactor::One;
        }
        else if (blendMode == "SrcColor")
        {
            return PKBlendFactor::SrcColor;
        }
        else if (blendMode == "OneMinusSrcColor")
        {
            return PKBlendFactor::OneMinusSrcColor;
        }
        else if (blendMode == "DstColor")
        {
            return PKBlendFactor::DstColor;
        }
        else if (blendMode == "OneMinusDstColor")
        {
            return PKBlendFactor::OneMinusDstColor;
        }
        else if (blendMode == "SrcAlpha")
        {
            return PKBlendFactor::SrcAlpha;
        }
        else if (blendMode == "OneMinusSrcAlpha")
        {
            return PKBlendFactor::OneMinusSrcAlpha;
        }
        else if (blendMode == "DstAlpha")
        {
            return PKBlendFactor::DstAlpha;
        }
        else if (blendMode == "OneMinusDstAlpha")
        {
            return PKBlendFactor::OneMinusDstAlpha;
        }
        else if (blendMode == "ConstColor")
        {
            return PKBlendFactor::ConstColor;
        }
        else if (blendMode == "OneMinusConstColor")
        {
            return PKBlendFactor::OneMinusConstColor;
        }
        else if (blendMode == "ConstAlpha")
        {
            return PKBlendFactor::ConstAlpha;
        }
        else if (blendMode == "OneMinusConstAlpha")
        {
            return PKBlendFactor::OneMinusConstAlpha;
        }

        return PKBlendFactor::None;
    }

    PKBlendOp GetBlendOpFromString(const std::string& blendOp)
    {
        if (blendOp == "Add")
        {
            return PKBlendOp::Add;
        }
        else if (blendOp == "Subtract")
        {
            return PKBlendOp::Subtract;
        }
        else if (blendOp == "ReverseSubtract")
        {
            return PKBlendOp::ReverseSubtract;
        }
        else if (blendOp == "Min")
        {
            return PKBlendOp::Min;
        }
        else if (blendOp == "Max")
        {
            return PKBlendOp::Max;
        }

        return PKBlendOp::None;
    }

    uint8_t GetColorMaskFromString(const std::string& colorMask)
    {
        if (colorMask.empty())
        {
            return 255;
        }

        uint8_t mask = 0;

        if (colorMask.find('R') != std::string::npos)
        {
            mask |= 1 << 0;
        }

        if (colorMask.find('G') != std::string::npos)
        {
            mask |= 1 << 1;
        }

        if (colorMask.find('B') != std::string::npos)
        {
            mask |= 1 << 2;
        }

        if (colorMask.find('A') != std::string::npos)
        {
            mask |= 1 << 3;
        }

        return mask;
    }

    PKCullMode GetCullModeFromString(const std::string& cull)
    {
        if (cull.empty() || cull == "Off")
        {
            return PKCullMode::Off;
        }
        else if (cull == "Front")
        {
            return PKCullMode::Front;
        }
        else if (cull == "Back")
        {
            return PKCullMode::Back;
        }

        return PKCullMode::Off;
    }

    PKRasterMode GetRasterModeFromString(const std::string& rasterMode)
    {
        if (rasterMode.empty() || rasterMode == "Default")
        {
            return PKRasterMode::Default;
        }
        else if (rasterMode == "OverEstimate")
        {
            return PKRasterMode::OverEstimate;
        }
        else if (rasterMode == "UnderEstimate")
        {
            return PKRasterMode::UnderEstimate;
        }

        return PKRasterMode::Default;
    }

    PKShaderStage GetShaderStageFromString(const std::string& type)
    {
        if (type == "VERTEX")
        {
            return PKShaderStage::Vertex;
        }

        if (type == "FRAGMENT")
        {
            return PKShaderStage::Fragment;
        }

        if (type == "GEOMETRY")
        {
            return PKShaderStage::Geometry;
        }

        if (type == "TESSELATION_CONTROL")
        {
            return PKShaderStage::TesselationControl;
        }

        if (type == "TESSELATION_EVALUATE")
        {
            return PKShaderStage::TesselationEvaluation;
        }

        if (type == "COMPUTE")
        {
            return PKShaderStage::Compute;
        }

        if (type == "RAY_GENERATION")
        {
            return PKShaderStage::RayGeneration;
        }

        if (type == "RAY_MISS")
        {
            return PKShaderStage::RayMiss;
        }

        if (type == "RAY_CLOSEST_HIT")
        {
            return PKShaderStage::RayClosestHit;
        }

        if (type == "RAY_ANY_HIT")
        {
            return PKShaderStage::RayAnyHit;
        }

        if (type == "RAY_INTERSECTION")
        {
            return PKShaderStage::RayIntersection;
        }

        return PKShaderStage::MaxCount;
    }

    PKDescriptorType GetResourceType(SpvReflectDescriptorType type)
    {
        switch (type)
        {
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER: return PKDescriptorType::Sampler;
        case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return PKDescriptorType::SamplerTexture;
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE: return PKDescriptorType::Texture;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE: return PKDescriptorType::Image;
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER: return PKDescriptorType::ConstantBuffer;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER: return PKDescriptorType::StorageBuffer;
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: return PKDescriptorType::DynamicConstantBuffer;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: return PKDescriptorType::DynamicStorageBuffer;
        case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: return PKDescriptorType::InputAttachment;
        case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR: return PKDescriptorType::AccelerationStructure;
        }

        return PKDescriptorType::Invalid;
    }

    shaderc_shader_kind ConvertToShadercKind(PKShaderStage stage)
    {
        switch (stage)
        {
        case PKShaderStage::Vertex: return shaderc_vertex_shader;
        case PKShaderStage::TesselationControl: return shaderc_tess_control_shader;
        case PKShaderStage::TesselationEvaluation: return shaderc_tess_evaluation_shader;
        case PKShaderStage::Geometry: return shaderc_geometry_shader;
        case PKShaderStage::Fragment: return shaderc_fragment_shader;
        case PKShaderStage::Compute: return shaderc_compute_shader;

        case PKShaderStage::RayGeneration: return shaderc_raygen_shader;
        case PKShaderStage::RayMiss: return shaderc_miss_shader;
        case PKShaderStage::RayClosestHit: return shaderc_closesthit_shader;
        case PKShaderStage::RayAnyHit: return shaderc_anyhit_shader;
        case PKShaderStage::RayIntersection: return shaderc_intersection_shader;
        }

        return shaderc_shader_kind::shaderc_glsl_infer_from_source;
    }

    void ConvertHLSLTypesToGLSL(std::string& source)
    {
        const std::string surroundMask = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.";
        StringUtilities::ReplaceAll(source, surroundMask, "lerp", "mix");
        StringUtilities::ReplaceAll(source, surroundMask, "tex2D", "texture");
        StringUtilities::ReplaceAll(source, surroundMask, "tex2DLod", "textureLod");

        StringUtilities::ReplaceAll(source, surroundMask, "bool2", "bvec2");
        StringUtilities::ReplaceAll(source, surroundMask, "bool3", "bvec3");
        StringUtilities::ReplaceAll(source, surroundMask, "bool4", "bvec4");

        StringUtilities::ReplaceAll(source, surroundMask, "half", "float16_t");
        StringUtilities::ReplaceAll(source, surroundMask, "half2", "f16vec2");
        StringUtilities::ReplaceAll(source, surroundMask, "half3", "f16vec3");
        StringUtilities::ReplaceAll(source, surroundMask, "half4", "f16vec4");

        StringUtilities::ReplaceAll(source, surroundMask, "float2", "vec2");
        StringUtilities::ReplaceAll(source, surroundMask, "float3", "vec3");
        StringUtilities::ReplaceAll(source, surroundMask, "float4", "vec4");

        StringUtilities::ReplaceAll(source, surroundMask, "double", "float64_t");
        StringUtilities::ReplaceAll(source, surroundMask, "double2", "f64vec2");
        StringUtilities::ReplaceAll(source, surroundMask, "double3", "f64vec3");
        StringUtilities::ReplaceAll(source, surroundMask, "double4", "f64vec4");

        StringUtilities::ReplaceAll(source, surroundMask, "short", "int16_t");
        StringUtilities::ReplaceAll(source, surroundMask, "short2", "i16vec2");
        StringUtilities::ReplaceAll(source, surroundMask, "short3", "i16vec3");
        StringUtilities::ReplaceAll(source, surroundMask, "short4", "i16vec4");

        StringUtilities::ReplaceAll(source, surroundMask, "ushort", "uint16_t");
        StringUtilities::ReplaceAll(source, surroundMask, "ushort2", "u16vec2");
        StringUtilities::ReplaceAll(source, surroundMask, "ushort3", "u16vec3");
        StringUtilities::ReplaceAll(source, surroundMask, "ushort4", "u16vec4");

        StringUtilities::ReplaceAll(source, surroundMask, "byte", "uint8_t");
        StringUtilities::ReplaceAll(source, surroundMask, "byte2", "u8vec2");
        StringUtilities::ReplaceAll(source, surroundMask, "byte3", "u8vec3");
        StringUtilities::ReplaceAll(source, surroundMask, "byte4", "u8vec4");

        StringUtilities::ReplaceAll(source, surroundMask, "sbyte", "int8_t");
        StringUtilities::ReplaceAll(source, surroundMask, "sbyte2", "i8vec2");
        StringUtilities::ReplaceAll(source, surroundMask, "sbyte3", "i8vec3");
        StringUtilities::ReplaceAll(source, surroundMask, "sbyte4", "i8vec4");

        StringUtilities::ReplaceAll(source, surroundMask, "int2", "ivec2");
        StringUtilities::ReplaceAll(source, surroundMask, "int3", "ivec3");
        StringUtilities::ReplaceAll(source, surroundMask, "int4", "ivec4");

        StringUtilities::ReplaceAll(source, surroundMask, "uint2", "uvec2");
        StringUtilities::ReplaceAll(source, surroundMask, "uint3", "uvec3");
        StringUtilities::ReplaceAll(source, surroundMask, "uint4", "uvec4");

        StringUtilities::ReplaceAll(source, surroundMask, "long", "int64_t");
        StringUtilities::ReplaceAll(source, surroundMask, "long2", "i64vec2");
        StringUtilities::ReplaceAll(source, surroundMask, "long3", "i64vec3");
        StringUtilities::ReplaceAll(source, surroundMask, "long4", "i64vec4");

        StringUtilities::ReplaceAll(source, surroundMask, "ulong", "uint64_t");
        StringUtilities::ReplaceAll(source, surroundMask, "ulong2", "u64vec2");
        StringUtilities::ReplaceAll(source, surroundMask, "ulong3", "u64vec3");
        StringUtilities::ReplaceAll(source, surroundMask, "ulong4", "u64vec4");

        StringUtilities::ReplaceAll(source, surroundMask, "half2x2", "f16mat2");
        StringUtilities::ReplaceAll(source, surroundMask, "half3x3", "f16mat3");
        StringUtilities::ReplaceAll(source, surroundMask, "half4x4", "f16mat4");

        StringUtilities::ReplaceAll(source, surroundMask, "float2x2", "mat2");
        StringUtilities::ReplaceAll(source, surroundMask, "float3x3", "mat3");
        StringUtilities::ReplaceAll(source, surroundMask, "float4x4", "mat4");

        StringUtilities::ReplaceAll(source, surroundMask, "double2x2", "f64mat2");
        StringUtilities::ReplaceAll(source, surroundMask, "double3x3", "f64mat3");
        StringUtilities::ReplaceAll(source, surroundMask, "double4x4", "f64mat4");
    }
}