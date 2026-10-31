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

    constexpr const static char* PK_HLSL_PRIMITIVE_TYPES[] =
    {
         "bool2",
         "bool3",
         "bool4",
         "half", 
         "half2",
         "half3",
         "half4",
         "float2", 
         "float3", 
         "float4", 
         "double", 
         "double2",
         "double3",
         "double4",
         "short",
         "short2", 
         "short3", 
         "short4", 
         "ushort", 
         "ushort2",
         "ushort3",
         "ushort4",
         "byte", 
         "byte2",
         "byte3",
         "byte4",
         "sbyte",
         "sbyte2", 
         "sbyte3", 
         "sbyte4", 
         "int2", 
         "int3", 
         "int4", 
         "uint2",
         "uint3",
         "uint4",
         "long", 
         "long2",
         "long3",
         "long4",
         "ulong",
         "ulong2", 
         "ulong3", 
         "ulong4", 
         "half2x2",
         "half2x3",
         "half2x4",
         "half3x2",
         "half3x3",
         "half3x4",
         "half4x2",
         "half4x3",
         "half4x4",
         "float2x2", 
         "float2x3", 
         "float2x4", 
         "float3x2", 
         "float3x3", 
         "float3x4", 
         "float4x2", 
         "float4x3", 
         "float4x4", 
         "double2x2",
         "double2x3",
         "double2x4",
         "double3x2",
         "double3x3",
         "double3x4",
         "double4x2",
         "double4x3",
         "double4x4"
    };

    constexpr const static char* PK_GLSL_PRIMITIVE_TYPES[] =
    {
        "bvec2",
        "bvec3",
        "bvec4",
        "float16_t",
        "f16vec2",
        "f16vec3",
        "f16vec4",
        "vec2",
        "vec3",
        "vec4",
        "float64_t",
        "f64vec2",
        "f64vec3",
        "f64vec4",
        "int16_t",
        "i16vec2",
        "i16vec3",
        "i16vec4",
        "uint16_t",
        "u16vec2",
        "u16vec3",
        "u16vec4",
        "uint8_t",
        "u8vec2",
        "u8vec3",
        "u8vec4",
        "int8_t",
        "i8vec2",
        "i8vec3",
        "i8vec4",
        "ivec2",
        "ivec3",
        "ivec4",
        "uvec2",
        "uvec3",
        "uvec4",
        "int64_t",
        "i64vec2",
        "i64vec3",
        "i64vec4",
        "uint64_t",
        "u64vec2",
        "u64vec3",
        "u64vec4",
        "f16mat2",
        "f16mat2x3",
        "f16mat2x4",
        "f16mat3x2",
        "f16mat3",
        "f16mat3x4",
        "f16mat4x2",
        "f16mat4x3",
        "f16mat4",
        "mat2",
        "mat2x3",
        "mat2x4",
        "mat3x2",
        "mat3",
        "mat3x4",
        "mat4x2",
        "mat4x3",
        "mat4",
        "f64mat2",
        "f64mat2x3",
        "f64mat2x4",
        "f64mat3x2",
        "f64mat3",
        "f64mat3x4",
        "f64mat4x2",
        "f64mat4x3",
        "f64mat4"
    };

    // Order matters for search
    constexpr const static char* PK_HLSL_TEXTURE_POSTFIXES[] =
    {
        "1DArray",
        "1D",
        "2DMSArray",
        "2DArray",
        "2DMS",
        "2D",
        "3D",
        "CubeArray",
        "Cube",
    };

    constexpr static uint32_t PK_SHADER_PRIMITIVE_TYPE_COUNT = std::size(PK_HLSL_PRIMITIVE_TYPES);
    constexpr static uint32_t PK_HLSL_TEXTURE_POSTFIX_COUNT = std::size(PK_HLSL_TEXTURE_POSTFIXES);

    const static char* PKElementType_NAMES[] =
    {
        "Invalid",
        "float",
        "float2",
        "float3",
        "float4",
        "double",
        "double2",
        "double3",
        "double4",
        "half",
        "half2",
        "half3",
        "half4",
        "int",
        "int2",
        "int3",
        "int4",
        "uint",
        "uint2",
        "uint3",
        "uint4",
        "short",
        "short2",
        "short3",
        "short4",
        "ushort",
        "ushort2",
        "ushort3",
        "ushort4",
        "long",
        "long2",
        "long3",
        "long4",
        "ulong",
        "ulong2",
        "ulong3",
        "ulong4",
        "float2x2",
        "float3x3",
        "float4x4",
        "float3x4",
        "double2x2",
        "double3x3",
        "double4x4",
        "half2x2",
        "half3x3",
        "half4x4",
        "texture2D",
        "texture3D",
        "textureCube",
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
            if (strncmp(arr[i], str, length) == 0)
            {
                return i;
            }
        }

        return fallback;
    }

    #define DECLARE_STRING_TO_ENUM(TType, TFallback) TType StringTo##TType(const char* str) { return (TType)FindEnumIndexFromString(TType##_NAMES, str, (uint32_t)TFallback); }

    DECLARE_STRING_TO_ENUM(PKElementType, PKElementType::Invalid)
    DECLARE_STRING_TO_ENUM(PKShaderStage, PKShaderStage::EnumCount)
    DECLARE_STRING_TO_ENUM(PKComparison, PKComparison::Off)
    DECLARE_STRING_TO_ENUM(PKCullMode, PKCullMode::Off)
    DECLARE_STRING_TO_ENUM(PKBlendFactor, PKBlendFactor::None)
    DECLARE_STRING_TO_ENUM(PKBlendOp, PKBlendOp::None)
    DECLARE_STRING_TO_ENUM(PKRasterMode, PKRasterMode::Default)

    #undef DECLARE_STRING_TO_ENUM

    static bool IsIdentifierChar(char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
    }
    
    static size_t CountIdentifierOccurrences(const std::string& source, std::string_view identifier)
    {
        if (identifier.empty())
        {
            return 0ull;
        }

        size_t count = 0ull;
        size_t pos = 0ull;

        while ((pos = source.find(identifier.data(), pos, identifier.size())) != std::string::npos)
        {
            const bool validBefore = (pos == 0ull) || (!IsIdentifierChar(source[pos - 1ull]) && source[pos - 1ull] != '.');
            const size_t end = pos + identifier.size();
            const bool validAfter = (end >= source.size()) || !IsIdentifierChar(source[end]);

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
        const size_t len = source.size();
        size_t stmtStart = 0ull;
        size_t openBrace = std::string::npos;
        size_t closeBrace = std::string::npos;
        int32_t braceDepth = 0;
        int32_t parenDepth = 0;
        bool isFunctionHeader = false;

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
            const char c = source[i];

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
                    size_t prev = i;
                    while (prev > stmtStart && isspace((unsigned char)source[prev - 1ull]))
                    {
                        --prev;
                    }
                    isFunctionHeader = (prev > stmtStart && source[prev - 1ull] == ')');
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

    static std::string StripLayoutQualifiers(std::string str)
    {
        size_t pos = 0ull;
        while (true)
        {
            size_t open, close;
            if (!StringUtilities::FindScope(str, pos, "layout(", ")", &open, &close))
            {
                break;
            }
            str.erase(open, (close + 1ull) - open);
            pos = open;
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
        while (end > 0ull && !IsIdentifierChar(text[end - 1ull]))
        {
            --end;
        }

        size_t start = end;
        while (start > 0ull && IsIdentifierChar(text[start - 1ull]))
        {
            --start;
        }

        return std::string(text.substr(start, end - start));
    }

    PKColorMask StringToPKColorMask(const char* str)
    {
        auto length = str ? strnlen(str, 4) : 0;

        if (length == 0)
        {
            return PKColorMask::RGBA;
        }

        uint8_t mask = (uint8_t)PKColorMask::NONE;

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

    PKElementType GetElementType(SpvReflectFormat format)
    {
        switch (format)
        {
            case SPV_REFLECT_FORMAT_UNDEFINED: return PKElementType::Invalid;
            case SPV_REFLECT_FORMAT_R16_UINT: return PKElementType::Ushort;
            case SPV_REFLECT_FORMAT_R16_SINT: return PKElementType::Short;
            case SPV_REFLECT_FORMAT_R16_SFLOAT: return PKElementType::Half;
            case SPV_REFLECT_FORMAT_R16G16_UINT: return PKElementType::Ushort2;
            case SPV_REFLECT_FORMAT_R16G16_SINT: return PKElementType::Short2;
            case SPV_REFLECT_FORMAT_R16G16_SFLOAT: return PKElementType::Half2;
            case SPV_REFLECT_FORMAT_R16G16B16_UINT: return PKElementType::Ushort3;
            case SPV_REFLECT_FORMAT_R16G16B16_SINT: return PKElementType::Short3;
            case SPV_REFLECT_FORMAT_R16G16B16_SFLOAT: return PKElementType::Half3;
            case SPV_REFLECT_FORMAT_R16G16B16A16_UINT: return PKElementType::Ushort4;
            case SPV_REFLECT_FORMAT_R16G16B16A16_SINT: return PKElementType::Short4;
            case SPV_REFLECT_FORMAT_R16G16B16A16_SFLOAT: return PKElementType::Half4;
            case SPV_REFLECT_FORMAT_R32_UINT: return PKElementType::Uint;
            case SPV_REFLECT_FORMAT_R32_SINT: return PKElementType::Int;
            case SPV_REFLECT_FORMAT_R32_SFLOAT: return PKElementType::Float;
            case SPV_REFLECT_FORMAT_R32G32_UINT: return PKElementType::Uint2;
            case SPV_REFLECT_FORMAT_R32G32_SINT: return PKElementType::Int2;
            case SPV_REFLECT_FORMAT_R32G32_SFLOAT: return PKElementType::Float2;
            case SPV_REFLECT_FORMAT_R32G32B32_UINT: return PKElementType::Uint3;
            case SPV_REFLECT_FORMAT_R32G32B32_SINT: return PKElementType::Int3;
            case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT: return PKElementType::Float3;
            case SPV_REFLECT_FORMAT_R32G32B32A32_UINT: return PKElementType::Uint4;
            case SPV_REFLECT_FORMAT_R32G32B32A32_SINT: return PKElementType::Int4;
            case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT: return PKElementType::Float4;
            case SPV_REFLECT_FORMAT_R64_UINT: return PKElementType::Ulong;
            case SPV_REFLECT_FORMAT_R64_SINT: return PKElementType::Long;
            case SPV_REFLECT_FORMAT_R64_SFLOAT: return PKElementType::Double;
            case SPV_REFLECT_FORMAT_R64G64_UINT: return PKElementType::Ulong2;
            case SPV_REFLECT_FORMAT_R64G64_SINT: return PKElementType::Long2;
            case SPV_REFLECT_FORMAT_R64G64_SFLOAT: return PKElementType::Double2;
            case SPV_REFLECT_FORMAT_R64G64B64_UINT: return PKElementType::Ulong3;
            case SPV_REFLECT_FORMAT_R64G64B64_SINT: return PKElementType::Long3;
            case SPV_REFLECT_FORMAT_R64G64B64_SFLOAT: return PKElementType::Double3;
            case SPV_REFLECT_FORMAT_R64G64B64A64_UINT: return PKElementType::Ulong4;
            case SPV_REFLECT_FORMAT_R64G64B64A64_SINT: return PKElementType::Long4;
            case SPV_REFLECT_FORMAT_R64G64B64A64_SFLOAT: return PKElementType::Double4;
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
            case PKElementType::Ushort: return "uint16_t";
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
            case PKElementType::Float3x4: return "mat3x4";
            case PKElementType::Double2x2: return "f64mat2";
            case PKElementType::Double3x3: return "f64mat3";
            case PKElementType::Double4x4: return "f64mat4";
            case PKElementType::Half2x2: return "f16mat2";
            case PKElementType::Half3x3: return "f16mat3";
            case PKElementType::Half4x4: return "f16mat4";
            case PKElementType::Texture2D: return "uint";
            case PKElementType::Texture3D: return "uint";
            case PKElementType::TextureCube: return "uint";
            case PKElementType::Keyword: return "INVALID";
            case PKElementType::Invalid: return "INVALID";
        }
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

    shaderc_shader_kind ConvertToShadercKind(PKShaderStage stage)
    {
        switch (stage)
        {
            case PKShaderStage::Vertex: return shaderc_vertex_shader;
            case PKShaderStage::TesselationControl: return shaderc_tess_control_shader;
            case PKShaderStage::TesselationEvaluation: return shaderc_tess_evaluation_shader;
            case PKShaderStage::Geometry: return shaderc_geometry_shader;
            case PKShaderStage::MeshTask: return shaderc_task_shader;
            case PKShaderStage::MeshAssembly: return shaderc_mesh_shader;
            case PKShaderStage::Fragment: return shaderc_fragment_shader;
            case PKShaderStage::Compute: return shaderc_compute_shader;

            case PKShaderStage::RayGeneration: return shaderc_raygen_shader;
            case PKShaderStage::RayMiss: return shaderc_miss_shader;
            case PKShaderStage::RayClosestHit: return shaderc_closesthit_shader;
            case PKShaderStage::RayAnyHit: return shaderc_anyhit_shader;
            case PKShaderStage::RayIntersection: return shaderc_intersection_shader;

            case PKShaderStage::EnumCount: return shaderc_shader_kind::shaderc_glsl_infer_from_source;
        }
    }

    std::string ReflectBindingName(SpvReflectDescriptorBinding* binding)
    {
        auto name = std::string(binding->name);

        if (name.empty())
        {
            name = std::string(binding->type_description->type_name);
        }

        // if the variable declared an alias. reflect the first member name from the layout.
        const auto aliasLength = strlen(PK_SHADER_ATTRIB_ALIAS);
        if (name.size() > aliasLength && strncmp(name.data() + name.size() - aliasLength, PK_SHADER_ATTRIB_ALIAS, aliasLength) == 0)
        {
            if (binding->type_description->member_count == 1)
            {
                return std::string(binding->type_description->members[0].struct_member_name);
            }
            else
            {
                // Remove erroneus alias from name if type has other than a single member.
                return name.substr(0, name.size() - aliasLength);
            }
        }

        return name;
    }

    std::string FormatErrorMessage(const std::string& name, const std::string& source, const std::string& error)
    {
        std::string output;
        auto lineToken = name + std::string(":");
        output.reserve(error.size());
        
        auto pos = 0ull;

        while (pos < error.size())
        {
            auto eol = error.find_first_of("\r\n", pos);
            eol = pos == std::string::npos ? error.size() - 1ull : eol;

            auto linepos = error.find(lineToken, pos);
            auto numbeg = linepos + lineToken.size();

            output.append(error.substr(pos, (eol + 1ull) - pos));

            if (linepos < eol && isdigit(error[numbeg]))
            {
                auto numend = error.find(':', numbeg);

                if (numend < eol)
                {
                    auto number = std::stoi(error.substr(numbeg, numend - numbeg));
                    output.append(StringUtilities::GetLineAtIndex(source, number));
                }
            }

            pos = eol + 1ull;
        }

        return output;
    }

    uint32_t CountBits(uint32_t bits)
    {
        bits = bits - ((bits >> 1) & 0x55555555);
        bits = (bits & 0x33333333) + ((bits >> 2) & 0x33333333);
        bits = (bits + (bits >> 4)) & 0x0F0F0F0F;
        bits *= 0x01010101;
        bits = bits >> 24;
        return bits;
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

    void InsertRequiredExtensions(std::string& source, PKShaderStage stage)
    {
        source.insert(0, PK_GL_EXTENSIONS_COMMON);

        if (stage == PKShaderStage::MeshTask || stage == PKShaderStage::MeshAssembly)
        {
            source.insert(0, PK_GL_EXTENSIONS_MESHSHADING);
        }

        if (stage == PKShaderStage::RayGeneration ||
            stage == PKShaderStage::RayMiss ||
            stage == PKShaderStage::RayClosestHit ||
            stage == PKShaderStage::RayAnyHit ||
            stage == PKShaderStage::RayIntersection)
        {
            source.insert(0, PK_GL_EXTENSIONS_RAYTRACING);
        }
    }

    int RemoveEntryPointLocals(std::string& source, const std::string& entryPointName, PKShaderStage stage)
    {
        auto currentpos = 0ull;

        const auto lengthOpen = strlen(PK_SHADER_ATTRIB_LOCAL_OPEN);
        const auto lengthClose = strlen(PK_SHADER_ATTRIB_LOCAL_CLOSE);

        while (true)
        {
            size_t posopen, posclose;

            if (!StringUtilities::FindScope(source, currentpos, PK_SHADER_ATTRIB_LOCAL_OPEN, PK_SHADER_ATTRIB_LOCAL_CLOSE, &posopen, &posclose))
            {
                return 0;
            }

            currentpos = posopen;
            auto content = source.substr(posopen + lengthOpen, posclose - (posopen + lengthOpen));
            auto arguments = StringUtilities::Split(content, ", ");
            auto foundEntry = false;

            for (auto& arg : arguments)
            {
                if (arg.compare(entryPointName) == 0 || arg.compare(PKShaderStage_NAMES[(uint32_t)stage]) == 0)
                {
                    foundEntry = true;
                    break;
                }
            }

            if (foundEntry)
            {
                source.erase(posopen, (posclose + lengthClose) - posopen);
                continue;
            }

            posclose = source.find(';', posclose);
            size_t controlopen = 0ull;
            size_t controlclose = 0ull;

            if (StringUtilities::FindScope(source, currentpos, '{', '}', &controlopen, &controlclose) && controlopen < posclose)
            {
                posclose = controlclose;

                if (source[posclose + 1u] == ';')
                {
                    posclose++;
                }
            }

            if (posclose == std::string::npos)
            {
                printf("Couldnt find a valid control flow scope after [pk_local] attribute.\n");
                return -1;
            }

            source.erase(posopen, (posclose + 1u) - posopen);
        }
    }

    void RemoveInactiveGroupSizeLayouts(std::string& source, PKShaderStage stage)
    {
        if (stage == PKShaderStage::Compute)
        {
            std::vector<size_t> positions;

            auto currentpos = 0ull;

            while (true)
            {
                size_t posopen, posclose;

                if (!StringUtilities::FindScope(source, currentpos, "layout(", ")", &posopen, &posclose))
                {
                    break;
                }

                auto localsizepos = source.find("local_size_x", posopen);
                if (posopen < localsizepos && localsizepos < posclose)
                {
                    positions.push_back(posopen);
                }

                currentpos = posclose;
            }

            auto mainpos = source.find("void main()");
            auto selected = false;

            for (int32_t i = positions.size() - 1; i >= 0; --i)
            {
                if (positions.at(i) < mainpos && !selected)
                {
                    selected = true;
                    continue;
                }

                auto posopen = positions.at(i);
                auto posclose = source.find(';', posopen);
                source.erase(posopen, (posclose + 1u) - posopen);
            }
        }
    }

    void ProcessShaderVersion(std::string& source)
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

    void RemoveDescriptorSets(std::string& source)
    {
        auto currentpos = 0ull;

        while (true)
        {
            size_t open, close;
            if (!StringUtilities::FindScope(source, currentpos, "layout(", ")", &open, &close))
            {
                break;
            }

            constexpr auto lengthopen = 7u;
            constexpr auto lengthclose = 1u;
            auto tokens = StringUtilities::SplitNoWhiteSpace(source.substr(open + lengthopen, close - (open + lengthopen)), ",");

            auto isSet = false;

            for (auto& token : tokens)
            {
                isSet |= strncmp(token.data(), "set=", 4) == 0;
            }

            if (!isSet)
            {
                currentpos = close;
                continue;
            }

            if (tokens.size() == 1ull)
            {
                source.erase(open, (close + lengthclose) - open);
                continue;
            }

            std::string layout = "layout(";

            for (auto i = 0u; i < tokens.size(); ++i)
            {
                if (strncmp(tokens[i].data(), "set=", 4) != 0)
                {
                    layout.append(tokens[i]);

                    if (i < tokens.size() - 2u)
                    {
                        layout.append(",");
                    }
                }
            }

            layout.append(")");
            source.replace(open, (close - open) + 1u, layout);
            currentpos = open + layout.size();
        }
    }

    void RemoveUnsupportedRayTracingFields(std::string& source, PKShaderStage stage)
    {
        auto isRayTracingSupported = false;
        // Wont detect if these are set to enabled as I'm too lazy to pattern match that here.
        // Will fail compiling anyway if not enabled.
        isRayTracingSupported |= source.find("GL_EXT_ray_tracing") != std::string::npos;
        isRayTracingSupported |= source.find("GL_EXT_ray_query") != std::string::npos;

        auto isRayTracingHitStage = (((uint32_t)PKShaderStageFlags::RayTraceGroupHit | (uint32_t)PKShaderStageFlags::RayTraceGroupMiss) & (1u << (uint32_t)stage)) != 0u;

        auto currentpos = 0ull;

        while (!isRayTracingSupported)
        {
            auto uniformPos = source.find("uniform ", currentpos);

            if (uniformPos == std::string::npos)
            {
                break;
            }

            auto structurePos = source.find_first_not_of(' ', uniformPos + 8u);

            if (structurePos == std::string::npos || strncmp(source.data() + structurePos, "accelerationStructureEXT ", 24u) != 0)
            {
                currentpos = uniformPos + 8u;
                continue;
            }

            auto endofField = source.find_first_of(';', structurePos);

            if (endofField == std::string::npos)
            {
                break;
            }

            source.erase(uniformPos, endofField + 1u - uniformPos);
        }

        currentpos = 0ull;

        while (!isRayTracingHitStage || !isRayTracingSupported)
        {
            auto attribPos = source.find("hitAttributeEXT ", currentpos);

            if (attribPos == std::string::npos)
            {
                break;
            }

            auto endofField = source.find_first_of(';', attribPos);

            if (endofField == std::string::npos)
            {
                break;
            }

            source.erase(attribPos, endofField + 1u - attribPos);
        }

        while (isRayTracingHitStage)
        {
            auto attribPos = source.find("rayPayloadEXT", currentpos);

            if (attribPos == std::string::npos)
            {
                break;
            }

            currentpos = attribPos + 15u;
            source.replace(attribPos, 13u, "rayPayloadInEXT");
        }
    }

    void ConvertPrintf(std::string& source)
    {
        auto insertExtension = false;
        auto currentpos = 0ull;

        while (currentpos != std::string::npos)
        {
            currentpos = source.find("printf(", currentpos);

            if (currentpos != std::string::npos)
            {
                insertExtension = true;
                source.replace(currentpos, 7ull, "debugPrintfEXT(");
                currentpos += 15ull;
            }
        }

        if (insertExtension)
        {
            source.insert(0u, PK_GL_EXTENSION_PRINTF);
        }
    }

    void ConvertPKNumThreads(std::string& source)
    {
        auto currentpos = 0ull;

        const auto lengthopen = strlen(PK_SHADER_ATTRIB_NUMTHREADS_OPEN);
        const auto lengthclose = strlen(PK_SHADER_ATTRIB_NUMTHREADS_CLOSE);

        while (true)
        {
            size_t open, close;

            if (!StringUtilities::FindScope(source, currentpos, PK_SHADER_ATTRIB_NUMTHREADS_OPEN, PK_SHADER_ATTRIB_NUMTHREADS_CLOSE, &open, &close))
            {
                break;
            }

            std::string layout = "layout(";
            auto values = StringUtilities::Split(source.substr(open + lengthopen, close - (open + lengthopen)), ", ");
            const char* dimensionNames[3] = { "local_size_x=", "local_size_y=", "local_size_z=" };

            for (auto i = 0u; i < values.size(); ++i)
            {
                layout.append(dimensionNames[i]);
                layout.append(values[i]);

                if (i != values.size() - 1ull)
                {
                    layout.append(",");
                }
            }

            layout.append(") in;");

            source.erase(open, (close + lengthclose) - open);
            source.insert(open, layout);
            currentpos = open + layout.size();
        }
    }

    void ConvertHLSLBuffers(std::string& source)
    {
        auto currentpos = 0ull;

        while (true)
        {
            size_t open, close;

            if (!StringUtilities::FindScope(source, currentpos, "StructuredBuffer <", ">", &open, &close))
            {
                break;
            }

            constexpr auto lengthopen = 18u;
            constexpr auto lengthclose = 1u;
            auto type = StringUtilities::Trim(source.substr(open + lengthopen, close - (open + lengthopen)));

            if (type.empty())
            {
                currentpos = close;
                continue;
            }

            auto nameStart = source.find_first_not_of(' ', close + lengthclose);
            auto nameEnd = source.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_", nameStart);

            if (nameStart == std::string::npos || nameEnd == std::string::npos)
            {
                currentpos = close;
                continue;
            }

            auto name = source.substr(nameStart, nameEnd - nameStart);
            auto hasWritableTag = open >= 2ull && strncmp("RW", source.data() + open - 2ull, 2ull) == 0;
            auto accessToken = hasWritableTag ? std::string() : " readonly";

            if (hasWritableTag)
            {
                open -= 2ull;
            }

            auto uniformTagPos = source.rfind("uniform", open);

            if (uniformTagPos != std::string::npos && source.find_first_not_of(" ", uniformTagPos + 7u) == open)
            {
                open = uniformTagPos;
            }

            source.replace(open, nameEnd - open, "layout(scalar)" + accessToken + " buffer " + name + "_pkalias{" + type + " " + name + "[];}");
        }
    }

    void ConvertHLSLTextures(std::string& source)
    {
        auto currentpos = 0ull;

        while (true)
        {
            const auto textureLength = 7ull;
            auto open = source.find("Texture", currentpos);
            auto close = open + textureLength;

            if (open == std::string::npos)
            {
                break;
            }

            if (!isalnum(source[close]))
            {
                currentpos = close;
                continue;
            }

            auto hasWritableTag = strncmp("RW", source.data() + open - 2ull, 2ull) == 0;
            auto type = std::string(hasWritableTag ? "image" : "texture");

            if (hasWritableTag)
            {
                open -= 2ull;
            }

            auto postfix = 0u;

            for (auto i = 0u; i < PK_HLSL_TEXTURE_POSTFIX_COUNT; ++i)
            {
                const auto length = strlen(PK_HLSL_TEXTURE_POSTFIXES[i]);

                if (strncmp(source.c_str() + close, PK_HLSL_TEXTURE_POSTFIXES[i], length) == 0)
                {
                    type.append(PK_HLSL_TEXTURE_POSTFIXES[i]);
                    postfix = length;
                    break;
                }
            }

            if (!postfix)
            {
                currentpos = close;
                continue;
            }

            close += postfix;

            if (!isspace(source[close]))
            {
                currentpos = close;
                continue;
            }

            size_t formatOpen, formatClose;

            if (StringUtilities::FindScope(source, close, "<", ">", &formatOpen, &formatClose) && formatOpen == close + 1ull)
            {
                // Uint types begin with u. This is after glsl type conversion.
                if (source[formatOpen + 2u] == 'u')
                {
                    type.insert(0, "u");
                }

                // Int types begin with i. This is after glsl type conversion.
                if (source[formatOpen + 2u] == 'i')
                {
                    type.insert(0, "i");
                }

                close = formatClose + 1u;
            }

            source.replace(open, close - open, type);
            currentpos = open;
        }
    }

    void ConvertHLSLCBuffers(std::string& source)
    {
        auto currentpos = 0ull;

        while (currentpos != std::string::npos)
        {
            auto uniformPos = source.find("uniform ", currentpos);

            if (uniformPos == std::string::npos)
            {
                break;
            }

            auto cbufferPos = source.find_first_not_of(' ', uniformPos + 8u);

            if (cbufferPos == std::string::npos || strncmp(source.data() + cbufferPos, "cbuffer ", 8u) != 0)
            {
                currentpos = uniformPos + 8u;
                continue;
            }

            source.replace(uniformPos, cbufferPos + 7u - uniformPos, "layout(scalar) uniform ");
            currentpos = uniformPos + 23u;
        }
    }

    void ConvertHLSLTypesToGLSL(std::string& source)
    {
        const std::string surroundMask = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.";
        StringUtilities::ReplaceAll(source, surroundMask, "lerp", "mix");
        StringUtilities::ReplaceAll(source, surroundMask, "frac", "fract");
        StringUtilities::ReplaceAll(source, surroundMask, "rsqrt", "inversesqrt");
        StringUtilities::ReplaceAll(source, surroundMask, "asuint", "floatBitsToUint");
        StringUtilities::ReplaceAll(source, surroundMask, "asfloat", "uintBitsToFloat");
        StringUtilities::ReplaceAll(source, surroundMask, "atan2", "atan");
        StringUtilities::ReplaceAll(source, surroundMask, "ddx_fine", "dFdxFine");
        StringUtilities::ReplaceAll(source, surroundMask, "ddy_fine", "dFdyFine");
        StringUtilities::ReplaceAll(source, surroundMask, "ddx_coarse", "dFdxCoarse");
        StringUtilities::ReplaceAll(source, surroundMask, "ddy_coarse", "dFdyCoarse");

        StringUtilities::ReplaceAll(source, surroundMask, "GroupMemoryBarrierWithGroupSync", "barrier");
        StringUtilities::ReplaceAll(source, surroundMask, "WaveIsFirstLane", "subgroupElect");
        StringUtilities::ReplaceAll(source, surroundMask, "WaveActiveAllTrue", "subgroupAll");
        StringUtilities::ReplaceAll(source, surroundMask, "WaveActiveAnyTrue", "subgroupAny");
        StringUtilities::ReplaceAll(source, surroundMask, "WaveReadLaneAt", "subgroupShuffle");
        StringUtilities::ReplaceAll(source, surroundMask, "WaveReadFirstLane", "subgroupBroadcastFirst");
        StringUtilities::ReplaceAll(source, surroundMask, "WaveActiveSum", "subgroupAdd");
        StringUtilities::ReplaceAll(source, surroundMask, "WaveActiveMax", "subgroupMax");
        StringUtilities::ReplaceAll(source, surroundMask, "WaveActiveBallot", "subgroupBallot");
        StringUtilities::ReplaceAll(source, surroundMask, "QuadReadAcrossX", "subgroupQuadSwapHorizontal");
        StringUtilities::ReplaceAll(source, surroundMask, "QuadReadAcrossY", "subgroupQuadSwapVertical");
        StringUtilities::ReplaceAll(source, surroundMask, "QuadReadAcrossDiagonal", "subgroupQuadSwapDiagonal");
        StringUtilities::ReplaceAll(source, surroundMask, "QuadAll", "subgroupQuadAll");
        StringUtilities::ReplaceAll(source, surroundMask, "QuadAny", "subgroupQuadAny");
        StringUtilities::ReplaceAll(source, surroundMask, "countbits", "bitCount");

        for (auto i = 0u; i < PK_SHADER_PRIMITIVE_TYPE_COUNT; ++i)
        {
            StringUtilities::ReplaceAll(source, surroundMask, PK_HLSL_PRIMITIVE_TYPES[i], PK_GLSL_PRIMITIVE_TYPES[i]);
        }
    }


    static bool ExtractPrimitiveVariable(size_t& offset, std::string& source, PKShaderStage stage, SourcePushConstants& members)
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

        if (nameEnd == std::string::npos || source[nameEnd] != ';')
        {
            offset = nameStart;
            return false;
        }

        auto isValidType = false;

        for (auto i = 0u; i < PK_SHADER_PRIMITIVE_TYPE_COUNT; ++i)
        {
            if (strncmp(PK_GLSL_PRIMITIVE_TYPES[i], source.data() + typeStart, typeEnd - typeStart) == 0)
            {
                isValidType = true;
                break;
            }
        }

        if (!isValidType)
        {
            offset = nameEnd + 1u;
            return false;
        }

        offset = nameEnd + 1u;
        
        auto type = source.substr(typeStart, typeEnd - typeStart);
        auto name = source.substr(nameStart, nameEnd - nameStart);
        auto field = type + " " + name + ";\n";

        if (CountIdentifierOccurrences(source, name) <= 1ull)
        {
            return true;
        }

        // slow find member. I don't want to implement an insertion ordered map here so whatever.
        for (auto& member : members)
        {
            if (member.name.compare(name) == 0)
            {
                if (member.field.compare(field) != 0)
                {
                    printf("Warning constant parameter '%s' was declared again with a different format '%s'", member.field.c_str(), field.c_str());
                }

                member.stageFlags |= 1u << (uint32_t)stage;
                return true;
            }
        }

        members.push_back({ name, field, 1u << (uint32_t)stage });
        return true;
    }

    static bool ExtractPushConstantBlock(size_t& offset, std::string& source, PKShaderStage stage, SourcePushConstants& members)
    {
        size_t blockStart, blockEnd;

        if (!StringUtilities::FindScope(source, offset, "{", "}", &blockStart, &blockEnd))
        {
            return false;
        }

        blockEnd++;

        if (source[blockEnd] != ';')
        {
            return false;
        }

        auto currentPos = blockStart + 1ull;

        while (true)
        {
            currentPos = source.find_first_not_of(" \n\r", currentPos);

            if (currentPos == std::string::npos)
            {
                break;
            }

            auto memberEnd = currentPos;
            if (!ExtractPrimitiveVariable(memberEnd, source, stage, members))
            {
                break;
            }

            currentPos = memberEnd;
        }

        offset = blockEnd + 1ull;
        return true;
    }

    void ExtractPushConstants(std::string& source, PKShaderStage stage, SourcePushConstants& members)
    {
        auto currentpos = 0ull;

        while (true)
        {
            auto startSingle = source.find("uniform ", currentpos);
            auto startLayout = source.find("layout(push_constant) ", currentpos);

            if (startSingle == std::string::npos && startLayout == std::string::npos)
            {
                break;
            }

            if (startSingle < startLayout)
            {
                auto endSingle = startSingle + 8ull;
                if (ExtractPrimitiveVariable(endSingle, source, stage, members))
                {
                    source.erase(startSingle, endSingle - startSingle);
                    continue;
                }

                currentpos = endSingle;
            }
            else
            {
                auto endLayout = startLayout + 22ull;
                if (ExtractPushConstantBlock(endLayout, source, stage, members))
                {
                    source.erase(startLayout, endLayout - startLayout);
                    continue;
                }

                currentpos = endLayout;
            }
        }
    }

    void CompilePushConstantBlock(std::string* stageSources, const SourcePushConstants& constants)
    {
        struct SortedField
        {
            std::string field;
            uint32_t bits;
        };

        if (constants.size() == 0)
        {
            return;
        }

        std::vector<SortedField> sorted;
        sorted.reserve(constants.size());

        for (auto& constant : constants)
        {
            sorted.push_back({ constant.field, CountBits(constant.stageFlags) });
        }

        std::stable_sort(sorted.begin(), sorted.end(), [](const SortedField& a, const SortedField& b)
        {
            return a.bits > b.bits;
        });

        std::string layout;
        layout.append("layout(scalar, push_constant) uniform pk_global_push_constant_block\n{\n");
        for (auto& member : sorted)
        {
            layout.append(member.field);
        }
        layout.append("};\n");

        for (auto i = 0u; i < (uint32_t)PKShaderStage::EnumCount; ++i)
        {
            if (!stageSources[i].empty())
            {
                auto insertPos = 0ull;

                while (insertPos < stageSources[i].size())
                {
                    const auto lineStart = stageSources[i].find_first_not_of(" \t\r\n", insertPos);

                    if (lineStart == std::string::npos || stageSources[i][lineStart] != '#')
                    {
                        insertPos = lineStart != std::string::npos ? lineStart : stageSources[i].size();
                        break;
                    }

                    const auto lineEnd = stageSources[i].find_first_of("\r\n", lineStart);
                    insertPos = lineEnd != std::string::npos ? lineEnd + 1ull : stageSources[i].size();
                }

                stageSources[i].insert(insertPos, layout);
            }
        }
    }

    void AddLineCounter(std::string* stageSources)
    {
        for (auto i = 0u; i < (uint32_t)PKShaderStage::EnumCount; ++i)
        {
            if (!stageSources[i].empty())
            {
                stageSources[i].insert(stageSources[i].find_first_of("\r\n") + 1ull, "#line 2\n");
            }
        }
    }
    
    void RemoveUnusedResourcesAndFunctions(std::string& source)
    {
        bool changed = true;
        while (changed)
        {
            changed = false;
            const auto statements = ScanTopLevelStatements(source);

            for (int32_t s = (int32_t)statements.size() - 1; s >= 0; --s)
            {
                const auto& stmt = statements[s];

                if (stmt.isFunction)
                {
                    // Find function name before parameter list '('
                    int32_t pDepth = 0;
                    size_t paramOpen = std::string::npos;
                    for (size_t k = stmt.openBrace; k > stmt.start; --k)
                    {
                        if (source[k - 1ull] == ')') pDepth++;
                        else if (source[k - 1ull] == '(')
                        {
                            if (--pDepth == 0)
                            {
                                paramOpen = k - 1ull;
                                break;
                            }
                        }
                    }

                    if (paramOpen != std::string::npos)
                    {
                        const auto funcName = ExtractLastIdentifierBefore(std::string_view(source.data() + stmt.start, paramOpen - stmt.start));
                        if (!funcName.empty() && funcName != "main" && CountIdentifierOccurrences(source, funcName) <= 1ull)
                        {
                            source.erase(stmt.start, stmt.end - stmt.start);
                            changed = true;
                        }
                    }
                    continue;
                }

                // Block declaration: uniform / buffer { ... } [instanceName];
                if (stmt.openBrace != std::string::npos && stmt.closeBrace != std::string::npos)
                {
                    const auto header = StripLayoutQualifiers(source.substr(stmt.start, stmt.openBrace - stmt.start));
                    const auto tokens = StringUtilities::Split(header, " \t\r\n");

                    bool isResourceBlock = false;
                    for (const auto& t : tokens)
                    {
                        if (t == "uniform" || t == "buffer")
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

                // Simple declaration: [layout(...)] uniform Type Name[...];
                const auto decl = StripLayoutQualifiers(source.substr(stmt.start, (stmt.end - 1ull) - stmt.start));
                const auto tokens = StringUtilities::Split(decl, " \t\r\n");
                if (tokens.size() >= 3ull && tokens[0] == "uniform")
                {
                    const auto varName = ExtractLastIdentifierBefore(decl);
                    if (!varName.empty() && CountIdentifierOccurrences(source, varName) <= 1ull)
                    {
                        source.erase(stmt.start, stmt.end - stmt.start);
                        changed = true;
                    }
                }
            }
        }
    }

    void RemoveUnusedStageOutputs(std::string* stageSources)
    {
        // Find the last active pre-fragment graphics stage
        int32_t producerStage = -1;
        for (int32_t stage = (int32_t)PKShaderStage::MeshAssembly; stage >= (int32_t)PKShaderStage::Vertex; --stage)
        {
            if (!stageSources[stage].empty())
            {
                producerStage = stage;
                break;
            }
        }

        if (producerStage == -1)
        {
            return;
        }

        std::vector<std::string> activeFragInputs;
        auto& fragSource = stageSources[(uint32_t)PKShaderStage::Fragment];

        if (!fragSource.empty())
        {
            const auto fragStmts = ScanTopLevelStatements(fragSource);
            for (int32_t s = (int32_t)fragStmts.size() - 1; s >= 0; --s)
            {
                const auto& stmt = fragStmts[s];
                if (stmt.isFunction || stmt.openBrace != std::string::npos)
                {
                    continue;
                }

                auto decl = StripLayoutQualifiers(fragSource.substr(stmt.start, (stmt.end - 1ull) - stmt.start));
                const size_t bracketPos = decl.find('[');
                if (bracketPos != std::string::npos)
                {
                    decl.erase(bracketPos);
                }

                const auto tokens = StringUtilities::Split(decl, " \t\r\n");
                if (tokens.size() < 3ull)
                {
                    continue;
                }

                bool isInput = false;
                for (size_t i = 0ull; i + 2ull < tokens.size(); ++i)
                {
                    if (tokens[i] == "in")
                    {
                        isInput = true;
                        break;
                    }
                }

                if (isInput)
                {
                    const auto& varName = tokens.back();
                    if (CountIdentifierOccurrences(fragSource, varName) <= 1ull)
                    {
                        fragSource.erase(stmt.start, stmt.end - stmt.start);
                    }
                    else
                    {
                        activeFragInputs.push_back(varName);
                    }
                }
            }
        }

        auto& prodSource = stageSources[producerStage];
        const auto prodStmts = ScanTopLevelStatements(prodSource);

        for (int32_t s = (int32_t)prodStmts.size() - 1; s >= 0; --s)
        {
            const auto& stmt = prodStmts[s];
            if (stmt.isFunction || stmt.openBrace != std::string::npos)
            {
                continue;
            }

            auto decl = StripLayoutQualifiers(prodSource.substr(stmt.start, (stmt.end - 1ull) - stmt.start));
            std::string arraySuffix;
            const size_t bracketPos = decl.find('[');
            if (bracketPos != std::string::npos)
            {
                arraySuffix = decl.substr(bracketPos);
                decl.erase(bracketPos);
            }

            const auto tokens = StringUtilities::Split(decl, " \t\r\n");
            if (tokens.size() < 3ull)
            {
                continue;
            }

            bool isOutput = false;
            for (size_t i = 0ull; i + 2ull < tokens.size(); ++i)
            {
                if (tokens[i] == "out")
                {
                    isOutput = true;
                    break;
                }
            }

            if (!isOutput)
            {
                continue;
            }

            const auto& varName = tokens.back();
            const auto& typeName = tokens[tokens.size() - 2ull];
            const bool usedInFrag = std::find(activeFragInputs.begin(), activeFragInputs.end(), varName) != activeFragInputs.end();

            if (!usedInFrag)
            {
                // Demote unused stage output to a private global variable so spirv-opt dead-strips all writes to it
                if (arraySuffix == "[]")
                {
                    arraySuffix = "[256]";
                }

                const std::string replacement = typeName + " " + varName + arraySuffix + ";";
                prodSource.replace(stmt.start, stmt.end - stmt.start, replacement);
            }
        }
    }
}
