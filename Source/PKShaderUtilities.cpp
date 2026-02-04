#include "PKShaderUtilities.h"
#include "PKStringUtilities.h"

namespace PKAssets::Shader
{
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
            case PKElementType::Float3x4: return "mat3x4";
            case PKElementType::Double2x2: return "f64mat2";
            case PKElementType::Double3x3: return "f64mat3";
            case PKElementType::Double4x4: return "f64mat4";
            case PKElementType::Half2x2: return "f16mat2";
            case PKElementType::Half3x3: return "f16mat3";
            case PKElementType::Half4x4: return "f16mat4";
            case PKElementType::Texture2DHandle: return "uint";
            case PKElementType::Texture3DHandle: return "uint";
            case PKElementType::TextureCubeHandle: return "uint";
            case PKElementType::Invalid: return "INVALID";
        }
    }

    PKDescriptorType GetResourceType(SpvReflectDescriptorType type)
    {
        switch (type)
        {
            case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER: return PKDescriptorType::Sampler;
            case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return PKDescriptorType::SamplerTexture;
            case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE: return PKDescriptorType::Texture;
            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE: return PKDescriptorType::Image;
            case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER: return PKDescriptorType::Invalid; // Not supported
            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: return PKDescriptorType::Invalid; // Not supported
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
            case PKShaderStage::MeshTask: return shaderc_task_shader;
            case PKShaderStage::MeshAssembly: return shaderc_mesh_shader;

            case PKShaderStage::RayGeneration: return shaderc_raygen_shader;
            case PKShaderStage::RayMiss: return shaderc_miss_shader;
            case PKShaderStage::RayClosestHit: return shaderc_closesthit_shader;
            case PKShaderStage::RayAnyHit: return shaderc_anyhit_shader;
            case PKShaderStage::RayIntersection: return shaderc_intersection_shader;

            case PKShaderStage::MaxCount: return shaderc_shader_kind::shaderc_glsl_infer_from_source;
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

    void FindLineRange(const std::string& name, const std::string& message, int* outMin, int* outMax)
    {
        *outMin = 0xFFFF;
        *outMax = -0xFFFF;

        auto token = name + std::string(":");
        auto pos = message.find(token.c_str(), 0);

        while (pos != std::string::npos)
        {
            auto eol = message.find_first_of("\r\n", pos);
            auto sol = message.find_first_not_of("\r\n", eol);
            
            auto spos = pos + strlen(token.c_str());
            auto eot = message.find_first_of(":", spos);
            
            if (eot != std::string::npos && isdigit(message[spos]))
            {
                auto number = std::stoi(message.substr(spos, eot - spos));

                if (number < *outMin)
                {
                    *outMin = number;
                }

                if (number > *outMax)
                {
                    *outMax = number;
                }
            }

            if (eol == std::string::npos ||
                sol == std::string::npos)
            {
                return;
            }

            pos = message.find(token.c_str(), sol);
        }
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
            attributes->ztest = PKAssets::StringToPKComparison(valueZTest.c_str());
        }

        if (!valueBlendColor.empty())
        {
            auto keywords = StringUtilities::Split(valueBlendColor, " \n\r");

            attributes->blendSrcFactorColor = PKBlendFactor::None;
            attributes->blendDstFactorColor = PKBlendFactor::None;
            attributes->blendOpColor = PKBlendOp::None;

            if (keywords.size() == 3)
            {
                attributes->blendOpColor = PKAssets::StringToPKBlendOp(keywords.at(0).c_str());
                attributes->blendSrcFactorColor = PKAssets::StringToPKBlendFactor(keywords.at(1).c_str());
                attributes->blendDstFactorColor = PKAssets::StringToPKBlendFactor(keywords.at(2).c_str());
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
                attributes->blendOpAlpha = PKAssets::StringToPKBlendOp(keywords.at(0).c_str());
                attributes->blendSrcFactorAlpha = PKAssets::StringToPKBlendFactor(keywords.at(0).c_str());
                attributes->blendDstFactorAlpha = PKAssets::StringToPKBlendFactor(keywords.at(2).c_str());
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
                attributes->rasterMode = PKAssets::StringToPKRasterMode(keywords.at(0).c_str());
            }

            if (keywords.size() > 1)
            {
                attributes->overEstimation = (uint8_t)std::stoi(keywords.at(1));
            }
        }

        attributes->colorMask = PKAssets::StringToPKColorMask(valueColorMask.c_str());
        attributes->cull = PKAssets::StringToPKCullMode(valueCull.c_str());
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
        size_t currentpos = 0ull;

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
            auto arguments = StringUtilities::Split(content, ",");
            auto foundEntry = false;

            for (auto& arg : arguments)
            {
                auto trimmed = StringUtilities::Trim(arg);

                if (trimmed.compare(entryPointName) == 0 || trimmed.compare(PK_SHADER_STAGE_NAMES[(uint32_t)stage]) == 0)
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

            size_t currentpos = 0ull;

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
        size_t currentpos = 0ull;

        while (true)
        {
            size_t open, close;
            if (!StringUtilities::FindScope(source, currentpos, "layout(", ")", &open, &close))
            {
                break;
            }

            constexpr auto lengthopen = 7u;
            constexpr auto lengthclose = 1u;
            auto tokens = StringUtilities::Split(source.substr(open + lengthopen, close - (open + lengthopen)), ",");

            auto isSet = false;

            for (auto& token : tokens)
            {
                token.erase(std::remove_if(token.begin(), token.end(), std::isspace), token.end());
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

    void ConvertPKNumThreads(std::string& source)
    {
        size_t currentpos = 0ull;

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
            auto values = StringUtilities::Split(source.substr(open + lengthopen, close - (open + lengthopen)), ",");
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

    void ConvertHLSLTypesToGLSL(std::string& source)
    {
        const std::string surroundMask = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.";
        StringUtilities::ReplaceAll(source, surroundMask, "lerp", "mix");
        StringUtilities::ReplaceAll(source, surroundMask, "asuint", "floatBitsToUint");
        StringUtilities::ReplaceAll(source, surroundMask, "asfloat", "uintBitsToFloat");

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
        StringUtilities::ReplaceAll(source, surroundMask, "half2x3", "f16mat2x3");
        StringUtilities::ReplaceAll(source, surroundMask, "half2x4", "f16mat2x4");

        StringUtilities::ReplaceAll(source, surroundMask, "half3x2", "f16mat3x2");
        StringUtilities::ReplaceAll(source, surroundMask, "half3x3", "f16mat3");
        StringUtilities::ReplaceAll(source, surroundMask, "half3x4", "f16mat3x4");

        StringUtilities::ReplaceAll(source, surroundMask, "half4x2", "f16mat4x2");
        StringUtilities::ReplaceAll(source, surroundMask, "half4x3", "f16mat4x3");
        StringUtilities::ReplaceAll(source, surroundMask, "half4x4", "f16mat4");

        StringUtilities::ReplaceAll(source, surroundMask, "float2x2", "mat2");
        StringUtilities::ReplaceAll(source, surroundMask, "float2x3", "mat2x3");
        StringUtilities::ReplaceAll(source, surroundMask, "float2x4", "mat2x4");

        StringUtilities::ReplaceAll(source, surroundMask, "float3x2", "mat3x2");
        StringUtilities::ReplaceAll(source, surroundMask, "float3x3", "mat3");
        StringUtilities::ReplaceAll(source, surroundMask, "float3x4", "mat3x4");
        
        StringUtilities::ReplaceAll(source, surroundMask, "float4x2", "mat4x2");
        StringUtilities::ReplaceAll(source, surroundMask, "float4x3", "mat4x3");
        StringUtilities::ReplaceAll(source, surroundMask, "float4x4", "mat4");

        StringUtilities::ReplaceAll(source, surroundMask, "double2x2", "f64mat2");
        StringUtilities::ReplaceAll(source, surroundMask, "double2x3", "f64mat2x3");
        StringUtilities::ReplaceAll(source, surroundMask, "double2x4", "f64mat2x4");

        StringUtilities::ReplaceAll(source, surroundMask, "double3x2", "f64mat3x2");
        StringUtilities::ReplaceAll(source, surroundMask, "double3x3", "f64mat3");
        StringUtilities::ReplaceAll(source, surroundMask, "double3x4", "f64mat3x4");
        
        StringUtilities::ReplaceAll(source, surroundMask, "double4x2", "f64mat4x2");
        StringUtilities::ReplaceAll(source, surroundMask, "double4x3", "f64mat4x3");
        StringUtilities::ReplaceAll(source, surroundMask, "double4x4", "f64mat4");
    }
}
