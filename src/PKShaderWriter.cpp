#pragma once
#include "PKShaderWriter.h"
#include "PKAssetWriter.h"
#include "PKStringUtilities.h"
#include <shaderc/shaderc.hpp>
#include <SPIRV-Reflect/spirv_reflect.h>
#include <spirv-tools/optimizer.hpp>
#include <unordered_map>
#include <map>
#include <stdexcept>
#include <sstream>

namespace PK::Assets::Shader
{
    typedef shaderc::Compiler ShaderCompiler;

    struct ReflectBinding
    {
        uint_t firstStage = (int)PKShaderStage::MaxCount;
        SpvReflectDescriptorBinding* bindings[(int)PKShaderStage::MaxCount]{};
        SpvReflectDescriptorBinding* get() { return bindings[firstStage]; }
    };

    typedef std::vector<uint32_t> ShaderSpriV;

    struct ReflectionData
    {
        SpvReflectShaderModule modules[(int)PKShaderStage::MaxCount]{};
        std::map<std::string, ReflectBinding> uniqueBindings;
        std::map<std::string, SpvReflectBlockVariable*> uniqueVariables;
        std::map<std::string, uint32_t> variableStageFlags;
        std::map<uint32_t, uint32_t> setStageFlags;
        uint32_t setCount = 0u;
    };

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
            pos = StringUtilities::ExtractToken(pos, "#multi_compile ", source, output, false);

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
        auto valueZWrite = StringUtilities::ExtractToken("#ZWrite ", source, false);
        auto valueZTest = StringUtilities::ExtractToken("#ZTest ", source, false);
        auto valueBlendColor = StringUtilities::ExtractToken("#BlendColor ", source, false);
        auto valueBlendAlpha = StringUtilities::ExtractToken("#BlendAlpha ", source, false);
        auto valueColorMask = StringUtilities::ExtractToken("#ColorMask ", source, false);
        auto valueCull = StringUtilities::ExtractToken("#Cull ", source, false);
        auto valueOffset = StringUtilities::ExtractToken("#Offset ", source, false);

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
                attributes->blendSrcFactorColor = GetBlendFactorFromString(keywords.at(0));
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
                                   std::string>& shaderSources)
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
            ProcessShaderStageDefine(kv.first, kv.second);
            kv.second.insert(0, variantDefines);
            ProcessShaderVersion(kv.second);
        }

        return 0;
    }

    // @TODO This doesn't work atm. Generates an invalid op code when trying to create a vulkan shader module. find a solution?
    static ShaderSpriV OptimizeSpirv(const uint32_t* code, size_t size, bool stripDebug)
    {
        spvtools::Optimizer opt(SPV_ENV_VULKAN_1_2);
        auto print_msg_to_stderr = [](spv_message_level_t, const char*, const spv_position_t&, const char* m) { printf("error: %s \n", m); };

        opt.SetMessageConsumer(print_msg_to_stderr);

        if (stripDebug)
        {
            opt.RegisterPass(spvtools::CreateStripDebugInfoPass())
               .RegisterPass(spvtools::CreateStripReflectInfoPass());
        }
        else
        {
            opt.RegisterPass(spvtools::CreateWrapOpKillPass())
               .RegisterPass(spvtools::CreateDeadBranchElimPass())
               .RegisterPass(spvtools::CreateMergeReturnPass())
               .RegisterPass(spvtools::CreateInlineExhaustivePass())
               .RegisterPass(spvtools::CreateEliminateDeadFunctionsPass())
             //  .RegisterPass(spvtools::CreateAggressiveDCEPass())
               .RegisterPass(spvtools::CreatePrivateToLocalPass())
               .RegisterPass(spvtools::CreateLocalSingleBlockLoadStoreElimPass())
               .RegisterPass(spvtools::CreateLocalSingleStoreElimPass())
            //   .RegisterPass(spvtools::CreateAggressiveDCEPass())
               .RegisterPass(spvtools::CreateScalarReplacementPass())
               .RegisterPass(spvtools::CreateLocalAccessChainConvertPass())
               .RegisterPass(spvtools::CreateLocalSingleBlockLoadStoreElimPass())
               .RegisterPass(spvtools::CreateLocalSingleStoreElimPass())
            //   .RegisterPass(spvtools::CreateAggressiveDCEPass())
               .RegisterPass(spvtools::CreateLocalMultiStoreElimPass())
             //  .RegisterPass(spvtools::CreateAggressiveDCEPass())
               .RegisterPass(spvtools::CreateCCPPass())
             //  .RegisterPass(spvtools::CreateAggressiveDCEPass())
               .RegisterPass(spvtools::CreateLoopUnrollPass(true))
               .RegisterPass(spvtools::CreateDeadBranchElimPass())
               .RegisterPass(spvtools::CreateRedundancyEliminationPass())
               .RegisterPass(spvtools::CreateCombineAccessChainsPass())
               .RegisterPass(spvtools::CreateSimplificationPass())
               .RegisterPass(spvtools::CreateScalarReplacementPass())
               .RegisterPass(spvtools::CreateLocalAccessChainConvertPass())
               .RegisterPass(spvtools::CreateLocalSingleBlockLoadStoreElimPass())
               .RegisterPass(spvtools::CreateLocalSingleStoreElimPass())
             //  .RegisterPass(spvtools::CreateAggressiveDCEPass())
               .RegisterPass(spvtools::CreateSSARewritePass())
              // .RegisterPass(spvtools::CreateAggressiveDCEPass())
               .RegisterPass(spvtools::CreateVectorDCEPass())
               .RegisterPass(spvtools::CreateDeadInsertElimPass())
               .RegisterPass(spvtools::CreateDeadBranchElimPass())
               .RegisterPass(spvtools::CreateSimplificationPass())
               .RegisterPass(spvtools::CreateIfConversionPass())
               .RegisterPass(spvtools::CreateCopyPropagateArraysPass())
              // .RegisterPass(spvtools::CreateReduceLoadSizePass())
           //    .RegisterPass(spvtools::CreateAggressiveDCEPass())
               .RegisterPass(spvtools::CreateBlockMergePass())
               .RegisterPass(spvtools::CreateRedundancyEliminationPass())
               .RegisterPass(spvtools::CreateDeadBranchElimPass())
               .RegisterPass(spvtools::CreateBlockMergePass())
               .RegisterPass(spvtools::CreateSimplificationPass());
        }

        ShaderSpriV spirv;
        if (!opt.Run(code, size, &spirv))
        {
            return ShaderSpriV();
        }

        return spirv;
    }


    static ShaderSpriV CompileGLSLToSpirV(const ShaderCompiler& compiler, PKShaderStage stage, const std::string& source_name, const std::string& source)
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
            return ShaderSpriV();
        }

        return { module.cbegin(), module.cend() }; //OptimizeSpirv(module.cbegin(), module.cend() - module.cbegin(), false);
    }

    static int GetReflectionModule(ReflectionData& reflection, PKShaderStage stage, const ShaderSpriV& spriv)
    {
        auto* module = &reflection.modules[(uint32_t)stage];

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

    static void GetUniqueBindings(ReflectionData& reflection, PKShaderStage stage)
    {
        auto* module = &reflection.modules[(uint32_t)stage];

        uint32_t bindingCount = 0u;
        uint32_t setCount = 0u;

        spvReflectEnumerateDescriptorBindings(module, &bindingCount, nullptr);
        spvReflectEnumerateDescriptorSets(module, &setCount, nullptr);

        if (setCount > reflection.setCount)
        {
            reflection.setCount = setCount;
        }

        std::vector<SpvReflectDescriptorBinding*> activeBindings;
        activeBindings.resize(bindingCount);
        spvReflectEnumerateDescriptorBindings(module, &bindingCount, activeBindings.data());

        for (auto i = 0u; i < bindingCount; ++i)
        {
            auto desc = activeBindings.at(i);
            auto name = std::string(desc->name);

            if (name.empty())
            {
                name = std::string(desc->type_description->type_name);
            }

            reflection.setStageFlags[activeBindings.at(i)->set] |= 1 << (int)stage;

            auto& binding = reflection.uniqueBindings[name];
            binding.firstStage = binding.firstStage > (int)stage ? (int)stage : binding.firstStage;
            binding.bindings[(int)stage] = activeBindings.at(i);
        }
    }

    static void GetVertexAttributes(ReflectionData& reflection, PKShaderStage stage, PKVertexAttribute* attributes)
    {
        if (stage != PKShaderStage::Vertex)
        {
            return;
        }

        auto* module = &reflection.modules[(uint32_t)stage];

        auto count = 0u;
        spvReflectEnumerateEntryPointInputVariables(module, module->entry_point_name, &count, nullptr);

        std::vector<SpvReflectInterfaceVariable*> variables;
        variables.resize(count);

        spvReflectEnumerateEntryPointInputVariables(module, module->entry_point_name, &count, variables.data());

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

    static void GetPushConstants(ReflectionData& reflection, PKShaderStage stage)
    {
        auto* module = &reflection.modules[(uint32_t)stage];

        auto count = 0u;
        spvReflectEnumerateEntryPointPushConstantBlocks(module, module->entry_point_name, &count, nullptr);

        std::vector<SpvReflectBlockVariable*> blocks;
        blocks.resize(count);

        spvReflectEnumerateEntryPointPushConstantBlocks(module, module->entry_point_name, &count, blocks.data());

        auto i = 0;

        for (auto* block : blocks)
        {
            auto name = std::string(block->name);
            reflection.variableStageFlags[name] |= 1 << (int)stage;

            if (reflection.uniqueVariables.count(name) <= 0)
            {
                reflection.uniqueVariables[name] = block;
            }
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
        std::unordered_map<PKShaderStage, std::string> shaderSources;

        ReadFile(pathSrc, source);
        ExtractMulticompiles(source, mckeywords, keywords, &shader.get()->variantcount, &directiveCount);
        ExtractStateAttributes(source, &shader.get()->attributes);
        GetSharedInclude(source, sharedInclude);

        auto pKeywords = buffer.Write<PKShaderKeyword>(keywords.data(), keywords.size());
        shader.get()->keywords.Set(buffer.data(), pKeywords);
        
        auto pVariants = buffer.Allocate<PKShaderVariant>(shader.get()->variantcount);
        shader.get()->variants.Set(buffer.data(), pVariants);

        for (uint32_t i = 0; i < shader.get()->variantcount; ++i)
        {
            GetVariantDefines(mckeywords, i, variantDefines);

            if (ProcessStageSources(source, sharedInclude, variantDefines, shaderSources) != 0)
            {
                printf("Failed to preprocess shader variant glsl stage sources! \n");
                return -1;
            }

            ReflectionData reflectionData{};

            for (auto& kv : shaderSources)
            {
                printf("Compiling %s variant %i stage % i \n", filename.c_str(), i, (int)kv.first);

                auto spirv = CompileGLSLToSpirV(compiler, kv.first, filename, kv.second);

                if (spirv.size() == 0)
                {
                    printf("Failed to compile data from shader variant source! \n");
                    return -1;
                }

                if (GetReflectionModule(reflectionData, kv.first, spirv) != 0)
                {
                    printf("Failed to extract reflection data from shader variant source! \n");
                    return -1;
                }

                GetUniqueBindings(reflectionData, kv.first);
                GetVertexAttributes(reflectionData, kv.first, pVariants[i].vertexAttributes);
                GetPushConstants(reflectionData, kv.first);
            }

            CompressBindIndices(reflectionData);

            for (auto& kv : shaderSources)
            {
                auto size = spvReflectGetCodeSize(&reflectionData.modules[(int)kv.first]);
                auto code = spvReflectGetCode(&reflectionData.modules[(int)kv.first]);
               // auto optimized = OptimizeSpirv(code, size / sizeof(uint32_t), true);
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

                for (auto& kv : reflectionData.uniqueVariables)
                {
                    WriteName(pConstantVariables[i].name, kv.first.c_str());
                    pConstantVariables[i].offset = (unsigned short)kv.second->offset;
                    pConstantVariables[i].stageFlags = reflectionData.variableStageFlags[kv.first];
                    pConstantVariables[i].size = kv.second->size;
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
                    auto i = kv.first;
                    pDescriptorSets[i].descriptorCount = (uint_t)kv.second.size();
                    pDescriptorSets[i].stageflags = reflectionData.setStageFlags[kv.first];
                    auto pDescriptors = buffer.Write(kv.second.data(), kv.second.size());
                    pDescriptorSets[i].descriptors.Set(buffer.data(), pDescriptors);
                }
            }

            ReleaseReflectionData(reflectionData);
        }

        return WriteAsset(pathDst, buffer);
    }
}