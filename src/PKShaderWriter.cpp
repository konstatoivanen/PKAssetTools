#pragma once
#include "PKShaderWriter.h"
#include "PKAssetWriter.h"
#include "PKStringUtilities.h"
#include <shaderc/shaderc.hpp>
#include <SPIRV-Reflect/spirv_reflect.h>
#include <unordered_map>
#include <map>
#include <stdexcept>
#include <sstream>

namespace PK::Assets::Shader
{
    using PK::Assets::GetElementType;

    typedef shaderc::Compiler ShaderCompiler;

    struct ReflectBinding
    {
        uint_t firstStage = (int)PKShaderStage::MaxCount;
        const SpvReflectDescriptorBinding* bindings[(int)PKShaderStage::MaxCount]{};
        const SpvReflectDescriptorBinding* get() { return bindings[firstStage]; }
    };

    typedef std::vector<uint32_t> ShaderSpriV;

    struct ReflectionData
    {
        SpvReflectShaderModule modules[(int)PKShaderStage::MaxCount]{};
        std::map<std::string, ReflectBinding> uniqueBindings;
        std::map<std::string, const SpvReflectBlockVariable*> uniqueVariables;
        std::map<std::string, uint32_t> variableStageFlags;
        std::map<uint32_t, uint32_t> setStageFlags;
        uint32_t setCount = 0u;
    };

    constexpr const static char* Instancing_Base_GLSL =
        "#define PK_INSTANCING_ENABLED                                                                                                                  \n"
        "struct PK_Transform { mat4 localToWorld; mat4 worldToLocal; };                                                                                 \n"
        "struct PK_Draw { uint material; uint transform; };                                                                                             \n"
        "layout(std430, set = 0, binding = 100) readonly buffer pk_Instancing_Transforms { PK_Transform pk_Instancing_Transforms_Data[]; };             \n"
        "layout(std430, set = 3, binding = 101) readonly buffer pk_Instancing_Indices { PK_Draw pk_Instancing_Indices_Data[]; };                        \n"
        "layout(std430, set = 3, binding = 102) readonly buffer pk_Instancing_Properties { PK_MaterialPropertyBlock pk_Instancing_Properties_Data[]; }; \n"
        "layout(set = 3, binding = 103) uniform sampler2D pk_Instancing_Textures2D[];                                                                   \n"
        "layout(set = 3, binding = 104) uniform sampler3D pk_Instancing_Textures3D[];                                                                   \n"
        "layout(set = 3, binding = 105) uniform samplerCube pk_Instancing_TexturesCube[];                                                               \n"
        "mat4 pk_MATRIX_M;                                                                                                                              \n"
        "mat4 pk_MATRIX_I_M;                                                                                                                            \n"
        "void PK_INSTANCING_ASSIGN_LOCALS(uint index)                                                                                                   \n"
        "{                                                                                                                                              \n"
        "    PK_Draw draw = pk_Instancing_Indices_Data[index];                                                                                          \n"
        "    PK_Transform transform = pk_Instancing_Transforms_Data[draw.transform];                                                                    \n"
        "    pk_MATRIX_M = transform.localToWorld;                                                                                                      \n"
        "    pk_MATRIX_I_M = transform.worldToLocal;                                                                                                    \n"
        "    PK_MaterialPropertyBlock prop = pk_Instancing_Properties_Data[draw.material];                                                              \n";

    constexpr const static char* Instancing_Stage_GLSL = "PK_INSTANCING_ASSIGN_STAGE_LOCALS \n";

    constexpr const static char* Instancing_Vertex_GLSL =
        "out flat uint vs_INSTANCE_ID;                                                                                                          \n"
        "#define PK_INSTANCING_ASSIGN_STAGE_LOCALS PK_INSTANCING_ASSIGN_LOCALS(uint(gl_InstanceIndex)); vs_INSTANCE_ID = uint(gl_InstanceIndex);\n";

    constexpr const static char* Instancing_Fragment_GLSL =
        "in flat uint vs_INSTANCE_ID;                                                           \n"
        "#define PK_INSTANCING_ASSIGN_STAGE_LOCALS PK_INSTANCING_ASSIGN_LOCALS(vs_INSTANCE_ID); \n";

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

    static PKComparison GetZTestFromString(const std::string& ztest)
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

    static PKBlendFactor GetBlendFactorFromString(const std::string& blendMode)
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

    static PKBlendOp GetBlendOpFromString(const std::string& blendOp)
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

    static unsigned short GetColorMaskFromString(const std::string& colorMask)
    {
        if (colorMask.empty())
        {
            return 255;
        }

        unsigned short mask = 0;

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

    static PKCullMode GetCullModeFromString(const std::string& cull)
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

    static PKShaderStage GetShaderStageFromString(const std::string& type)
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

        return PKShaderStage::MaxCount;
    }

    static PKDescriptorType GetResourceType(SpvReflectDescriptorType type)
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
        }

        return PKDescriptorType::Invalid;
    }


    static void ReadFile(const std::string& filepath, std::string& ouput)
    {
        ouput = StringUtilities::ReadFileRecursiveInclude(filepath);
    }

    static void ConvertHLSLTypesToGLSL(std::string& source)
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

    static void ExtractMulticompiles(std::string& source, 
                                     std::vector<std::vector<std::string>>& keywords, 
                                     std::vector<PKShaderKeyword>& outKeywords, 
                                     uint_t* outVariantCount, 
                                     uint_t* outDirectiveCount)
    {
        std::string output;
        size_t pos = 0;
        size_t dcount = 0;
        size_t vcount = 1;

        while (true)
        {
            pos = StringUtilities::ExtractToken(pos, PK_SHADER_ATTRIB_MULTI_COMPILE, source, output, false);

            if (pos == std::string::npos)
            {
                break;
            }

            auto directive = StringUtilities::Split(output, " ");

            for (auto i = 0; i < directive.size(); ++i)
            {
                auto& keyword = directive.at(i);

                if (keyword != "_")
                {
                    PKShaderKeyword pkkw{};
                    pkkw.offsets = (uint_t)((((dcount << 4) | (i & 0xF)) << 24) | (vcount & 0xFFFFFF));
                    WriteName(pkkw.name, keyword.c_str());
                    outKeywords.push_back(pkkw);
                }
            }

            keywords.push_back(directive);
            dcount++;
            vcount *= directive.size();
        }

        *outDirectiveCount = (uint_t)dcount;
        *outVariantCount = (uint_t)vcount;
    }

    static void ExtractStateAttributes(std::string& source, PKShaderFixedStateAttributes* attributes)
    {
        auto valueZWrite = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_ZWRITE, source, false);
        auto valueZTest = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_ZTEST, source, false);
        auto valueBlendColor = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_BLENDCOLOR, source, false);
        auto valueBlendAlpha = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_BLENDALPHA, source, false);
        auto valueColorMask = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_COLORMASK, source, false);
        auto valueCull = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_CULL, source, false);
        auto valueOffset = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_OFFSET, source, false);

        if (!valueZWrite.empty())
        {
            valueZWrite = StringUtilities::Trim(valueZWrite);
            attributes->zwrite = valueZWrite == "True" ? 1 : 0;
        }

        if (!valueZTest.empty())
        {
            attributes->ztest = GetZTestFromString(StringUtilities::Trim(valueZTest));
        }

        if (!valueBlendColor.empty())
        {
            auto keywords = StringUtilities::Split(valueBlendColor, " \n\r");

            attributes->blendSrcFactorColor = PKBlendFactor::None;
            attributes->blendDstFactorColor = PKBlendFactor::None;
            attributes->blendOpColor = PKBlendOp::None;

            if (keywords.size() == 3)
            {
                attributes->blendOpColor = GetBlendOpFromString(keywords.at(0));
                attributes->blendSrcFactorColor = GetBlendFactorFromString(keywords.at(1));
                attributes->blendDstFactorColor = GetBlendFactorFromString(keywords.at(2));
            }
        }

        if (!valueBlendAlpha.empty())
        {
            auto keywords = StringUtilities::Split(valueBlendAlpha, " \n\r");

            attributes->blendSrcFactorAlpha = PKBlendFactor::None;
            attributes->blendDstFactorAlpha = PKBlendFactor::None;
            attributes->blendOpAlpha = PKBlendOp::None;

            if (keywords.size() == 3)
            {
                attributes->blendOpAlpha = GetBlendOpFromString(keywords.at(0));
                attributes->blendSrcFactorAlpha = GetBlendFactorFromString(keywords.at(0));
                attributes->blendDstFactorAlpha = GetBlendFactorFromString(keywords.at(2));
            }
        }

        if (!valueOffset.empty())
        {
            auto keywords = StringUtilities::Split(valueOffset, " \n\r");

            attributes->zoffsets[0] = 0.0f;
            attributes->zoffsets[1] = 0.0f;
            attributes->zoffsets[2] = 0.0f;

            if (keywords.size() == 3)
            {
                attributes->zoffsets[0] = std::stof(keywords.at(0));
                attributes->zoffsets[1] = std::stof(keywords.at(1));
                attributes->zoffsets[2] = std::stof(keywords.at(2));
            }
        }

        attributes->colorMask = GetColorMaskFromString(valueColorMask);
        attributes->cull = GetCullModeFromString(StringUtilities::Trim(valueCull));
    }

    static void ProcessInstancingProperties(std::string& source, std::vector<PKMaterialProperty>& materialProperties)
    {
        std::string output;
        size_t pos = 0;

        materialProperties.clear();

        while (true)
        {
            pos = StringUtilities::ExtractToken(pos, PK_SHADER_ATTRIB_MATERIAL_PROP, source, output, false);

            if (pos == std::string::npos)
            {
                break;
            }

            auto parts = StringUtilities::Split(output, " ");

            if (parts.size() != 2)
            {
                continue;
            }

            auto type = PK::Assets::GetElementType(parts.at(0).c_str());

            if (type == PKElementType::Invalid)
            {
                continue;
            }

            PKMaterialProperty prop;
            prop.type = type;
            WriteName(prop.name, parts.at(1).c_str());
            materialProperties.push_back(prop);
        }

        if (materialProperties.size() == 0)
        {
            return;
        }

        std::string block = "struct PK_MaterialPropertyBlock\n{\n";

        for (auto& prop : materialProperties)
        {
            block += "    " + GetGLSLType(prop.type) + " " + std::string(prop.name) + ";\n";
        }

        block += "};\n";

        for (auto& prop : materialProperties)
        {
            auto name = std::string(prop.name);

            switch (prop.type)
            {
                case PKElementType::Texture2DHandle:
                    block += "uint " + name + "_Handle;\n";
                    break;
                case PKElementType::Texture3DHandle:
                    block += "uint " + name + "_Handle;\n";
                    break;
                case PKElementType::TextureCubeHandle:
                    block += "uint " + name + "_Handle;\n";
                    break;
                default:
                    block += GetGLSLType(prop.type) + " " + name + ";\n";
                    break;
            }
        }

        block += Instancing_Base_GLSL;

        for (auto& prop : materialProperties)
        {
            auto name = std::string(prop.name);

            switch (prop.type)
            {
                case PKElementType::Texture2DHandle:
                case PKElementType::Texture3DHandle:
                case PKElementType::TextureCubeHandle:
                    block += "    " + name + "_Handle = prop." + name + ";\n";
                    break;

                default:
                    block += "    " + name + " = prop." + name + ";\n";
                    break;
            }
        }
        
        block += "}\n";

        for (auto& prop : materialProperties)
        {
            auto name = std::string(prop.name);

            switch (prop.type)
            {
                case PKElementType::Texture2DHandle:
                    block += "#define " + name + " pk_Instancing_Textures2D[" + name + "_Handle]\n";
                    break;
                case PKElementType::Texture3DHandle:
                    block += "#define " + name + " pk_Instancing_Textures3D[" + name + "_Handle]\n";
                    break;
                case PKElementType::TextureCubeHandle:
                    block += "#define " + name + " pk_Instancing_TexturesCube[" + name + "_Handle]\n";
                    break;
                default:
                    break;
            }
        }

        source.insert(0, block);
    }

    static void InsertRequiredExtensions(std::string& source)
    {
        source.insert(0, "#extension GL_EXT_shader_explicit_arithmetic_types : require \n");
        source.insert(0, "#extension GL_EXT_nonuniform_qualifier : require \n");
    }

    static void InsertInstancingDefine(std::string& source, PKShaderStage stage, bool enableInstancing)
    {
        if (!enableInstancing)
        {
            return;
        }

        switch (stage)
        {
            case PKShaderStage::Vertex: source.insert(0, Instancing_Vertex_GLSL); break;
            case PKShaderStage::Fragment: source.insert(0, Instancing_Fragment_GLSL); break;
            default: return;
        }

        auto pos = source.find("main()");
        pos = source.find('{', pos);
        auto eol = source.find_first_of("\r\n", pos);
        auto nextLinePos = source.find_first_not_of("\r\n", eol);
        source.insert(nextLinePos, Instancing_Stage_GLSL);
    }

    static void GetSharedInclude(const std::string& source, std::string& sharedInclude)
    {
        auto typeToken = "#pragma PROGRAM_";
        auto typeTokenLength = strlen(typeToken);
        auto pos = source.find(typeToken, 0); //Start of shader type declaration line

        // Treat code in the beginning of the source as shared include
        if (pos != std::string::npos && pos != 0)
        {
            sharedInclude = source.substr(0, pos);
        }
    }

    static void GetVariantDefines(const std::vector<std::vector<std::string>>& keywords, uint32_t index, std::string& defines)
    {
        defines.clear();

        for (auto i = 0; i < keywords.size(); ++i)
        {
            auto& declares = keywords.at(i);
            auto& keyword = declares.at(index % declares.size());

            if (keyword != "_")
            {
                defines.append("#define ");
                defines.append(keyword);
                defines.append("\n");
            }

            index /= (uint32_t)declares.size();
        }
    }

    static void ProcessShaderVersion(std::string& source)
    {
        auto versionToken = StringUtilities::ExtractToken("#version ", source, true);

        if (versionToken.empty())
        {
            source.insert(0, "#version 460\n");
        }
        else
        {
            source.insert(0, versionToken);
        }
    }

    static void ProcessShaderStageDefine(PKShaderStage stage, std::string& source)
    {
        switch (stage)
        {
            case PKShaderStage::Vertex: source.insert(0, "#define SHADER_STAGE_VERTEX\n"); return;
            case PKShaderStage::Fragment: source.insert(0, "#define SHADER_STAGE_FRAGMENT\n"); return;
            case PKShaderStage::Geometry: source.insert(0, "#define SHADER_STAGE_GEOMETRY\n"); return;
            case PKShaderStage::TesselationControl: source.insert(0, "#define SHADER_STAGE_TESSELATION_CONTROL\n"); return;
            case PKShaderStage::TesselationEvaluation: source.insert(0, "#define SHADER_STAGE_TESSELATION_EVALUATE\n"); return;
            case PKShaderStage::Compute: source.insert(0, "#define SHADER_STAGE_COMPUTE\n"); return;
        }
    }

    static int ProcessStageSources(const std::string& source, 
                                   const std::string& sharedInclude, 
                                   const std::string& variantDefines, 
                                   std::unordered_map<PKShaderStage, 
                                   std::string>& shaderSources,
                                   bool enableInstancing)
    {
        shaderSources.clear();

        auto typeToken = "#pragma PROGRAM_";
        auto typeTokenLength = strlen(typeToken);
        auto pos = source.find(typeToken, 0); //Start of shader type declaration line

        while (pos != std::string::npos)
        {
            auto eol = source.find_first_of("\r\n", pos); //End of shader type declaration line
            auto nextLinePos = source.find_first_not_of("\r\n", eol); //Start of shader code after shader type declaration line

            if (eol == std::string::npos ||
                nextLinePos == std::string::npos)
            {
                printf("Syntax error! \n");
                return -1;
            }

            auto begin = pos + typeTokenLength; //Start of shader type name (after "#pragma PROGRAM_" keyword)
            auto type = source.substr(begin, eol - begin);
            auto stage = GetShaderStageFromString(type);

            if (stage == PKShaderStage::MaxCount)
            {
                printf("Unsupported shader stage specified! \n");
                return -1;
            }

            pos = source.find(typeToken, nextLinePos); //Start of next shader type declaration line

            shaderSources[stage] = pos == std::string::npos ? source.substr(nextLinePos) : source.substr(nextLinePos, pos - nextLinePos);
        }

        for (auto& kv : shaderSources)
        {
            kv.second.insert(0, sharedInclude);
            InsertInstancingDefine(kv.second, kv.first, enableInstancing);
            ProcessShaderStageDefine(kv.first, kv.second);
            kv.second.insert(0, variantDefines);
            ProcessShaderVersion(kv.second);
        }

        return 0;
    }

    static int CompileGLSLToSpirV(const ShaderCompiler& compiler, PKShaderStage stage, const std::string& source_name, const std::string& source, ShaderSpriV& spirvd, ShaderSpriV& spirvr)
    {
        auto kind = shaderc_shader_kind::shaderc_glsl_infer_from_source;

        switch (stage)
        {
            case PKShaderStage::Vertex: kind = shaderc_shader_kind::shaderc_vertex_shader; break;
            case PKShaderStage::TesselationControl: kind = shaderc_shader_kind::shaderc_tess_control_shader; break;
            case PKShaderStage::TesselationEvaluation: kind = shaderc_shader_kind::shaderc_tess_evaluation_shader; break;
            case PKShaderStage::Geometry: kind = shaderc_shader_kind::shaderc_geometry_shader; break;
            case PKShaderStage::Fragment: kind = shaderc_shader_kind::shaderc_fragment_shader; break;
            case PKShaderStage::Compute: kind = shaderc_shader_kind::shaderc_compute_shader; break;
        }

        shaderc::CompileOptions options;

        options.SetAutoBindUniforms(true);
        options.SetAutoMapLocations(true);

        auto module = compiler.CompileGlslToSpv(source, kind, source_name.c_str(), options);
        auto status = module.GetCompilationStatus();

        if (status != shaderc_compilation_status_success)
        {
            printf(module.GetErrorMessage().c_str());
            printf("\n ----------BEGIN SOURCE---------- \n");

            std::istringstream iss(source);
            auto index = 0u;

            for (std::string line; std::getline(iss, line); )
            {
                printf("%i: %s \n", index++, line.c_str());
            }

            printf("\n ----------END SOURCE---------- \n");
            return -1;
        }

        spirvd = { module.cbegin(), module.cend() };

        options.SetOptimizationLevel(shaderc_optimization_level::shaderc_optimization_level_performance);

        module = compiler.CompileGlslToSpv(source, kind, source_name.c_str(), options);

        spirvr = { module.cbegin(), module.cend() };

        return 0;
    }

    static int GetReflectionModule(SpvReflectShaderModule* module, const ShaderSpriV& spriv)
    {
        if (spvReflectCreateShaderModule(spriv.size() * sizeof(uint32_t), spriv.data(), module) != SPV_REFLECT_RESULT_SUCCESS)
        {
            return -1;
        }

        return 0;
    }

    static void ReleaseReflectionData(ReflectionData& reflection)
    {
        for (auto i = 0u; i < (int)PKShaderStage::MaxCount; ++i)
        {
            if (reflection.modules[i].entry_point_count != 0)
            {
                spvReflectDestroyShaderModule(&reflection.modules[i]);
            }
        }
    }

    static void GetUniqueBindings(ReflectionData& reflection, SpvReflectShaderModule* debugModule, PKShaderStage stage)
    {
        auto* module = &reflection.modules[(uint32_t)stage];

        uint32_t bindingCount = 0u;
        uint32_t setCount = 0u;

        spvReflectEnumerateDescriptorBindings(debugModule, &bindingCount, nullptr);

        std::vector<SpvReflectDescriptorBinding*> activeBindings;
        activeBindings.resize(bindingCount);
        spvReflectEnumerateDescriptorBindings(debugModule, &bindingCount, activeBindings.data());

        for (auto i = 0u; i < bindingCount; ++i)
        {
            auto desc = activeBindings.at(i);

            if (!desc->accessed)
            {
                continue;
            }

            auto releaseBinding = spvReflectGetDescriptorBinding(module, desc->binding, desc->set, nullptr);

            if (releaseBinding == nullptr)
            {
                continue;
            }

            auto name = std::string(desc->name);

            if (name.empty())
            {
                name = std::string(desc->type_description->type_name);
            }

            reflection.setStageFlags[desc->set] |= 1 << (int)stage;

            auto& binding = reflection.uniqueBindings[name];
            binding.firstStage = binding.firstStage > (int)stage ? (int)stage : binding.firstStage;
            binding.bindings[(int)stage] = releaseBinding;
        }
    }

    static void GetVertexAttributes(SpvReflectShaderModule* debugModule, PKShaderStage stage, PKVertexAttribute* attributes)
    {
        if (stage != PKShaderStage::Vertex)
        {
            return;
        }

        auto count = 0u;
        spvReflectEnumerateEntryPointInputVariables(debugModule, debugModule->entry_point_name, &count, nullptr);

        std::vector<SpvReflectInterfaceVariable*> variables;
        variables.resize(count);

        spvReflectEnumerateEntryPointInputVariables(debugModule, debugModule->entry_point_name, &count, variables.data());

        auto i = 0;

        for (auto* variable : variables)
        {
            // Ignore built in variables
            if (variable->built_in != -1)
            {
                continue;
            }

            attributes[i].location = variable->location;
            WriteName(attributes[i].name, variable->name);
            attributes[i++].type = GetElementType(variable->format);
        }
    }

    static void GetPushConstants(ReflectionData& reflection, SpvReflectShaderModule* debugModule, PKShaderStage stage)
    {
        auto* module = &reflection.modules[(uint32_t)stage];

        auto count = 0u;
        spvReflectEnumerateEntryPointPushConstantBlocks(debugModule, debugModule->entry_point_name, &count, nullptr);

        std::vector<SpvReflectBlockVariable*> blocks;
        blocks.resize(count);

        spvReflectEnumerateEntryPointPushConstantBlocks(debugModule, debugModule->entry_point_name, &count, blocks.data());

        auto i = 0u;

        for (auto* block : blocks)
        {
            auto name = std::string(block->name);
            reflection.variableStageFlags[name] |= 1 << (int)stage;

            if (reflection.uniqueVariables.count(name) <= 0)
            {
                reflection.uniqueVariables[name] = spvReflectGetPushConstantBlock(module, i, nullptr);
            }

            ++i;
        }
    }

    static void CompressBindIndices(ReflectionData& reflection)
    {
        std::map<uint_t, uint_t> setCounters;
        std::map<uint_t, uint_t> setRemap;
        std::vector<SpvReflectDescriptorSet*> sets;
        auto setCount = 0u;
        auto setIndex = 0u;

        decltype(reflection.setStageFlags) newStageFlags;
        newStageFlags.swap(reflection.setStageFlags);

        for (auto& kv : newStageFlags)
        {
            reflection.setStageFlags[setIndex] = kv.second;
            setRemap[kv.first] = setIndex++;
        }

        reflection.setCount = setIndex;

        for (auto i = 0u; i < (int)PKShaderStage::MaxCount; ++i)
        {
            if (reflection.modules[i]._internal == nullptr)
            {
                continue;
            }

            auto& module = reflection.modules[i];
            spvReflectEnumerateDescriptorSets(&module, &setCount, nullptr);
            sets.resize(setCount);
            spvReflectEnumerateDescriptorSets(&module, &setCount, sets.data());

            for (auto set : sets)
            {
                if (set->set != setRemap[set->set])
                {
                    spvReflectChangeDescriptorSetNumber(&module, set, setRemap[set->set]);
                }
            }
        }

        for (auto& kv : reflection.uniqueBindings)
        {
            auto desc = kv.second.get();
            auto bindId = setCounters[desc->set];
            setCounters[desc->set]++;
            
            for (auto i = 0u; i < (int)PKShaderStage::MaxCount; ++i)
            {
                if (kv.second.bindings[i] != nullptr)
                {
                    auto currentId = kv.second.bindings[i]->binding;
                    spvReflectChangeDescriptorBindingNumbers(&reflection.modules[i], kv.second.bindings[i], bindId, desc->set);
                }
            }
        }
    }

    int WriteShader(const char* pathSrc, const char* pathDst)
    {
        auto filename = StringUtilities::ReadFileName(pathSrc);
        
        printf("Preprocessing shader: %s \n", filename.c_str());

        auto buffer = PKAssetBuffer();
        buffer.header.get()->type = PKAssetType::Shader;
        WriteName(buffer.header.get()->name, filename.c_str());

        auto shader = buffer.Allocate<PKShader>();

        ShaderCompiler compiler;
        uint_t directiveCount;
        std::string source;
        std::string sharedInclude;
        std::string variantDefines;
        std::vector<std::vector<std::string>> mckeywords;
        std::vector<PKShaderKeyword> keywords;
        std::vector<PKMaterialProperty> materialProperties;
        std::unordered_map<PKShaderStage, std::string> shaderSources;

        ReadFile(pathSrc, source);
        ExtractMulticompiles(source, mckeywords, keywords, &shader.get()->variantcount, &directiveCount);
        ExtractStateAttributes(source, &shader.get()->attributes);
        ProcessInstancingProperties(source, materialProperties);
        ConvertHLSLTypesToGLSL(source);
        GetSharedInclude(source, sharedInclude);
        InsertRequiredExtensions(sharedInclude);

        auto enableInstancing = materialProperties.size() > 0;
        shader.get()->keywordCount = (uint_t)keywords.size();
        shader.get()->materialPropertyCount = (uint_t)materialProperties.size();
        
        if (keywords.size() > 0)
        {
            auto pKeywords = buffer.Write<PKShaderKeyword>(keywords.data(), keywords.size());
            shader.get()->keywords.Set(buffer.data(), pKeywords);
        }

        if (materialProperties.size() > 0)
        {
            auto pMaterialProperties = buffer.Write<PKMaterialProperty>(materialProperties.data(), materialProperties.size());
            shader.get()->materialProperties.Set(buffer.data(), pMaterialProperties);
        }

        auto pVariants = buffer.Allocate<PKShaderVariant>(shader.get()->variantcount);
        shader.get()->variants.Set(buffer.data(), pVariants);

        for (uint32_t i = 0; i < shader.get()->variantcount; ++i)
        {
            GetVariantDefines(mckeywords, i, variantDefines);

            if (ProcessStageSources(source, sharedInclude, variantDefines, shaderSources, enableInstancing) != 0)
            {
                printf("Failed to preprocess shader variant glsl stage sources! \n");
                return -1;
            }

            ReflectionData reflectionData{};

            for (auto& kv : shaderSources)
            {
                printf("Compiling %s variant %i stage % i \n", filename.c_str(), i, (int)kv.first);

                ShaderSpriV spirvd;
                ShaderSpriV spirvr;

                if (CompileGLSLToSpirV(compiler, kv.first, filename, kv.second, spirvd, spirvr) != 0)
                {
                    printf("Failed to compile spirv from shader variant source! \n");
                    return -1;
                }

                SpvReflectShaderModule moduled;
                auto* moduler = &reflectionData.modules[(uint32_t)kv.first];

                if (GetReflectionModule(moduler, spirvr) != 0)
                {
                    printf("Failed to extract reflection data from shader variant source! \n");
                    return -1;
                }

                if (GetReflectionModule(&moduled, spirvd) != 0)
                {
                    printf("Failed to extract reflection data from shader variant source! \n");
                    return -1;
                }

                GetUniqueBindings(reflectionData, &moduled, kv.first);
                GetVertexAttributes(&moduled, kv.first, pVariants[i].vertexAttributes);
                GetPushConstants(reflectionData, &moduled, kv.first);

                spvReflectDestroyShaderModule(&moduled);
            }

            CompressBindIndices(reflectionData);

            for (auto& kv : shaderSources)
            {
                auto size = spvReflectGetCodeSize(&reflectionData.modules[(int)kv.first]);
                auto code = spvReflectGetCode(&reflectionData.modules[(int)kv.first]);
                auto pSpirv = buffer.Write(code, size / sizeof(uint32_t));
                pVariants[i].sprivSizes[(int)kv.first] = size;
                pVariants[i].sprivBuffers[(int)kv.first].Set(buffer.data(), pSpirv);
            }

            if (i == 0)
            {
                shader.get()->type = pVariants[i].sprivSizes[(int)PKShaderStage::Compute] != 0 ? Type::Compute : Type::Graphics;
            }

            if (reflectionData.uniqueVariables.size() > 0)
            {
                pVariants[i].constantVariableCount = (uint_t)reflectionData.uniqueVariables.size();
                auto pConstantVariables = buffer.Allocate<PKConstantVariable>(reflectionData.uniqueVariables.size());
                pVariants[i].constantVariables.Set(buffer.data(), pConstantVariables);

                auto j = 0u;
                for (auto& kv : reflectionData.uniqueVariables)
                {
                    WriteName(pConstantVariables[j].name, kv.first.c_str());
                    pConstantVariables[j].offset = (unsigned short)kv.second->offset;
                    pConstantVariables[j].stageFlags = reflectionData.variableStageFlags[kv.first];
                    pConstantVariables[j++].size = kv.second->size;
                }
            }

            if (reflectionData.setCount > 0)
            {
                pVariants[i].descriptorSetCount = reflectionData.setCount;
                auto pDescriptorSets = buffer.Allocate<PKDescriptorSet>(reflectionData.setCount);
                pVariants[i].descriptorSets.Set(buffer.data(), pDescriptorSets);

                std::map<uint_t, std::vector<PKDescriptor>> descriptors;

                for (auto& kv : reflectionData.uniqueBindings)
                {
                    auto desc = kv.second.get();

                    if (desc->set >= PK_ASSET_MAX_DESCRIPTOR_SETS)
                    {
                        printf("Warning has a descriptor set outside of supported range (%i / %i) \n", desc->set, PK_ASSET_MAX_DESCRIPTOR_SETS);
                        continue;
                    }
    
                    PKDescriptor descriptor{};
                    descriptor.binding = desc->binding;
                    descriptor.count = desc->count;
                    descriptor.type = GetResourceType(desc->descriptor_type);
                    WriteName(descriptor.name, kv.first.c_str());
                    descriptors[desc->set].push_back(descriptor);
                }

                for (auto& kv : descriptors)
                {
                    pDescriptorSets[kv.first].descriptorCount = (uint_t)kv.second.size();
                    pDescriptorSets[kv.first].stageflags = reflectionData.setStageFlags[kv.first];
                    auto pDescriptors = buffer.Write(kv.second.data(), kv.second.size());
                    pDescriptorSets[kv.first].descriptors.Set(buffer.data(), pDescriptors);
                }
            }

            ReleaseReflectionData(reflectionData);
        }

        return WriteAsset(pathDst, buffer);
    }
}