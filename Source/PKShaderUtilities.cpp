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

    struct UniformMeta
    {
        const char* name = nullptr;
        const char* fieldType = nullptr;
        const char* targetBase = nullptr;
        const char* defaultTemplate = nullptr;
        PKUniform type = PKUniform::Invalid;
        bool requiresTemplate = false;
    };

    constexpr const static UniformMeta PK_UNIFORM_RESOURCE_META[] =
    {
        { "SamplerRef",               "byte",   "SamplerState",                    nullptr,    PKUniform::Sampler,               false },
        { "SamplerCmpRef",            "byte",   "SamplerComparisonState",          nullptr,    PKUniform::Sampler,               false },
        { "Texture2DRef",             "ushort", "Texture2D",                       "<float4>", PKUniform::Texture,               false },
        { "Texture3DRef",             "ushort", "Texture3D",                       "<float4>", PKUniform::Texture,               false },
        { "TextureCubeRef",           "ushort", "TextureCube",                     "<float4>", PKUniform::Texture,               false },
        { "Texture2DArrayRef",        "ushort", "Texture2DArray",                  "<float4>", PKUniform::Texture,               false },
        { "TextureCubeArrayRef",      "ushort", "TextureCubeArray",                "<float4>", PKUniform::Texture,               false },
        { "Texture2DMSRef",           "ushort", "Texture2DMS",                     "<float4>", PKUniform::Texture,               false },
        { "RWTexture2DRef",           "ushort", "RWTexture2D",                     "<float4>", PKUniform::Image,                 false },
        { "RWTexture2DArrayRef",      "ushort", "RWTexture2DArray",                "<float4>", PKUniform::Image,                 false },
        { "RWTexture3DRef",           "ushort", "RWTexture3D",                     "<float4>", PKUniform::Image,                 false },
        { "StructuredBufferRef",      "ushort", "StructuredBuffer",                nullptr,    PKUniform::StorageBuffer,         true  },
        { "RWStructuredBufferRef",    "ushort", "RWStructuredBuffer",              nullptr,    PKUniform::StorageBuffer,         true  },
        { "ByteAddressBufferRef",     "ushort", "ByteAddressBuffer",               nullptr,    PKUniform::StorageBuffer,         false },
        { "RWByteAddressBufferRef",   "ushort", "RWByteAddressBuffer",             nullptr,    PKUniform::StorageBuffer,         false },
        { "ConstantBufferRef",        "ushort", "ConstantBuffer",                  nullptr,    PKUniform::ConstantBuffer,        true  },
        { "AccelerationStructureRef", "ulong",  "RaytracingAccelerationStructure", nullptr,    PKUniform::AccelerationStructure, false },
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

    static uint32_t CountIdentifierInRange(const simplecpp::Token* first, const simplecpp::Token* last, const std::string& name)
    {
        auto count = 0u;

        for (const auto* token = first; token != nullptr; token = token->next)
        {
            if (token->name && (token->previous == nullptr || token->previous->op != '.') && token->str() == name)
            {
                count++;
            }

            if (token == last)
            {
                break;
            }
        }

        return count;
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
                const auto isPrototype = openBrace == nullptr && !seenEquals && functionName != nullptr;
                statements.push_back({ statementStart, token, openBrace, closeBrace, isPrototype ? functionName : nullptr, isPrototype });
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

    static std::unordered_map<std::string, uint32_t> BuildIdentifierUsageMap(const simplecpp::TokenList& tokens, const std::vector<TokenStatement>& statements)
    {
        std::unordered_map<std::string, uint32_t> counts;

        for (const auto* token = tokens.cfront(); token != nullptr; token = token->next)
        {
            if (token->name && (token->previous == nullptr || token->previous->op != '.'))
            {
                counts[token->str()]++;
            }
        }

        // Subtract declaration tokens so the map only stores actual reference counts
        for (const auto& statement : statements)
        {
            const simplecpp::Token* declToken = nullptr;

            if (statement.isFunction)
            {
                declToken = statement.functionName;
            }
            else if (auto* propToken = FindPropertyDeclarationName(statement))
            {
                declToken = propToken;
            }
            else if (auto* varToken = FindVariableDeclarationName(statement))
            {
                declToken = varToken;
            }

            if (declToken != nullptr)
            {
                auto iterator = counts.find(declToken->str());
                if (iterator != counts.end() && iterator->second > 0u)
                {
                    iterator->second--;
                }
            }
        }

        return counts;
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

                    if (keypos != std::string::npos && isspace((uint8_t)defines[keypos + entries[i].keyword.size()]))
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


    #define DECLARE_STRING_TO_ENUM(TType, TFallback) TType StringTo##TType(const char* str) { return (TType)FindEnumIndexFromString(TType##_NAMES, str, (uint32_t)TFallback); }
    DECLARE_STRING_TO_ENUM(PKUniform, PKUniform::Invalid)
    DECLARE_STRING_TO_ENUM(PKShaderStage, PKShaderStage::EnumCount)
    DECLARE_STRING_TO_ENUM(PKComparison, PKComparison::Off)
    DECLARE_STRING_TO_ENUM(PKCullMode, PKCullMode::Off)
    DECLARE_STRING_TO_ENUM(PKBlendFactor, PKBlendFactor::None)
    DECLARE_STRING_TO_ENUM(PKBlendOp, PKBlendOp::None)
    DECLARE_STRING_TO_ENUM(PKRasterMode, PKRasterMode::Default)
    #undef DECLARE_STRING_TO_ENUM

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

    PKUniform ReflectUniformType(std::string_view typeText, std::string* outFieldType, std::string* outTargetType)
    {
        auto baseName = typeText;
        std::string_view templateArg{};

        const auto templateOpen = typeText.find('<');

        if (templateOpen != std::string_view::npos)
        {
            const auto templateClose = typeText.rfind('>');

            if (templateClose == std::string_view::npos || templateClose < templateOpen)
            {
                return PKUniform::Invalid;
            }

            baseName = typeText.substr(0ull, templateOpen);
            templateArg = typeText.substr(templateOpen, (templateClose + 1ull) - templateOpen);
        }

        if (templateArg.empty())
        {
            for (auto i = (uint32_t)PKUniform::Bool; i <= (uint32_t)PKUniform::Half4x4; ++i)
            {
                if (baseName == PKUniform_NAMES[i])
                {
                    if (outFieldType)
                    {
                        *outFieldType = std::string(baseName);
                    }

                    if (outTargetType)
                    {
                        *outTargetType = std::string(baseName);
                    }

                    return (PKUniform)i;
                }
            }
        }

        for (const auto& meta : PK_UNIFORM_RESOURCE_META)
        {
            if (baseName == meta.name)
            {
                if (meta.requiresTemplate && templateArg.empty())
                {
                    return PKUniform::Invalid;
                }

                if (outFieldType)
                {
                    *outFieldType = meta.fieldType;
                }

                if (outTargetType)
                {
                    *outTargetType = std::string(meta.targetBase);

                    if (!templateArg.empty())
                    {
                        *outTargetType += std::string(templateArg);
                    }
                    else if (meta.defaultTemplate != nullptr)
                    {
                        *outTargetType += meta.defaultTemplate;
                    }
                }

                return meta.type;
            }
        }

        return PKUniform::Invalid;
    }

    std::string CompileUniformProperty(PKUniform type, const std::string& targetType, const std::string& name, const std::string& valueExpr, bool isBitpacked)
    {
        std::string expr = valueExpr;

        if (type == PKUniform::AccelerationStructure)
        {
            expr = targetType + "(" + valueExpr + ")";
        }
        else if (PKUniformIsResourceHandle(type))
        {
            expr = "DescriptorHandle<" + targetType + ">(uint2(" + valueExpr + ",0u))";
        }
        else if (isBitpacked && type == PKUniform::Half)
        {
            expr = "half(f16tof32(" + valueExpr + "))";
        }
        else if (isBitpacked)
        {
            expr = targetType + "(" + valueExpr + ")";
        }

        return "property " + targetType + " " + name + "{get{return " + expr + ";}}\n";
    }

    PKUniform GetResourceType(SpvReflectDescriptorType type)
    {
        switch (type)
        {
            case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:                    return PKUniform::Sampler;
            case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:              return PKUniform::Texture;
            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:              return PKUniform::Image;
            case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:             return PKUniform::ConstantBuffer;
            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:             return PKUniform::StorageBuffer;
            case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:           return PKUniform::InputAttachment;
            case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR: return PKUniform::AccelerationStructure;
            default:                                                     return PKUniform::Invalid;
        }
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

    void ExtractLogDisassembly(std::string& source, bool* outValue)
    {
        *outValue = !StringUtilities::ExtractToken(PK_SHADER_ATTRIB_LOGDISASSEMBLY, source, true).empty();

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

    void ExtractPushConstants(simplecpp::TokenList& tokens, uint32_t stageMask, SourcePushConstants& outConstants)
    {
        const auto statements = ScanTopLevelStatements(tokens);
        const auto usageMap = BuildIdentifierUsageMap(tokens, statements);

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

            auto* typeStartToken = token->next;

            if (typeStartToken == nullptr || !typeStartToken->name)
            {
                continue;
            }

            auto* typeEndToken = typeStartToken;

            if (typeEndToken->next != nullptr && typeEndToken->next->op == '<')
            {
                typeEndToken = FindTokenScope(typeEndToken->next, statement.end, '<', '>');

                if (typeEndToken == nullptr)
                {
                    continue;
                }
            }

            auto* nameToken = typeEndToken->next;

            if (nameToken == nullptr || !nameToken->name)
            {
                continue;
            }

            uint16_t count = 1u;

            if (nameToken->next == statement.end && statement.end->op == ';')
            {
                count = 1u;
            }

            else if (nameToken->next != nullptr && nameToken->next->op == '[')
            {
                auto* closeBracket = FindTokenScope(nameToken->next, statement.end, '[', ']');
                auto* countToken = nameToken->next->next;

                if (closeBracket == nullptr ||
                    closeBracket->next != statement.end ||
                    statement.end->op != ';' ||
                    countToken == nullptr ||
                    !countToken->number ||
                    countToken->next != closeBracket)
                {
                    continue;
                }

                count = (uint16_t)std::stoul(countToken->str());
            }
            else
            {
                continue;
            }

            std::string typeText;

            for (auto* t = typeStartToken; t != nameToken; t = t->next)
            {
                typeText += t->str();
            }

            std::string fieldType;
            std::string targetType;
            const auto uniformType = ReflectUniformType(typeText, &fieldType, &targetType);

            if (uniformType == PKUniform::Invalid)
            {
                continue;
            }

            const auto& name = nameToken->str();

            if (GetIdentifierUsageCount(usageMap, name) > 0u)
            {
                auto found = false;

                for (auto& member : outConstants)
                {
                    if (member.name == name)
                    {
                        if (member.targetType != targetType || member.count != count)
                        {
                            printf("Warning constant parameter '%s' was declared again with a different format '%s'\n", member.name.c_str(), targetType.c_str());
                        }

                        member.stageFlags |= stageMask;
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    SourcePushConstant constant{};
                    constant.name = name;
                    constant.fieldType = fieldType;
                    constant.targetType = targetType;
                    constant.type = uniformType;
                    constant.count = count;
                    constant.stageFlags = stageMask;
                    outConstants.push_back(constant);
                }
            }

            EraseTokenRange(tokens, statement.start, statement.end);
        }
    }

    void CompilePushConstantBlock(std::string& source, SourcePushConstants& constants)
    {
        if (constants.empty())
        {
            return;
        }

        std::stable_sort(constants.begin(), constants.end(), [](const SourcePushConstant& a, const SourcePushConstant& b)
        {
            const auto sizeA = PKUniformToSize(a.type) * a.count;
            const auto sizeB = PKUniformToSize(b.type) * b.count;
            const auto isPackedA = sizeA < 4u;
            const auto isPackedB = sizeB < 4u;

            if (isPackedA != isPackedB)
            {
                return !isPackedA;
            }

            const auto alignA = PKUniformToScalarSize(a.type);
            const auto alignB = PKUniformToScalarSize(b.type);
            return alignA != alignB ? alignA > alignB : sizeA > sizeB;
        });

        auto memberIndex = 0u;
        auto packedByteOffset = 0u;
        std::string blockFields;
        std::string resourceAccessors;

        for (auto& member : constants)
        {
            const auto elementSize = PKUniformToSize(member.type);
            const auto totalSize = elementSize * member.count;
            const auto isHandle = PKUniformIsResourceHandle(member.type);

            if (totalSize < 4u)
            {
                packedByteOffset = (packedByteOffset + elementSize - 1u) & ~(elementSize - 1u);
                const auto wordIndex = packedByteOffset / 4u;

                member.memberIndex = (uint16_t)(memberIndex + wordIndex);
                member.memberSize = 4u;
                member.memberAlignment = 4u;
                member.bitOffset = (uint8_t)((packedByteOffset % 4u) * 8u);
                member.bitSize = (uint8_t)(totalSize * 8u);
                packedByteOffset += totalSize;

                const auto extractExpr = "bitfieldExtract(pk_packed_pc_" + std::to_string(wordIndex) + "," + std::to_string(member.bitOffset) + "," + std::to_string(member.bitSize) + ")";
                resourceAccessors += CompileUniformProperty(member.type, member.targetType, member.name, extractExpr, true);
                continue;
            }

            member.memberIndex = (uint16_t)(memberIndex++);
            member.bitOffset = 0u;
            member.bitSize = 0u;

            // Single resource access was handled above. no need to check count here.
            if (isHandle && member.type != PKUniform::AccelerationStructure)
            {
                const auto wordCount = (member.count + 1u) / 2u;
                member.memberSize = (uint16_t)(wordCount * 4u);
                member.memberAlignment = 4u;
                blockFields += "uint " + member.name + "_Handle[" + std::to_string(wordCount) + "];\n";
                resourceAccessors += "PK_PC_HANDLE_ARRAY(" + member.targetType + "," + member.name + ")\n";
                continue;
            }

            member.memberSize = (uint16_t)totalSize;
            member.memberAlignment = (uint8_t)PKUniformToScalarSize(member.type);

            if (isHandle)
            {
                const auto fieldName = member.name + "_Handle";
                blockFields += member.fieldType + " " + fieldName + ";\n";
                resourceAccessors += CompileUniformProperty(member.type, member.targetType, member.name, fieldName, false);
                continue;
            }

            const auto arraySuffix = member.count > 1u ? "[" + std::to_string(member.count) + "]" : "";
            blockFields += member.fieldType + " " + member.name + arraySuffix + ";\n";
        }

        const auto packedWordCount = (packedByteOffset + 3u) / 4u;

        for (auto wordIndex = 0u; wordIndex < packedWordCount; ++wordIndex)
        {
            blockFields += "uint pk_packed_pc_" + std::to_string(wordIndex) + ";\n";
        }

        source.insert(0ull, "[[vk::push_constant]] cbuffer pk_global_push_constant_block\n{\n" + blockFields + "};\n" + resourceAccessors);
    }

    void RemoveUnusedResourcesAndFunctions(simplecpp::TokenList& tokens, EntryPointInfo* const* activeEntries)
    {
        auto changed = true;

        while (changed)
        {
            changed = false;
            const auto statements = ScanTopLevelStatements(tokens);
            const auto usageMap = BuildIdentifierUsageMap(tokens, statements);

            for (auto index = (int32_t)statements.size() - 1; index >= 0; --index)
            {
                const auto& statement = statements[index];

                if (statement.isFunction && statement.functionName != nullptr)
                {
                    const auto& functionName = statement.functionName->str();
                    const auto totalRefs = GetIdentifierUsageCount(usageMap, functionName);
                    const auto selfRefs = CountIdentifierInRange(statement.openBrace, statement.end, functionName);

                    if (FindActiveEntryPoint(functionName, activeEntries) == nullptr && totalRefs <= selfRefs)
                    {
                        EraseTokenRange(tokens, statement.start, statement.end);
                        changed = true;
                    }
                    continue;
                }

                if (auto* propertyNameToken = FindPropertyDeclarationName(statement))
                {
                    if (GetIdentifierUsageCount(usageMap, propertyNameToken->str()) == 0u)
                    {
                        EraseTokenRange(tokens, statement.start, statement.end);
                        changed = true;
                    }
                    continue;
                }

                if (auto* variableNameToken = FindVariableDeclarationName(statement))
                {
                    if (GetIdentifierUsageCount(usageMap, variableNameToken->str()) == 0u)
                    {
                        EraseTokenRange(tokens, statement.start, statement.end);
                        changed = true;
                    }
                }
            }
        }
    }
}