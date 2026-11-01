#include <algorithm>
#include "PKStringUtilities.h"
#include "PKShaderUtilities.h"

namespace PKAssets::Shader
{
    struct TopLevelStatement
    {
        size_t start = 0ull;
        size_t end = 0ull;
        size_t openBrace = std::string::npos;
        size_t closeBrace = std::string::npos;
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

    static bool IsAlphaNumeric(char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
    }

    static size_t CountIdentifierOccurrences(const std::string& source, std::string_view identifier)
    {
        if (identifier.empty())
        {
            return 0ull;
        }

        auto count = 0ull;
        auto pos = 0ull;

        while ((pos = source.find(identifier.data(), pos, identifier.size())) != std::string::npos)
        {
            const auto validBefore = pos == 0ull || (!IsAlphaNumeric(source[pos - 1ull]) && source[pos - 1ull] != '.');
            const auto  end = pos + identifier.size();
            const auto validAfter = end >= source.size() || !IsAlphaNumeric(source[end]);

            if (validBefore && validAfter)
            {
                count++;
            }

            pos = end;
        }

        return count;
    }

    static std::vector<TopLevelStatement> ScanTopLevelStatements(const std::string& source)
    {
        std::vector<TopLevelStatement> statements;
        size_t i = 0ull;
        const auto len = source.size();
        auto stmtStart = 0ull;
        auto openBrace = std::string::npos;
        auto closeBrace = std::string::npos;
        auto braceDepth = 0;
        auto parenDepth = 0;
        auto isFunctionHeader = false;

        auto skipWhitespace = [&]()
        {
            while (i < len && (source[i] == ' ' || source[i] == '\t' || source[i] == '\r' || source[i] == '\n'))
            {
                ++i;
            }
        };

        skipWhitespace();
        stmtStart = i;

        while (i < len)
        {
            const auto c = source[i];

            if (braceDepth == 0 && parenDepth == 0 && c == '#')
            {
                while (i < len && source[i] != '\n')
                {
                    ++i;
                }

                skipWhitespace();
                stmtStart = i;
                continue;
            }

            if (c == '(')
            {
                parenDepth++;
            }
            else if (c == ')')
            {
                parenDepth = std::max(0, parenDepth - 1);
            }
            else if (c == '{')
            {
                if (braceDepth == 0)
                {
                    openBrace = i;
                    auto prev = i;
                    
                    while (prev > stmtStart && isspace((unsigned char)source[prev - 1ull]))
                    {
                        --prev;
                    }
                    
                    auto headerScan = prev;
                    
                    if (headerScan > stmtStart && source[headerScan - 1ull] != ')')
                    {
                        const auto colonPos = source.rfind(':', headerScan - 1ull);
                        
                        if (colonPos != std::string::npos && colonPos > stmtStart)
                        {
                            auto beforeColon = colonPos;
                            
                            while (beforeColon > stmtStart && isspace((unsigned char)source[beforeColon - 1ull]))
                            {
                                --beforeColon;
                            }
                            
                            headerScan = beforeColon;
                        }
                    }

                    isFunctionHeader = (headerScan > stmtStart && source[headerScan - 1ull] == ')');
                }

                braceDepth++;
            }
            else if (c == '}')
            {
                braceDepth = std::max(0, braceDepth - 1);

                if (braceDepth == 0)
                {
                    closeBrace = i;

                    if (isFunctionHeader)
                    {
                        statements.push_back({ stmtStart, i + 1ull, openBrace, closeBrace, true });
                        openBrace = closeBrace = std::string::npos;
                        isFunctionHeader = false;
                        ++i;
                        skipWhitespace();
                        stmtStart = i;
                        continue;
                    }
                }
            }
            else if (c == ';' && braceDepth == 0 && parenDepth == 0)
            {
                statements.push_back({ stmtStart, i + 1ull, openBrace, closeBrace, false });
                openBrace = closeBrace = std::string::npos;
                isFunctionHeader = false;
                ++i;
                skipWhitespace();
                stmtStart = i;
                continue;
            }

            ++i;
        }

        return statements;
    }

    static std::string StripQualifiersAndAttributes(std::string str)
    {
        size_t pos = 0ull;

        while (true)
        {
            size_t open, close;
            if (!StringUtilities::FindScope(str, pos, "layout (", ")", &open, &close) &&
                !StringUtilities::FindScope(str, pos, "layout(", ")", &open, &close))
            {
                break;
            }
            str.erase(open, (close + 1ull) - open);
            pos = open;
        }

        while (true)
        {
            const auto  firstNonSpace = str.find_first_not_of(" \t\r\n");
            
            if (firstNonSpace == std::string::npos || str[firstNonSpace] != '[')
            {
                break;
            }

            size_t open, close;

            if (!StringUtilities::FindScope(str, firstNonSpace, '[', ']', &open, &close))
            {
                break;
            }

            str.erase(open, (close + 1ull) - open);
        }

        return str;
    }

    static std::string ExtractLastIdentifierBefore(std::string_view text)
    {
        const size_t bracket = text.find('[');
        if (bracket != std::string_view::npos)
        {
            text = text.substr(0, bracket);
        }

        size_t end = text.size();
        while (end > 0ull && !IsAlphaNumeric(text[end - 1ull]))
        {
            --end;
        }

        size_t start = end;

        while (start > 0ull && IsAlphaNumeric(text[start - 1ull]))
        {
            --start;
        }

        return std::string(text.substr(start, end - start));
    }

    static std::string ExtractFunctionName(const std::string& source, const TopLevelStatement& stmt)
    {
        if (!stmt.isFunction || stmt.openBrace == std::string::npos)
        {
            return {};
        }

        auto scanEnd = stmt.openBrace;
        for (auto k = stmt.openBrace; k > stmt.start; --k)
        {
            const char c = source[k - 1ull];
            if (c == ')')
            {
                scanEnd = k;
                break;
            }
        }

        auto pDepth = 0;
        auto paramOpen = std::string::npos;

        for (auto k = scanEnd; k > stmt.start; --k)
        {
            if (source[k - 1ull] == ')')
            {
                pDepth++;
            }
            else if (source[k - 1ull] == '(')
            {
                if (--pDepth == 0)
                {
                    paramOpen = k - 1ull;
                    break;
                }
            }
        }

        if (paramOpen == std::string::npos)
        {
            return {};
        }

        auto nameEnd = paramOpen;
        
        while (nameEnd > stmt.start && !IsAlphaNumeric(source[nameEnd - 1ull]))
        {
            --nameEnd;
        }

        auto nameStart = nameEnd;

        while (nameStart > stmt.start && IsAlphaNumeric(source[nameStart - 1ull]))
        {
            --nameStart;
        }

        return source.substr(nameStart, nameEnd - nameStart);
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
        auto name = std::string(binding->name ? binding->name : "");

        if (name.empty() && binding->type_description && binding->type_description->type_name)
        {
            name = std::string(binding->type_description->type_name);
        }

        const auto aliasLength = strlen(PK_SHADER_ATTRIB_ALIAS);

        if (name.size() > aliasLength && strncmp(name.data() + name.size() - aliasLength, PK_SHADER_ATTRIB_ALIAS, aliasLength) == 0)
        {
            if (binding->type_description->member_count == 1)
            {
                return std::string(binding->type_description->members[0].struct_member_name);
            }
            else
            {
                return name.substr(0, name.size() - aliasLength);
            }
        }

        return name;
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
        auto value = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_LOGVERBOSE, source, true);
        *outValue = !value.empty();
    }

    void ExtractGenerateDebugInfo(std::string& source, bool* outValue)
    {
        auto value = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_GENERATEDEBUGINFO, source, true);
        *outValue = !value.empty();
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

    void SetActiveEntryPoints(std::string& source, const std::vector<EntryPointInfo>& entries, EntryPointInfo* const* activeEntries)
    {
        const auto statements = ScanTopLevelStatements(source);

        for (auto s = (int32_t)statements.size() - 1; s >= 0; --s)
        {
            const auto& stmt = statements[s];
            
            if (!stmt.isFunction)
            {
                continue;
            }

            const auto funcName = ExtractFunctionName(source, stmt);
            
            if (funcName.empty())
            {
                continue;
            }

            bool isDeclaredEntryPoint = false;

            for (const auto& entry : entries)
            {
                if (entry.name == funcName)
                {
                    isDeclaredEntryPoint = true;
                    break;
                }
            }

            if (!isDeclaredEntryPoint)
            {
                continue;
            }

            const EntryPointInfo* activeMatch = nullptr;
            
            for (auto stageIdx = 0u; stageIdx < (uint32_t)PKShaderStage::EnumCount; ++stageIdx)
            {
                if (activeEntries[stageIdx] && activeEntries[stageIdx]->name == funcName)
                {
                    activeMatch = activeEntries[stageIdx];
                    break;
                }
            }

            if (activeMatch == nullptr)
            {
                source.erase(stmt.start, stmt.end - stmt.start);
            }
            else
            {
                source.insert(stmt.start, GetSlangStageAttribute(activeMatch->stage));
            }
        }
    }

    static bool IsPushConstantPrimitiveType(std::string_view typeStr)
    {
        char buffer[64]{};
        
        if (typeStr.empty() || typeStr.size() >= sizeof(buffer))
        {
            return false;
        }
        
        memcpy(buffer, typeStr.data(), typeStr.size());

        const auto uniformType = StringToPKUniform(buffer);

        return uniformType != PKUniform::Invalid &&
            uniformType != PKUniform::Texture2D &&
            uniformType != PKUniform::Texture3D &&
            uniformType != PKUniform::TextureCube &&
            uniformType != PKUniform::Keyword;
    }

    static bool ExtractPrimitiveVariable(size_t& offset, std::string& source, uint32_t stageMask, SourcePushConstants& members)
    {
        auto typeStart = source.find_first_not_of(' ', offset);

        if (typeStart == std::string::npos)
        {
            return false;
        }

        auto typeEnd = source.find_first_of(' ', typeStart);

        if (typeEnd == std::string::npos)
        {
            return false;
        }

        auto nameStart = source.find_first_not_of(' ', typeEnd);

        if (nameStart == std::string::npos)
        {
            return false;
        }

        auto nameEnd = source.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_", nameStart);
        auto semiPos = source.find_first_not_of(" \t\r\n", nameEnd);

        if (semiPos == std::string::npos || source[semiPos] != ';')
        {
            offset = nameStart;
            return false;
        }

        const std::string_view typeView(source.data() + typeStart, typeEnd - typeStart);

        if (!IsPushConstantPrimitiveType(typeView))
        {
            offset = semiPos + 1u;
            return false;
        }

        offset = semiPos + 1u;

        auto type = std::string(typeView);
        auto name = source.substr(nameStart, nameEnd - nameStart);
        auto field = "    " + type + " " + name + ";\n";

        if (CountIdentifierOccurrences(source, name) <= 1ull)
        {
            return true;
        }

        for (auto& member : members)
        {
            if (member.name.compare(name) == 0)
            {
                if (member.field.compare(field) != 0)
                {
                    printf("Warning constant parameter '%s' was declared again with a different format '%s'", member.field.c_str(), field.c_str());
                }

                member.stageFlags |= stageMask;
                return true;
            }
        }

        members.push_back({ name, field, stageMask });
        return true;
    }

    static bool ExtractPushConstantBlock(size_t& offset, std::string& source, uint32_t stageMask, SourcePushConstants& members)
    {
        size_t blockStart, blockEnd;

        if (!StringUtilities::FindScope(source, offset, "{", "}", &blockStart, &blockEnd))
        {
            return false;
        }

        blockEnd = source.find_first_not_of(" \t\r\n", blockEnd + 1ull);

        if (blockEnd == std::string::npos || source[blockEnd] != ';')
        {
            return false;
        }

        auto currentPos = blockStart + 1ull;

        while (true)
        {
            currentPos = source.find_first_not_of(" \t\n\r", currentPos);

            if (currentPos == std::string::npos || currentPos >= blockEnd)
            {
                break;
            }

            auto memberEnd = currentPos;
            if (!ExtractPrimitiveVariable(memberEnd, source, stageMask, members))
            {
                break;
            }

            currentPos = memberEnd;
        }

        offset = blockEnd + 1ull;
        return true;
    }

    void ExtractPushConstants(std::string& source, uint32_t stageMask, SourcePushConstants& members)
    {
        auto currentpos = 0ull;

        while (true)
        {
            auto startSingle = source.find("uniform ", currentpos);
            auto startLayout = source.find("layout ( push_constant )", currentpos);

            if (startSingle == std::string::npos && startLayout == std::string::npos)
            {
                break;
            }

            if (startSingle < startLayout)
            {
                auto endSingle = startSingle + 8ull;
                if (ExtractPrimitiveVariable(endSingle, source, stageMask, members))
                {
                    source.erase(startSingle, endSingle - startSingle);
                    continue;
                }

                currentpos = endSingle;
            }
            else
            {
                auto endLayout = startLayout + 24ull;
                if (ExtractPushConstantBlock(endLayout, source, stageMask, members))
                {
                    source.erase(startLayout, endLayout - startLayout);
                    continue;
                }

                currentpos = endLayout;
            }
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

    void RemoveUnusedResourcesAndFunctions(std::string& source, EntryPointInfo* const* activeEntries)
    {
        auto isActiveEntry = [&](const std::string& name)
        {
            for (auto i = 0u; i < (uint32_t)PKShaderStage::EnumCount; ++i)
            {
                if (activeEntries[i] && activeEntries[i]->name == name)
                {
                    return true;
                }
            }
            return false;
        };

        bool changed = true;
        while (changed)
        {
            changed = false;
            const auto statements = ScanTopLevelStatements(source);

            for (auto s = (int32_t)statements.size() - 1; s >= 0; --s)
            {
                const auto& stmt = statements[s];

                if (stmt.isFunction)
                {
                    const auto funcName = ExtractFunctionName(source, stmt);
                    if (!funcName.empty() && !isActiveEntry(funcName) && CountIdentifierOccurrences(source, funcName) <= 1ull)
                    {
                        source.erase(stmt.start, stmt.end - stmt.start);
                        changed = true;
                    }
                    continue;
                }

                // Block declaration: uniform / buffer / cbuffer { ... } [instanceName];
                if (stmt.openBrace != std::string::npos && stmt.closeBrace != std::string::npos)
                {
                    const auto header = StripQualifiersAndAttributes(source.substr(stmt.start, stmt.openBrace - stmt.start));
                    const auto tokens = StringUtilities::Split(header, " \t\r\n");

                    bool isResourceBlock = false;
                    for (const auto& t : tokens)
                    {
                        if (t == "uniform" || t == "buffer" || t == "cbuffer")
                        {
                            isResourceBlock = true;
                            break;
                        }
                    }

                    if (!isResourceBlock)
                    {
                        continue;
                    }

                    const auto instanceName = ExtractLastIdentifierBefore(std::string_view(source.data() + stmt.closeBrace + 1ull, (stmt.end - 1ull) - (stmt.closeBrace + 1ull)));

                    if (!instanceName.empty())
                    {
                        if (CountIdentifierOccurrences(source, instanceName) <= 1ull)
                        {
                            source.erase(stmt.start, stmt.end - stmt.start);
                            changed = true;
                        }
                    }
                    else
                    {
                        const auto body = source.substr(stmt.openBrace + 1ull, stmt.closeBrace - (stmt.openBrace + 1ull));
                        const auto memberDecls = StringUtilities::Split(body, ";");
                        bool anyMemberUsed = false;

                        for (const auto& memberDecl : memberDecls)
                        {
                            const auto memberName = ExtractLastIdentifierBefore(memberDecl);
                            if (!memberName.empty() && CountIdentifierOccurrences(source, memberName) > 1ull)
                            {
                                anyMemberUsed = true;
                                break;
                            }
                        }

                        if (!anyMemberUsed)
                        {
                            source.erase(stmt.start, stmt.end - stmt.start);
                            changed = true;
                        }
                    }
                    continue;
                }

                const auto decl = StripQualifiersAndAttributes(source.substr(stmt.start, (stmt.end - 1ull) - stmt.start));
                const auto declNoArray = decl.substr(0ull, decl.find('['));

                if (declNoArray.find('(') != std::string::npos || decl.find('=') != std::string::npos)
                {
                    continue;
                }

                const auto tokens = StringUtilities::Split(declNoArray, " \t\r\n");
                if (tokens.size() >= 2ull &&
                    (tokens[0] == "uniform" ||
                        tokens[0] == "shared" ||
                        tokens[0] == "groupshared" ||
                        tokens[0] == "static"))
                {
                    const auto varName = ExtractLastIdentifierBefore(declNoArray);
                    if (!varName.empty() && CountIdentifierOccurrences(source, varName) <= 1ull)
                    {
                        source.erase(stmt.start, stmt.end - stmt.start);
                        changed = true;
                    }
                }
            }
        }
    }
}
