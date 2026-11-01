#include <algorithm>
#include <sstream>
#include "PKStringUtilities.h"
#include "PKAssetWriter.h"
#include "PKShaderUtilities.h"

namespace PKAssets::Shader
{
    struct TokenStatement
    {
        simplecpp::Token* start = nullptr;
        simplecpp::Token* end = nullptr;
        simplecpp::Token* openBrace = nullptr;
        simplecpp::Token* closeBrace = nullptr;
        simplecpp::Token* functionName = nullptr;
        bool isFunction = false;
    };

    const static char* PKUniform_NAMES[] =
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
        "Texture2D",
        "Texture3D",
        "TextureCube",
        "keyword",
    };

    const static char* PKComparison_NAMES[] =
    {
        "Off",
        "Never",
        "Less",
        "Equal",
        "LessEqual",
        "Greater",
        "NotEqual",
        "GreaterEqual",
        "Always",
    };

    const static char* PKCullMode_NAMES[] =
    {
        "Off",
        "Front",
        "Back"
    };

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
        "None",
        "Add",
        "Subtract",
        "ReverseSubtract",
        "Min",
        "Max",
    };

    const static char* PKRasterMode_NAMES[] =
    {
        "Default",
        "OverEstimate",
        "UnderEstimate",
    };

    template<size_t size>
    static uint32_t FindEnumIndexFromString(const char* const (&arr)[size], const char* str, uint32_t fallback)
    {
        auto length = str ? strnlen(str, 64) : 0u;

        for (auto i = 0u; length && i < size; ++i)
        {
            if (strlen(arr[i]) == length && strncmp(arr[i], str, length) == 0)
            {
                return i;
            }
        }

        return fallback;
    }

    #define DECLARE_STRING_TO_ENUM(TType, TFallback) TType StringTo##TType(const char* str) { return (TType)FindEnumIndexFromString(TType##_NAMES, str, (uint32_t)TFallback); }
    DECLARE_STRING_TO_ENUM(PKUniform, PKUniform::Invalid)
    DECLARE_STRING_TO_ENUM(PKShaderStage, PKShaderStage::EnumCount)
    DECLARE_STRING_TO_ENUM(PKComparison, PKComparison::Off)
    DECLARE_STRING_TO_ENUM(PKCullMode, PKCullMode::Off)
    DECLARE_STRING_TO_ENUM(PKBlendFactor, PKBlendFactor::None)
    DECLARE_STRING_TO_ENUM(PKBlendOp, PKBlendOp::None)
    DECLARE_STRING_TO_ENUM(PKRasterMode, PKRasterMode::Default)
    #undef DECLARE_STRING_TO_ENUM

    static void EraseTokenRange(simplecpp::TokenList& tokens, simplecpp::Token* first, simplecpp::Token* last)
    {
        auto* token = first;

        while (token != nullptr)
        {
            auto* next = token == last ? nullptr : token->next;
            tokens.deleteToken(token);
            token = next;
        }
    }

    static simplecpp::Token* FindTokenScope(simplecpp::Token* start, simplecpp::Token* end, char openOp, char closeOp)
    {
        if (start == nullptr || start->op != openOp)
        {
            return nullptr;
        }

        auto depth = 0;

        for (auto* token = start; token != nullptr && token != end; token = token->next)
        {
            if (token->op == openOp)
            {
                depth++;
            }
            else if (token->op == closeOp)
            {
                if (--depth == 0)
                {
                    return token;
                }
            }
        }

        return nullptr;
    }

    static simplecpp::Token* SkipLeadingAttributes(simplecpp::Token* start, simplecpp::Token* end)
    {
        auto* token = start;

        while (token != nullptr && token != end && token->op == '[')
        {
            auto* closeBracket = FindTokenScope(token, end, '[', ']');

            if (closeBracket == nullptr)
            {
                break;
            }

            token = closeBracket->next;
        }

        return token;
    }

    static std::unordered_map<std::string, uint32_t> BuildIdentifierUsageMap(const simplecpp::TokenList& tokens)
    {
        std::unordered_map<std::string, uint32_t> counts;

        for (const auto* token = tokens.cfront(); token != nullptr; token = token->next)
        {
            if (token->name && (token->previous == nullptr || token->previous->op != '.'))
            {
                counts[token->str()]++;
            }
        }

        return counts;
    }

    static uint32_t GetIdentifierUsageCount(const std::unordered_map<std::string, uint32_t>& usageMap, const std::string& name)
    {
        const auto iterator = usageMap.find(name);
        return iterator != usageMap.end() ? iterator->second : 0u;
    }

    static const EntryPointInfo* FindActiveEntryPoint(const std::string& name, EntryPointInfo* const* activeEntries)
    {
        for (auto stageIndex = 0u; stageIndex < (uint32_t)PKShaderStage::EnumCount; ++stageIndex)
        {
            if (activeEntries[stageIndex] && activeEntries[stageIndex]->name == name)
            {
                return activeEntries[stageIndex];
            }
        }

        return nullptr;
    }

    static bool IsDeclaredEntryPoint(const std::string& name, const std::vector<EntryPointInfo>& entries)
    {
        for (const auto& entry : entries)
        {
            if (entry.name == name)
            {
                return true;
            }
        }

        return false;
    }

    static std::vector<TokenStatement> ScanTopLevelStatements(simplecpp::TokenList& tokens)
    {
        std::vector<TokenStatement> statements;
        simplecpp::Token* statementStart = nullptr;
        simplecpp::Token* openBrace = nullptr;
        simplecpp::Token* closeBrace = nullptr;
        simplecpp::Token* functionName = nullptr;
        auto seenEquals = false;

        for (auto* token = tokens.front(); token != nullptr; token = token->next)
        {
            if (statementStart == nullptr)
            {
                statementStart = token;
            }

            if (token->op == '[')
            {
                auto* closeBracket = FindTokenScope(token, nullptr, '[', ']');

                if (closeBracket == nullptr)
                {
                    break;
                }

                token = closeBracket;
            }
            else if (token->op == '(')
            {
                if (openBrace == nullptr && !seenEquals && token->previous != nullptr && token->previous->name)
                {
                    functionName = token->previous;
                }

                auto* closeParen = FindTokenScope(token, nullptr, '(', ')');

                if (closeParen == nullptr)
                {
                    break;
                }

                token = closeParen;
            }
            else if (token->op == '=')
            {
                seenEquals = true;
                functionName = nullptr;
            }
            else if (token->op == '{')
            {
                openBrace = token;
                closeBrace = FindTokenScope(token, nullptr, '{', '}');

                if (closeBrace == nullptr)
                {
                    break;
                }

                token = closeBrace;
                const auto isFunction = functionName != nullptr;
                const auto isProperty = statementStart->name && statementStart->str() == "property";

                if (isFunction || isProperty)
                {
                    if (token->next != nullptr && token->next->op == ';')
                    {
                        token = token->next;
                    }

                    statements.push_back({ statementStart, token, openBrace, closeBrace, isFunction ? functionName : nullptr, isFunction });
                    statementStart = nullptr;
                    openBrace = nullptr;
                    closeBrace = nullptr;
                    functionName = nullptr;
                    seenEquals = false;
                }
            }
            else if (token->op == ';')
            {
                statements.push_back({ statementStart, token, openBrace, closeBrace, nullptr, false });
                statementStart = nullptr;
                openBrace = nullptr;
                closeBrace = nullptr;
                functionName = nullptr;
                seenEquals = false;
            }
        }

        return statements;
    }

    static simplecpp::Token* FindPropertyDeclarationName(const TokenStatement& statement)
    {
        if (statement.start != nullptr &&
            statement.openBrace != nullptr &&
            statement.start->name &&
            statement.start->str() == "property" &&
            statement.openBrace->previous != nullptr &&
            statement.openBrace->previous->name)
        {
            return statement.openBrace->previous;
        }

        return nullptr;
    }

    static simplecpp::Token* FindVariableDeclarationName(const TokenStatement& statement)
    {
        if (statement.isFunction || statement.openBrace != nullptr || statement.end == nullptr)
        {
            return nullptr;
        }

        auto* firstToken = SkipLeadingAttributes(statement.start, statement.end);

        if (firstToken == nullptr || !firstToken->name)
        {
            return nullptr;
        }

        const auto& qualifier = firstToken->str();

        if (qualifier != "uniform" && qualifier != "groupshared" && qualifier != "static")
        {
            return nullptr;
        }

        for (auto* token = firstToken->next; token != nullptr && token != statement.end->next; token = token->next)
        {
            if (token->op == '<')
            {
                token = FindTokenScope(token, statement.end->next, '<', '>');

                if (token == nullptr)
                {
                    return nullptr;
                }

                continue;
            }

            if (token->op == '(' || token->op == '=')
            {
                return nullptr;
            }

            if (token->op == '[' || token->op == ';')
            {
                return (token->previous != nullptr && token->previous->name) ? token->previous : nullptr;
            }
        }

        return nullptr;
    }

    PKColorMask StringToPKColorMask(const char* str)
    {
        auto length = str ? strnlen(str, 4) : 0;

        if (length == 0)
        {
            return PKColorMask::RGBA;
        }

        auto mask = (uint8_t)PKColorMask::NONE;

        for (auto i = 0u; i < length; ++i)
        {
            switch (str[i])
            {
                case 'X':
                case 'R': mask = mask | (uint8_t)PKColorMask::R; break;
                case 'Y':
                case 'G': mask = mask | (uint8_t)PKColorMask::G; break;
                case 'Z':
                case 'B': mask = mask | (uint8_t)PKColorMask::B; break;
                case 'W':
                case 'A': mask = mask | (uint8_t)PKColorMask::A; break;
                default: break;
            }
        }

        return (PKColorMask)mask;
    }

    PKUniform SpvFormatToPKUniform(SpvReflectFormat format)
    {
        switch (format)
        {
            case SPV_REFLECT_FORMAT_UNDEFINED:           return PKUniform::Invalid;
            case SPV_REFLECT_FORMAT_R16_SINT:            return PKUniform::Short;
            case SPV_REFLECT_FORMAT_R16G16_SINT:         return PKUniform::Short2;
            case SPV_REFLECT_FORMAT_R16G16B16_SINT:      return PKUniform::Short3;
            case SPV_REFLECT_FORMAT_R16G16B16A16_SINT:   return PKUniform::Short4;
            case SPV_REFLECT_FORMAT_R16_UINT:            return PKUniform::Ushort;
            case SPV_REFLECT_FORMAT_R16G16_UINT:         return PKUniform::Ushort2;
            case SPV_REFLECT_FORMAT_R16G16B16_UINT:      return PKUniform::Ushort3;
            case SPV_REFLECT_FORMAT_R16G16B16A16_UINT:   return PKUniform::Ushort4;
            case SPV_REFLECT_FORMAT_R16_SFLOAT:          return PKUniform::Half;
            case SPV_REFLECT_FORMAT_R16G16_SFLOAT:       return PKUniform::Half2;
            case SPV_REFLECT_FORMAT_R16G16B16_SFLOAT:    return PKUniform::Half3;
            case SPV_REFLECT_FORMAT_R16G16B16A16_SFLOAT: return PKUniform::Half4;
            case SPV_REFLECT_FORMAT_R32_SFLOAT:          return PKUniform::Float;
            case SPV_REFLECT_FORMAT_R32G32_SFLOAT:       return PKUniform::Float2;
            case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:    return PKUniform::Float3;
            case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT: return PKUniform::Float4;
            case SPV_REFLECT_FORMAT_R32_SINT:            return PKUniform::Int;
            case SPV_REFLECT_FORMAT_R32G32_SINT:         return PKUniform::Int2;
            case SPV_REFLECT_FORMAT_R32G32B32_SINT:      return PKUniform::Int3;
            case SPV_REFLECT_FORMAT_R32G32B32A32_SINT:   return PKUniform::Int4;
            case SPV_REFLECT_FORMAT_R32_UINT:            return PKUniform::Uint;
            case SPV_REFLECT_FORMAT_R32G32_UINT:         return PKUniform::Uint2;
            case SPV_REFLECT_FORMAT_R32G32B32_UINT:      return PKUniform::Uint3;
            case SPV_REFLECT_FORMAT_R32G32B32A32_UINT:   return PKUniform::Uint4;
            case SPV_REFLECT_FORMAT_R64_SINT:            return PKUniform::Long;
            case SPV_REFLECT_FORMAT_R64G64_SINT:         return PKUniform::Long2;
            case SPV_REFLECT_FORMAT_R64G64B64_SINT:      return PKUniform::Long3;
            case SPV_REFLECT_FORMAT_R64G64B64A64_SINT:   return PKUniform::Long4;
            case SPV_REFLECT_FORMAT_R64_UINT:            return PKUniform::Ulong;
            case SPV_REFLECT_FORMAT_R64G64_UINT:         return PKUniform::Ulong2;
            case SPV_REFLECT_FORMAT_R64G64B64_UINT:      return PKUniform::Ulong3;
            case SPV_REFLECT_FORMAT_R64G64B64A64_UINT:   return PKUniform::Ulong4;
            case SPV_REFLECT_FORMAT_R64_SFLOAT:          return PKUniform::Double;
            case SPV_REFLECT_FORMAT_R64G64_SFLOAT:       return PKUniform::Double2;
            case SPV_REFLECT_FORMAT_R64G64B64_SFLOAT:    return PKUniform::Double3;
            case SPV_REFLECT_FORMAT_R64G64B64A64_SFLOAT: return PKUniform::Double4;
        }

        return PKUniform::Invalid;
    }

    std::string PKUniformToSlangString(PKUniform type)
    {
        switch (type)
        {
            case PKUniform::Invalid:   return "INVALID";
            case PKUniform::Bool:      return "bool";
            case PKUniform::Bool2:     return "bool2";
            case PKUniform::Bool3:     return "bool3";
            case PKUniform::Bool4:     return "bool4";
            case PKUniform::Byte:      return "byte";
            case PKUniform::Byte2:     return "byte2";
            case PKUniform::Byte3:     return "byte3";
            case PKUniform::Byte4:     return "byte4";
            case PKUniform::Sbyte:     return "sbyte";
            case PKUniform::Sbyte2:    return "sbyte2";
            case PKUniform::Sbyte3:    return "sbyte3";
            case PKUniform::Sbyte4:    return "sbyte4";
            case PKUniform::Short:     return "short";
            case PKUniform::Short2:    return "short2";
            case PKUniform::Short3:    return "short3";
            case PKUniform::Short4:    return "short4";
            case PKUniform::Ushort:    return "ushort";
            case PKUniform::Ushort2:   return "ushort2";
            case PKUniform::Ushort3:   return "ushort3";
            case PKUniform::Ushort4:   return "ushort4";
            case PKUniform::Half:      return "half";
            case PKUniform::Half2:     return "half2";
            case PKUniform::Half3:     return "half3";
            case PKUniform::Half4:     return "half4";
            case PKUniform::Float:     return "float";
            case PKUniform::Float2:    return "float2";
            case PKUniform::Float3:    return "float3";
            case PKUniform::Float4:    return "float4";
            case PKUniform::Int:       return "int";
            case PKUniform::Int2:      return "int2";
            case PKUniform::Int3:      return "int3";
            case PKUniform::Int4:      return "int4";
            case PKUniform::Uint:      return "uint";
            case PKUniform::Uint2:     return "uint2";
            case PKUniform::Uint3:     return "uint3";
            case PKUniform::Uint4:     return "uint4";
            case PKUniform::Long:      return "long";
            case PKUniform::Long2:     return "long2";
            case PKUniform::Long3:     return "long3";
            case PKUniform::Long4:     return "long4";
            case PKUniform::Ulong:     return "ulong";
            case PKUniform::Ulong2:    return "ulong2";
            case PKUniform::Ulong3:    return "ulong3";
            case PKUniform::Ulong4:    return "ulong4";
            case PKUniform::Double:    return "double";
            case PKUniform::Double2:   return "double2";
            case PKUniform::Double3:   return "double3";
            case PKUniform::Double4:   return "double4";
            case PKUniform::Float2x2:  return "float2x2";
            case PKUniform::Float2x3:  return "float2x3";
            case PKUniform::Float2x4:  return "float2x4";
            case PKUniform::Float3x2:  return "float3x2";
            case PKUniform::Float3x3:  return "float3x3";
            case PKUniform::Float4x2:  return "float4x2";
            case PKUniform::Float4x3:  return "float4x3";
            case PKUniform::Float4x4:  return "float4x4";
            case PKUniform::Float3x4:  return "float3x4";
            case PKUniform::Double2x2: return "double2x2";
            case PKUniform::Double3x3: return "double3x3";
            case PKUniform::Double4x4: return "double4x4";
            case PKUniform::Half2x2:   return "half2x2";
            case PKUniform::Half3x3:   return "half3x3";
            case PKUniform::Half4x4:   return "half4x4";
            case PKUniform::Texture2D:   return "uint";
            case PKUniform::Texture3D:   return "uint";
            case PKUniform::TextureCube: return "uint";
            case PKUniform::Keyword:     return "INVALID";
        }

        return "INVALID";
    }

    PKDescriptorType GetResourceType(SpvReflectDescriptorType type)
    {
        switch (type)
        {
            case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER: return PKDescriptorType::Sampler;
            case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE: return PKDescriptorType::Texture;
            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE: return PKDescriptorType::Image;
            case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER: return PKDescriptorType::ConstantBuffer;
            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER: return PKDescriptorType::StorageBuffer;
            case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: return PKDescriptorType::InputAttachment;
            case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR: return PKDescriptorType::AccelerationStructure;
        }

        return PKDescriptorType::Invalid;
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

    const char* GetSlangStageAttribute(PKShaderStage stage)
    {
        switch (stage)
        {
            case PKShaderStage::Vertex:                return "[shader(\"vertex\")] ";
            case PKShaderStage::TesselationControl:    return "[shader(\"hull\")] ";
            case PKShaderStage::TesselationEvaluation: return "[shader(\"domain\")] ";
            case PKShaderStage::Geometry:              return "[shader(\"geometry\")] ";
            case PKShaderStage::MeshTask:              return "[shader(\"amplification\")] ";
            case PKShaderStage::MeshAssembly:          return "[shader(\"mesh\")] ";
            case PKShaderStage::Fragment:              return "[shader(\"fragment\")] ";
            case PKShaderStage::Compute:               return "[shader(\"compute\")] ";
            case PKShaderStage::RayGeneration:         return "[shader(\"raygeneration\")] ";
            case PKShaderStage::RayMiss:               return "[shader(\"miss\")] ";
            case PKShaderStage::RayClosestHit:         return "[shader(\"closesthit\")] ";
            case PKShaderStage::RayAnyHit:             return "[shader(\"anyhit\")] ";
            case PKShaderStage::RayIntersection:       return "[shader(\"intersection\")] ";
            default:                                   return "";
        }
    }

    std::string ReflectBindingName(SpvReflectDescriptorBinding* binding)
    {
        if (binding->name && binding->name[0] != '\0')
        {
            return std::string(binding->name);
        }

        if (binding->type_description && binding->type_description->type_name)
        {
            return std::string(binding->type_description->type_name);
        }

        return {};
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

    void ExtractLogVerbose(std::string& source, bool* outValue)
    {
        *outValue = !StringUtilities::ExtractToken(PK_SHADER_ATTRIB_LOGVERBOSE, source, true).empty();
    }

    void ExtractGenerateDebugInfo(std::string& source, bool* outValue)
    {
        *outValue = !StringUtilities::ExtractToken(PK_SHADER_ATTRIB_GENERATEDEBUGINFO, source, true).empty();
    }

    void ExtractStateAttributes(std::string& source, PKShaderFixedStateAttributes* attributes)
    {
        auto valueZWrite = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_ZWRITE, source, false, true);
        auto valueZTest = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_ZTEST, source, false, true);
        auto valueBlendColor = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_BLENDCOLOR, source, false);
        auto valueBlendAlpha = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_BLENDALPHA, source, false);
        auto valueColorMask = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_COLORMASK, source, false, true);
        auto valueCull = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_CULL, source, false, true);
        auto valueOffset = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_OFFSET, source, false);
        auto valueRasterMode = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_RASTERMODE, source, false);

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

    void ExtractMulticompiles(std::string& source, std::vector<std::string>& outVariantDefines, std::vector<PKShaderKeyword>& outKeywords)
    {
        std::vector<std::vector<std::string>> keywords{};
        std::string output;
        auto pos = 0ull;
        auto directiveCount = 0ull;
        auto variantCount = 1ull;

        while (true)
        {
            pos = StringUtilities::ExtractToken(pos, PK_SHADER_ATTRIB_MULTI_COMPILE, source, output, false);

            if (pos == std::string::npos)
            {
                break;
            }

            if (variantCount >= PK_ASSET_MAX_SHADER_KEYWORDS)
            {
                printf("Warning maximum number of shader variants exceeded!.\n");
                continue;
            }

            if (directiveCount >= PK_ASSET_MAX_SHADER_DIRECTIVES)
            {
                printf("Warning maximum number of shader multicompile directives reached!.\n");
                continue;
            }

            auto directive = StringUtilities::Split(output, " ");

            if (directive.size() >= PK_ASSET_MAX_SHADER_DIRECTIVE_SIZE)
            {
                printf("Warning multicompile directive keyword count exceeds supported limits!\n");
                continue;
            }

            for (auto i = 0u; i < directive.size(); ++i)
            {
                auto& keyword = directive.at(i);

                if (keyword != "_")
                {
                    PKShaderKeyword shaderKeyword{};
                    shaderKeyword.offset = (uint16_t)variantCount;
                    shaderKeyword.directive = directiveCount;
                    shaderKeyword.value = i;
                    WriteName(shaderKeyword.name, keyword.c_str());
                    outKeywords.push_back(shaderKeyword);
                }
            }

            keywords.push_back(directive);
            directiveCount++;
            variantCount *= directive.size();
        }

        outVariantDefines.reserve(variantCount);

        for (auto i = 0u; i < variantCount; ++i)
        {
            std::string defines;
            auto declareIndex = i;

            for (auto j = 0u; j < keywords.size(); ++j)
            {
                auto& declares = keywords.at(j);
                auto& keyword = declares.at(declareIndex % declares.size());

                if (keyword != "_")
                {
                    defines.append("#define ");
                    defines.append(keyword);
                    defines.append("\n");
                }

                declareIndex /= (uint32_t)declares.size();
            }

            outVariantDefines.push_back(defines);
        }
    }

    int ExtractEntryPoints(std::string& source, std::vector<EntryPointInfo>& outEntryPoints)
    {
        std::string output;
        auto pos = 0ull;

        while (true)
        {
            pos = StringUtilities::ExtractToken(pos, PK_SHADER_ATTRIB_PROGRAM, source, output, false);

            if (pos == std::string::npos)
            {
                break;
            }

            auto directives = StringUtilities::Split(output, " ");

            if (directives.size() != 2 && directives.size() != 3)
            {
                printf("Entry point declaration contains an invalid amount of arguments (should be stage, name)\n");
                return -1;
            }

            auto stage = StringToPKShaderStage(directives[0].c_str());

            if (stage == PKShaderStage::EnumCount)
            {
                printf("Unsupported shader stage specified!\n");
                return -1;
            }

            EntryPointInfo info;
            info.name = directives[1];
            info.stage = stage;
            info.keyword = directives.size() == 3 ? directives[2] : "";

            if (source.find(info.name, 0u) == std::string::npos)
            {
                printf("Definition for declared entry point %s was not found!\n", info.name.c_str());
                return -1;
            }

            outEntryPoints.push_back(info);
        }

        return 0;
    }

    static int32_t FindActiveEntryPointIndexForStage(const std::vector<EntryPointInfo>& entries, const std::string& defines, PKShaderStage stage)
    {
        auto maxIndex = -1;

        for (auto i = 0u; i < entries.size(); ++i)
        {
            if (entries[i].stage == stage)
            {
                if (!entries[i].keyword.empty())
                {
                    auto keypos = defines.find(entries[i].keyword, 0u);

                    if (keypos != std::string::npos && isspace((unsigned char)defines[keypos + entries[i].keyword.size()]))
                    {
                        return i;
                    }

                    continue;
                }

                maxIndex = i;
            }
        }

        return maxIndex;
    }

    void FindActiveEntryPoints(std::vector<EntryPointInfo>& entries, const std::string& variantDefines, EntryPointInfo** outStageEntries, uint32_t* outActiveStageMask, std::string& outActiveDefines)
    {
        *outActiveStageMask = 0u;
        outActiveDefines = variantDefines;

        for (auto stageIndex = 0u; stageIndex < (uint32_t)PKShaderStage::EnumCount; ++stageIndex)
        {
            const auto entryIndex = FindActiveEntryPointIndexForStage(entries, variantDefines, (PKShaderStage)stageIndex);

            if (entryIndex != -1)
            {
                outStageEntries[stageIndex] = &entries.at(entryIndex);
                *outActiveStageMask |= 1u << stageIndex;
                outActiveDefines += PK_SHADER_STAGE_DEFINES[stageIndex];
                outActiveDefines += "#define PK_ACTIVE_ENTRY_POINT_" + outStageEntries[stageIndex]->name + "\n";
            }
            else
            {
                outStageEntries[stageIndex] = nullptr;
            }
        }
    }

    int PreprocessSource(const std::string& name, const std::string& source, std::vector<std::string>& filenames, simplecpp::TokenList& outTokens)
    {
        std::istringstream stream(source);
        simplecpp::OutputList outputList;

        simplecpp::TokenList rawTokens(stream, filenames, name, {}, &outputList);
        rawTokens.removeComments();

        simplecpp::FileDataCache fileData;
        simplecpp::preprocess(outTokens, rawTokens, filenames, fileData, {}, &outputList);

        if (!outputList.empty())
        {
            printf("\n ----------BEGIN PREPROCESS ERROR---------- \n");
            for (const auto& output : outputList)
            {
                auto filename = output.location.fileIndex < filenames.size() ? filenames[output.location.fileIndex] : name;
                printf("%s:%u: %s\n", filename.c_str(), output.location.line, output.msg.c_str());
            }
            printf("\n ----------END PREPROCESS ERROR---------- \n\n");
            return -1;
        }

        return 0;
    }

    void SetActiveEntryPoints(simplecpp::TokenList& tokens, const std::vector<EntryPointInfo>& entries, EntryPointInfo* const* activeEntries)
    {
        const auto statements = ScanTopLevelStatements(tokens);

        for (auto index = (int32_t)statements.size() - 1; index >= 0; --index)
        {
            const auto& statement = statements[index];

            if (!statement.isFunction || statement.functionName == nullptr)
            {
                continue;
            }

            const auto& functionName = statement.functionName->str();

            if (!IsDeclaredEntryPoint(functionName, entries))
            {
                continue;
            }

            const auto* entry = FindActiveEntryPoint(functionName, activeEntries);

            if (entry == nullptr)
            {
                EraseTokenRange(tokens, statement.start, statement.end);
            }
            else
            {
                statement.start->setstr(std::string(GetSlangStageAttribute(entry->stage)) + statement.start->str());
            }
        }
    }

    static bool IsPushConstantPrimitiveType(const std::string& typeStr)
    {
        const auto uniformType = StringToPKUniform(typeStr.c_str());

        return uniformType != PKUniform::Invalid &&
            uniformType != PKUniform::Texture2D &&
            uniformType != PKUniform::Texture3D &&
            uniformType != PKUniform::TextureCube &&
            uniformType != PKUniform::Keyword;
    }

    void ExtractPushConstants(simplecpp::TokenList& tokens, uint32_t stageMask, SourcePushConstants& outConstants)
    {
        const auto usageMap = BuildIdentifierUsageMap(tokens);
        const auto statements = ScanTopLevelStatements(tokens);

        for (const auto& statement : statements)
        {
            if (statement.isFunction || statement.openBrace != nullptr || statement.end == nullptr)
            {
                continue;
            }

            auto* token = SkipLeadingAttributes(statement.start, statement.end);

            if (token == nullptr || !token->name || token->str() != "uniform")
            {
                continue;
            }

            auto* typeToken = token->next;

            if (typeToken == nullptr || !typeToken->name || !IsPushConstantPrimitiveType(typeToken->str()))
            {
                continue;
            }

            auto* nameToken = typeToken->next;

            if (nameToken == nullptr || !nameToken->name || nameToken->next != statement.end || statement.end->op != ';')
            {
                continue;
            }

            const auto& name = nameToken->str();
            const auto field = typeToken->str() + " " + name + ";\n";

            if (GetIdentifierUsageCount(usageMap, name) > 1u)
            {
                auto found = false;

                for (auto& member : outConstants)
                {
                    if (member.name == name)
                    {
                        if (member.field != field)
                        {
                            printf("Warning constant parameter '%s' was declared again with a different format '%s'", member.field.c_str(), field.c_str());
                        }

                        member.stageFlags |= stageMask;
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    outConstants.push_back({ name, field, stageMask });
                }
            }

            EraseTokenRange(tokens, statement.start, statement.end);
        }
    }

    void CompilePushConstantBlock(std::string& source, const SourcePushConstants& constants)
    {
        if (constants.empty())
        {
            return;
        }

        std::string layout = "[[vk::push_constant]] cbuffer pk_global_push_constant_block\n{\n";

        for (const auto& member : constants)
        {
            layout.append(member.field);
        }

        layout.append("};\n");

        source.insert(0ull, layout);
    }

    void RemoveUnusedResourcesAndFunctions(simplecpp::TokenList& tokens, EntryPointInfo* const* activeEntries)
    {
        auto changed = true;

        while (changed)
        {
            changed = false;
            const auto usageMap = BuildIdentifierUsageMap(tokens);
            const auto statements = ScanTopLevelStatements(tokens);

            for (auto index = (int32_t)statements.size() - 1; index >= 0; --index)
            {
                const auto& statement = statements[index];

                if (statement.isFunction && statement.functionName != nullptr)
                {
                    const auto& functionName = statement.functionName->str();

                    if (FindActiveEntryPoint(functionName, activeEntries) == nullptr &&
                        GetIdentifierUsageCount(usageMap, functionName) <= 1u)
                    {
                        EraseTokenRange(tokens, statement.start, statement.end);
                        changed = true;
                    }
                    continue;
                }

                if (auto* propertyNameToken = FindPropertyDeclarationName(statement))
                {
                    if (GetIdentifierUsageCount(usageMap, propertyNameToken->str()) <= 1u)
                    {
                        EraseTokenRange(tokens, statement.start, statement.end);
                        changed = true;
                    }
                    continue;
                }

                if (auto* variableNameToken = FindVariableDeclarationName(statement))
                {
                    if (GetIdentifierUsageCount(usageMap, variableNameToken->str()) <= 1u)
                    {
                        EraseTokenRange(tokens, statement.start, statement.end);
                        changed = true;
                    }
                }
            }
        }
    }
}