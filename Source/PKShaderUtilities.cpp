#include <algorithm>
#include <cstring>
#include <string_view>
#include "PKStringUtilities.h"
#include "PKShaderUtilities.h"

namespace PKAssets::Shader
{
    struct ResourceRefTypeDesc
    {
        const char* nameRef;
        const char* nameRaw;
        PKElementType type;
        bool isReadOnly;
    };

    struct ScalarAliasDesc
    {
        const char* name;
        PKElementType type;
    };

    constexpr const static ResourceRefTypeDesc PK_RESOURCE_REF_TYPES[] =
    {
        { "SamplerRef",               "SamplerState",                    PKElementType::Sampler,               true  },
        { "SamplerCmpRef",            "SamplerComparisonState",          PKElementType::Sampler,               true  },
        { "Texture2DRef",             "Texture2D",                       PKElementType::Texture,               true  },
        { "Texture3DRef",             "Texture3D",                       PKElementType::Texture,               true  },
        { "TextureCubeRef",           "TextureCube",                     PKElementType::Texture,               true  },
        { "Texture2DArrayRef",        "Texture2DArray",                  PKElementType::Texture,               true  },
        { "TextureCubeArrayRef",      "TextureCubeArray",                PKElementType::Texture,               true  },
        { "Texture2DMSRef",           "Texture2DMS",                     PKElementType::Texture,               true  },
        { "RWTexture2DRef",           "RWTexture2D",                     PKElementType::Image,                 false },
        { "RWTexture2DArrayRef",      "RWTexture2DArray",                PKElementType::Image,                 false },
        { "RWTexture3DRef",           "RWTexture3D",                     PKElementType::Image,                 false },
        { "StructuredBufferRef",      "PKStructuredBuffer",              PKElementType::StorageBuffer,         true  },
        { "RWStructuredBufferRef",    "PKRWStructuredBuffer",            PKElementType::StorageBuffer,         false },
        { "ByteAddressBufferRef",     "ByteAddressBuffer",               PKElementType::StorageBuffer,         true  },
        { "RWByteAddressBufferRef",   "RWByteAddressBuffer",             PKElementType::StorageBuffer,         false },
        { "CBufferRef",               "PKCBuffer",                       PKElementType::ConstantBuffer,        true  },
        { "AccelerationStructureRef", "RaytracingAccelerationStructure", PKElementType::AccelerationStructure, true  }
    };

    constexpr const static ScalarAliasDesc PK_SCALAR_ALIASES[] =
    {
        { "uint8_t",   PKElementType::Byte   },
        { "int8_t",    PKElementType::Sbyte  },
        { "int16_t",   PKElementType::Short  },
        { "uint16_t",  PKElementType::Ushort },
        { "float16_t", PKElementType::Half   },
        { "int64_t",   PKElementType::Long   },
        { "uint64_t",  PKElementType::Ulong  },
    };

    const static char* PKElementType_NAMES[] =
    {
        "Invalid",
        "bool",   "bool2",   "bool3",   "bool4",
        "byte",   "byte2",   "byte3",   "byte4",
        "sbyte",  "sbyte2",  "sbyte3",  "sbyte4",
        "short",  "short2",  "short3",  "short4",
        "ushort", "ushort2", "ushort3", "ushort4",
        "half",   "half2",   "half3",   "half4",
        "float",  "float2",  "float3",  "float4",
        "int",    "int2",    "int3",    "int4",
        "uint",   "uint2",   "uint3",   "uint4",
        "long",   "long2",   "long3",   "long4",
        "ulong",  "ulong2",  "ulong3",  "ulong4",
        "double", "double2", "double3", "double4",
        "float2x2", "float2x3", "float2x4",
        "float3x2", "float3x3", "float3x4",
        "float4x2", "float4x3", "float4x4",
        "double2x2", "double3x3", "double4x4",
        "half2x2",   "half3x3",   "half4x4",
        "SamplerRef",
        "Texture2DRef",
        "RWTexture2DRef",
        "CBufferRef",
        "StructuredBufferRef",
        "AccelerationStructureRef",
        "keyword"
    };

    const static char* PKComparison_NAMES[] = { "Off", "Never", "Less", "Equal", "LessEqual", "Greater", "NotEqual", "GreaterEqual", "Always", };

    const static char* PKCullMode_NAMES[] = { "Off", "Front", "Back" };

    const static char* PKBlendFactor_NAMES[] =
    {
        "None",
        "One",
        "Zero",
        "SrcColor",
        "SrcAlpha",
        "DstColor",
        "DstAlpha",
        "OneMinusSrcColor",
        "OneMinusSrcAlpha",
        "OneMinusDstColor",
        "OneMinusDstAlpha",
        "ConstColor",
        "OneMinusConstColor",
        "ConstAlpha",
        "OneMinusConstAlpha",
    };

    const static char* PKBlendOp_NAMES[] =
    {
        "None", "Add", "Subtract", "ReverseSubtract", "Min", "Max",
    };

    const static char* PKRasterMode_NAMES[] =
    {
        "Default", "OverEstimate", "UnderEstimate",
    };

    template<size_t size>
    static uint32_t FindEnumIndexFromString(const char* const (&arr)[size], const char* str, uint32_t fallback)
    {
        if (!str || !str[0])
        {
            return fallback;
        }

        for (auto i = 0u; i < size; ++i)
        {
            if (strcmp(arr[i], str) == 0)
            {
                return i;
            }
        }

        return fallback;
    }

    #define DECLARE_STRING_TO_ENUM(TType, TFallback) TType StringTo##TType(const char* str) { return (TType)FindEnumIndexFromString(TType##_NAMES, str, (uint32_t)TFallback); }
    DECLARE_STRING_TO_ENUM(PKShaderStage, PKShaderStage::EnumCount)
    DECLARE_STRING_TO_ENUM(PKComparison, PKComparison::Off)
    DECLARE_STRING_TO_ENUM(PKCullMode, PKCullMode::Off)
    DECLARE_STRING_TO_ENUM(PKBlendFactor, PKBlendFactor::None)
    DECLARE_STRING_TO_ENUM(PKBlendOp, PKBlendOp::None)
    DECLARE_STRING_TO_ENUM(PKRasterMode, PKRasterMode::Default)
    DECLARE_STRING_TO_ENUM(PKElementType, PKElementType::Invalid)
    #undef DECLARE_STRING_TO_ENUM

    PKColorMask StringToPKColorMask(const char* str)
    {
        const auto length = str ? strnlen(str, 4) : 0;
        auto mask = (uint8_t)PKColorMask::NONE;

        if (length == 0)
        {
            return PKColorMask::RGBA;
        }

        for (auto i = 0u; i < length; ++i)
        {
            switch (str[i])
            {
                case 'X': case 'R': mask |= (uint8_t)PKColorMask::R; break;
                case 'Y': case 'G': mask |= (uint8_t)PKColorMask::G; break;
                case 'Z': case 'B': mask |= (uint8_t)PKColorMask::B; break;
                case 'W': case 'A': mask |= (uint8_t)PKColorMask::A; break;
                default: break;
            }
        }

        return (PKColorMask)mask;
    }

    SlangStage PKShaderStageToSlangStage(PKShaderStage stage)
    {
        switch (stage)
        {
            case PKShaderStage::Vertex:                return SLANG_STAGE_VERTEX;
            case PKShaderStage::TesselationControl:    return SLANG_STAGE_HULL;
            case PKShaderStage::TesselationEvaluation: return SLANG_STAGE_DOMAIN;
            case PKShaderStage::Geometry:              return SLANG_STAGE_GEOMETRY;
            case PKShaderStage::MeshTask:              return SLANG_STAGE_AMPLIFICATION;
            case PKShaderStage::MeshAssembly:          return SLANG_STAGE_MESH;
            case PKShaderStage::Fragment:              return SLANG_STAGE_FRAGMENT;
            case PKShaderStage::Compute:               return SLANG_STAGE_COMPUTE;
            case PKShaderStage::RayGeneration:         return SLANG_STAGE_RAY_GENERATION;
            case PKShaderStage::RayMiss:               return SLANG_STAGE_MISS;
            case PKShaderStage::RayClosestHit:         return SLANG_STAGE_CLOSEST_HIT;
            case PKShaderStage::RayAnyHit:             return SLANG_STAGE_ANY_HIT;
            case PKShaderStage::RayIntersection:       return SLANG_STAGE_INTERSECTION;
            default:                                   return SLANG_STAGE_NONE;
        }
    }

    const char* PKShaderStageToSlangAttribute(PKShaderStage stage)
    {
        switch (stage)
        {
            case PKShaderStage::Vertex:                 return "[shader(\"vertex\")] ";
            case PKShaderStage::TesselationControl:     return "[shader(\"hull\")] ";
            case PKShaderStage::TesselationEvaluation:  return "[shader(\"domain\")] ";
            case PKShaderStage::Geometry:               return "[shader(\"geometry\")] ";
            case PKShaderStage::MeshTask:               return "[shader(\"amplification\")] ";
            case PKShaderStage::MeshAssembly:           return "[shader(\"mesh\")] ";
            case PKShaderStage::Fragment:               return "[shader(\"fragment\")] ";
            case PKShaderStage::Compute:                return "[shader(\"compute\")] ";
            case PKShaderStage::RayGeneration:          return "[shader(\"raygeneration\")] ";
            case PKShaderStage::RayMiss:                return "[shader(\"miss\")] ";
            case PKShaderStage::RayClosestHit:          return "[shader(\"closesthit\")] ";
            case PKShaderStage::RayAnyHit:              return "[shader(\"anyhit\")] ";
            case PKShaderStage::RayIntersection:        return "[shader(\"intersection\")] ";
            default:                                    return nullptr;
        }
    }

    PKElementType ReflectElementType(std::string_view string, bool* outReadonly, std::string* outTargetType)
    {
        const auto templateBeg = string.find('<');
        const auto hasTemplate = templateBeg != std::string_view::npos && string.back() == '>';
        const std::string name(hasTemplate ? string.substr(0, templateBeg) : string);
        const std::string templateArg(hasTemplate ? string.substr(templateBeg + 1ull, string.size() - templateBeg - 2ull) : "");

        for (const auto& referenceDesc : PK_RESOURCE_REF_TYPES)
        {
            if (name == referenceDesc.nameRef)
            {
                if (outReadonly)
                {
                    *outReadonly = referenceDesc.isReadOnly;
                }

                if (outTargetType)
                {
                    if (!referenceDesc.nameRaw)
                    {
                        *outTargetType = templateArg;
                    }
                    else if (hasTemplate)
                    {
                        *outTargetType = std::string(referenceDesc.nameRaw) + "<" + templateArg + ">";
                    }
                    else 
                    {
                        *outTargetType = referenceDesc.nameRaw;
                    }
                }

                return referenceDesc.type;
            }
        }

        if (!hasTemplate)
        {
            auto type = StringToPKElementType(name.c_str());

            if (type == PKElementType::Invalid)
            {
                for (const auto& alias : PK_SCALAR_ALIASES)
                {
                    if (name == alias.name)
                    {
                        type = alias.type;
                        break;
                    }
                }
            }

            if (type != PKElementType::Invalid && type != PKElementType::Keyword)
            {
                if (outReadonly)
                {
                    *outReadonly = true;
                }
                
                if (outTargetType)
                {
                    *outTargetType = string;
                }
                
                return type;
            }
        }

        return PKElementType::Invalid;
    }

    std::string FormatErrorMessage(const std::string& name, const std::string& source, const std::string& error)
    {
        std::string output;

        for (const auto& line : StringUtilities::Split(error, "\r\n"))
        {
            output.append(line + "\n");
            auto lineIndex = 0ull;

            if (StringUtilities::ExtractDiagnosticLineIndex(line, name, &lineIndex) && lineIndex)
            {
                output.append(StringUtilities::GetLineAtIndex(source, lineIndex - 1ull));
            }
        }

        return output;
    }

    void ExtractLogVerbose(std::string& source, bool* outValue) { *outValue = !StringUtilities::ExtractToken(PK_SHADER_ATTRIB_LOGVERBOSE, source, true).empty(); }

    void ExtractGenerateDebugInfo(std::string& source, bool* outValue) { *outValue = !StringUtilities::ExtractToken(PK_SHADER_ATTRIB_GENERATEDEBUGINFO, source, true).empty(); }

    void ExtractStateAttributes(std::string& source, PKShaderFixedStateAttributes* attributes)
    {
        const auto valueZWrite = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_ZWRITE, source, false, true);
        const auto valueZTest = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_ZTEST, source, false, true);
        const auto valueBlendColor = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_BLENDCOLOR, source, false);
        const auto valueBlendAlpha = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_BLENDALPHA, source, false);
        const auto valueColorMask = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_COLORMASK, source, false, true);
        const auto valueCull = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_CULL, source, false, true);
        const auto valueOffset = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_OFFSET, source, false);
        const auto valueRasterMode = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_RASTERMODE, source, false);

        if (!valueZWrite.empty())
        {
            attributes->zwrite = valueZWrite == "True" ? 1 : 0;
        }

        if (!valueZTest.empty())
        {
            attributes->ztest = StringToPKComparison(valueZTest.c_str());
        }

        if (!valueBlendColor.empty())
        {
            auto keywords = StringUtilities::Split(valueBlendColor, " \n\r");

            attributes->blendSrcFactorColor = PKBlendFactor::None;
            attributes->blendDstFactorColor = PKBlendFactor::None;
            attributes->blendOpColor = PKBlendOp::None;

            if (keywords.size() == 3)
            {
                attributes->blendOpColor = StringToPKBlendOp(keywords.at(0).c_str());
                attributes->blendSrcFactorColor = StringToPKBlendFactor(keywords.at(1).c_str());
                attributes->blendDstFactorColor = StringToPKBlendFactor(keywords.at(2).c_str());
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
                attributes->blendOpAlpha = StringToPKBlendOp(keywords.at(0).c_str());
                attributes->blendSrcFactorAlpha = StringToPKBlendFactor(keywords.at(1).c_str());
                attributes->blendDstFactorAlpha = StringToPKBlendFactor(keywords.at(2).c_str());
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


        if (!valueRasterMode.empty())
        {
            auto keywords = StringUtilities::Split(valueRasterMode, " \n\r");

            attributes->rasterMode = PKRasterMode::Default;
            attributes->overEstimation = 0x0;

            if (keywords.size() > 0)
            {
                attributes->rasterMode = StringToPKRasterMode(keywords.at(0).c_str());
            }

            if (keywords.size() > 1)
            {
                attributes->overEstimation = (uint8_t)std::stoi(keywords.at(1));
            }
        }


        attributes->colorMask = StringToPKColorMask(valueColorMask.c_str());
        attributes->cull = StringToPKCullMode(valueCull.c_str());
    }

    void ExtractPushConstants(std::string& source, SourcePushConstants& members)
    {
        auto currentpos = 0ull;

        while (true)
        {
            auto varBeg = source.find("uniform ", currentpos);
            auto varEnd = 0ull;
            auto count = 1u;
            std::string type;
            std::string name;
            std::string field;
            std::string replacement;
            std::string targetType;
            auto isReadOnly = true;
            auto isDuplicate = false;

            if (varBeg == std::string::npos)
            {
                break;
            }

            if ((varBeg && StringUtilities::IsAlphaNumeric(source[varBeg - 1ull])) || 
                !StringUtilities::ParseUniform(source, varBeg + 8ull, type, name, &count, &varEnd))
            {
                currentpos = varBeg + 8ull;
                continue;
            }

            auto elementType = ReflectElementType(type, &isReadOnly, &targetType);

            if (elementType == PKElementType::Invalid)
            {
                currentpos = varEnd;
                continue;
            }

            const auto arraySuffix = count > 1u ? "[" + std::to_string(count) + "]" : std::string();

            if (PKElementTypeIsResourceHandle(elementType))
            {
                const auto handleType = 
                    elementType == PKElementType::Sampler ? std::string("uint8_t") : 
                    elementType == PKElementType::AccelerationStructure ? std::string("uint64_t") : std::string("uint16_t");

                const auto aliasName = name + PK_SHADER_ATTRIB_ALIAS;
                field = handleType + " " + aliasName + arraySuffix + ";\n";

                if (count > 1u)
                {
                    const auto structName = name + "_pktype";
                    replacement = "struct " + structName + "{__subscript(uint i)->" + targetType + " {get{" + type + " r;r.index=" + aliasName + "[i];return r.Get();}}};static const " + structName + " " + name + "={};";
                }
                else
                {
                    replacement = "property " + targetType + " " + name + "{get{" + type + " r;r.index=" + aliasName + ";return r.Get();}}";
                }
            }
            else
            {
                field = type + " " + name + arraySuffix + ";\n";
            }

            for (auto& member : members)
            {
                if (member.name == name)
                {
                    if (member.field != field)
                    {
                        printf("Warning constant parameter '%s' was declared again with a different format '%s'\n", member.field.c_str(), field.c_str());
                    }

                    member.isReadOnly &= isReadOnly;
                    isDuplicate = true;
                    break;
                }
            }

            if (!isDuplicate)
            {
                members.push_back({ name, field, elementType, count, isReadOnly });
            }

            if (!replacement.empty())
            {
                source.replace(varBeg, varEnd - varBeg, replacement);
                currentpos = varBeg + replacement.size();
            }
            else
            {
                StringUtilities::ClearTextRange(source, varBeg, varEnd);
                currentpos = varEnd;
            }
        }
    }

    std::string CompilePushConstantBlock(const SourcePushConstants& constants)
    {
        if (!constants.empty())
        {
            std::vector<SourcePushConstant> sorted = constants;

            std::stable_sort(sorted.begin(), sorted.end(), [](const SourcePushConstant& a, const SourcePushConstant& b)
            {
                const auto alignA = PKElementTypeToScalarSize(a.type);
                const auto alignB = PKElementTypeToScalarSize(b.type);
                if (alignA != alignB)
                {
                    return alignA > alignB;
                }

                const auto sizeA = PKElementTypeToSize(a.type) * a.arraySize;
                const auto sizeB = PKElementTypeToSize(b.type) * b.arraySize;
                if (sizeA != sizeB)
                {
                    return sizeA > sizeB;
                }

                return a.name < b.name;
            });

            auto layout = std::string("[[vk::push_constant]]\ncbuffer pk_global_push_constant_block\n{\n");

            for (const auto& member : sorted)
            {
                layout.append(member.field);
            }

            layout.append("};\n");
            return layout;
        }

        return {};
    }
}
