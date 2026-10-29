#include <unordered_map>
#include <map>
#include <stdexcept>
#include <sstream>
#include <shaderc/shaderc.hpp>
#include "PKStringUtilities.h"
#include "PKSPVUtilities.h"
#include "PKShaderUtilities.h"
#include "PKShaderInstancing.h"
#include "PKFileVersionUtilities.h"
#include "PKAssetWriter.h"
#include "PKShaderWriter.h"

#ifdef _WIN32
#include <Windows.h>
#undef far
#undef near
#endif

namespace PKAssets::Shader
{
    static bool ReadFile(const std::string& filepath, std::string& ouput, std::filesystem::file_time_type lastDestWriteTime)
    {
        std::vector<std::string> includes;
        ouput = StringUtilities::ReadFileRecursiveInclude(filepath, includes);
        return PKVersionUtilities::IsFileAnyOutOfDate(includes, lastDestWriteTime);
    }

    static void ExtractMulticompiles(std::string& source, std::vector<std::string>& outVariantDefines, std::vector<PKShaderKeyword>& outKeywords)
    {
        std::vector<std::vector<std::string>> keywords{};
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

            if (vcount >= PK_ASSET_MAX_SHADER_KEYWORDS)
            {
                printf("Warning maximum number of shader variants exceeded!.\n");
                continue;
            }

            if (dcount >= PK_ASSET_MAX_SHADER_DIRECTIVES)
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
                    PKShaderKeyword pkkw{};
                    pkkw.offset = (uint16_t)vcount;
                    pkkw.directive = dcount;
                    pkkw.value = i;
                    WriteName(pkkw.name, keyword.c_str());
                    outKeywords.push_back(pkkw);
                }
            }

            keywords.push_back(directive);
            dcount++;
            vcount *= directive.size();
        }

        outVariantDefines.reserve(vcount);

        for (auto i = 0u; i < vcount; ++i)
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

    static int ExtractEntryPoints(std::string& source, std::vector<EntryPointInfo>& entryPoints)
    {
        std::string output;
        size_t pos = 0;

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
                printf("Entry point declaration contains an invalid amount of arguments (should be name, stage)\n");
                return -1;
            }

            auto stage = StringToPKShaderStage(directives[0].c_str());

            if (stage == PKShaderStage::EnumCount)
            {
                printf("Unsupported shader stage specified! \n");
                return -1;
            }

            EntryPointInfo info;
            info.name = directives[1];
            info.functionName = "void " + info.name + "()";
            info.stage = stage;
            info.keyword = directives.size() == 3 ? directives[2] : "";

            if (source.find(info.functionName, 0u) == std::string::npos)
            {
                printf("Definition for declared entry point %s was not found!", info.name.c_str());
                return -1;
            }

            entryPoints.push_back(info);
        }

        return 0;
    }

    static void SetActiveEntryPoint(std::string& source, const std::vector<EntryPointInfo>& entries, uint32_t activeIndex)
    {
        for (auto i = 0u; i < entries.size(); ++i)
        {
            if (i != activeIndex)
            {
                auto& entry = entries.at(i);
                auto position = source.find(entry.functionName, 0u);

                size_t scopeEnd;
                
                if (StringUtilities::FindScope(source, position, '{', '}', nullptr, &scopeEnd))
                {
                    source.erase(position, (scopeEnd + 1u) - position);
                }
            }
        }

        source.replace(source.find(entries[activeIndex].functionName), entries[activeIndex].functionName.size(), "void main()");
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

                    // Need to check that the match is exact in case of similar keyword define in another pass.
                    if (keypos != std::string::npos && isspace(defines[keypos + entries[i].keyword.size()]))
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

    static int32_t PreprocessGLSL(const ShaderCompiler& compiler, const CompileOptions& options, PKShaderStage stage, const std::string& name, std::string& source)
    {
        auto result = compiler.PreprocessGlsl(source, ConvertToShadercKind(stage), name.c_str(), options);

        if (result.GetCompilationStatus() != shaderc_compilation_status_success)
        {
            printf("\n ----------BEGIN ERROR---------- \n");
            printf("\n Stage: %s\n\n", PK_SHADER_STAGE_NAMES[(uint32_t)stage]);
            printf("%s", result.GetErrorMessage().c_str());
            printf("\n ----------END ERROR---------- \n\n");
            return -1;
        }

        source = std::string(result.cbegin(), result.cend());
        return 0;
    }

    static SpvReflectShaderModule* CompileToSPIRV(const ShaderCompiler& compiler, const CompileOptions& options, PKShaderStage stage, const std::string& name, const std::string& source)
    {
        // Will crash to buffer owerflow on variable name syntax errors :/
        auto result = compiler.CompileGlslToSpv(source, ConvertToShadercKind(stage), name.c_str(), options);

        if (result.GetCompilationStatus() != shaderc_compilation_status_success || result.GetNumWarnings() > 0)
        {
            #if defined(WIN32)
            SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 12);
            #endif

            printf("\n ----------BEGIN ERROR---------- \n");
            printf("\n Stage: %s\n\n", PK_SHADER_STAGE_NAMES[(uint32_t)stage]);
            printf("%s", FormatErrorMessage(name, source, result.GetErrorMessage()).c_str());
            printf("\n ----------END ERROR---------- \n\n");

            #if defined(WIN32)
            SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 15);
            #endif
        }

        if (result.GetCompilationStatus() != shaderc_compilation_status_success)
        {
            printf("Failed to compile SPIRV from shader variant source! \n");
            return nullptr;
        }

        auto module = new SpvReflectShaderModule();
        const auto codeSize = (size_t)(result.cend() - result.cbegin()) * sizeof(uint32_t);
        const auto codeStatus = spvReflectCreateShaderModule(codeSize, result.cbegin(), module);

        if (codeStatus != SPV_REFLECT_RESULT_SUCCESS)
        {
            printf("Failed to extract reflection data from shader variant source! \n");
            delete module;
            return nullptr;
        }

        return module;
    }

    static void ReleaseReflectionData(ReflectionData& reflection)
    {
        for (auto i = 0u; i < (int)PKShaderStage::EnumCount; ++i)
        {
            if (reflection.modules[i])
            {
                spvReflectDestroyShaderModule(reflection.modules[i]);
                delete reflection.modules[i];
            }
        }
    }

    static void GetVertexAttributes(ReflectionData& reflection, PKShaderStage stage)
    {
        if (stage != PKShaderStage::Vertex)
        {
            return;
        }

        auto* module = reflection.modules[(uint32_t)stage];

        auto count = 0u;
        spvReflectEnumerateEntryPointInputVariables(module, module->entry_point_name, &count, nullptr);

        std::vector<SpvReflectInterfaceVariable*> variables(count);
        spvReflectEnumerateEntryPointInputVariables(module, module->entry_point_name, &count, variables.data());

        if (count > 0 && reflection.logVerbose)
        {
            printf("    Interface:");
        }

        for (auto* variable : variables)
        {
            if (variable->built_in != -1)
            {
                continue;
            }

            if (reflection.vertexAttributes.size() >= PK_ASSET_MAX_VERTEX_ATTRIBUTES)
            {
                printf("Warning! Shader has more vertex attributes than supported (%i / %i) \n", (int)reflection.vertexAttributes.size(), PK_ASSET_MAX_VERTEX_ATTRIBUTES);
                continue;
            }

            PKVertexInputAttribute attribute{};
            WriteName(attribute.name, variable->name);
            attribute.location = variable->location;
            attribute.type = GetElementType(variable->format);
            reflection.vertexAttributes.push_back(attribute);

            if (reflection.logVerbose)
            {
                printf(" %s ", variable->name);
            }
        }

        if (count > 0 && reflection.logVerbose)
        {
            printf("\n");
        }
    }

    static void GetPushConstants(ReflectionData& reflection, PKShaderStage stage)
    {
        const auto* module = reflection.modules[(uint32_t)stage];
        const auto* block = spvReflectGetEntryPointPushConstantBlock(module, "main", nullptr);

        if (block == nullptr)
        {
            return;
        }

        if (reflection.logVerbose)
        {
            printf("    Constants:");
        }

        reflection.constantRangeSize = max(reflection.constantRangeSize, block->size);

        for (auto i = 0u; i < block->member_count; ++i)
        {
            const auto* member = block->members + i;

            if (member->flags & SPV_REFLECT_VARIABLE_FLAGS_UNUSED)
            {
                continue;
            }

            const auto name = std::string(member->name);
            reflection.uniqueConstants[name] = { (uint16_t)member->absolute_offset, (uint16_t)member->size };

            if (reflection.logVerbose)
            {
                printf(" %s", name.c_str());
            }
        }

        if (reflection.logVerbose)
        {
            printf("\n");
        }
    }

    static void GetComputeGroupSize(ReflectionData& reflection, PKShaderStage stage, uint16_t* outSize, bool logVerbose)
    {
        auto* module = reflection.modules[(uint32_t)stage];
        auto* entryPoint = module->entry_points;

        uint32_t size[3];

        if (ReflectLocalSize(module->_internal->spirv_code, module->_internal->spirv_word_count, size))
        {
            outSize[0] = (uint16_t)size[0];
            outSize[1] = (uint16_t)size[1];
            outSize[2] = (uint16_t)size[2];
        }
        else
        {
            outSize[0] = (uint16_t)entryPoint->local_size.x;
            outSize[1] = (uint16_t)entryPoint->local_size.y;
            outSize[2] = (uint16_t)entryPoint->local_size.z;
        }

        if (logVerbose && (outSize[0] > 0u || outSize[1] > 0u || outSize[2] > 0u))
        {
            printf("    GroupSize: %i,%i,%i\n", outSize[0], outSize[1], outSize[2]);
        }
    }

    static void GetUniqueBindings(ReflectionData& reflection, PKShaderStage stage)
    {
        const auto stageIdx = (uint32_t)stage;
        auto* module = reflection.modules[stageIdx];

        uint32_t bindingCount = 0u;
        spvReflectEnumerateDescriptorBindings(module, &bindingCount, nullptr);

        std::vector<SpvReflectDescriptorBinding*> activeBindings(bindingCount);
        spvReflectEnumerateDescriptorBindings(module, &bindingCount, activeBindings.data());

        for (auto i = 0u; i < bindingCount; ++i)
        {
            auto* desc = activeBindings.at(i);

            if (!desc->accessed)
            {
                reflection.deadVariableIds[stageIdx].push_back(desc->spirv_id);
                continue;
            }

            auto name = ReflectBindingName(desc);
            auto& binding = reflection.uniqueBindings[name];
            binding.name = name;
            binding.accessStageMask |= 1u << stageIdx;
            binding.type = GetResourceType(desc->descriptor_type);
            binding.bindingIndex = max(binding.bindingIndex, desc->binding);
            binding.setIndex = 0u;
            binding.count = desc->type_description->op == SpvOpTypeRuntimeArray ? PKAssets::PK_ASSET_MAX_UNBOUNDED_SIZE : desc->count;

            const auto isWritten = ReflectResourceWrite(module->_internal->spirv_code, module->_internal->spirv_word_count, desc->spirv_id, binding.type);
            
            if (isWritten)
            {
                binding.writeStageMask |= 1u << stageIdx;
            }

            binding.wordOffsetBinding[stageIdx] = desc->word_offset.binding;
            binding.wordOffsetSet[stageIdx] = desc->word_offset.set;

            if (reflection.logVerbose)
            {
                printf("    Resource %s: %s \n", isWritten ? "Write" : "Read", name.c_str());
            }
        }
    }

    static void CompressBindIndices(ReflectionData& reflection)
    {
        reflection.sortedBindings.clear();
        reflection.sortedBindings.reserve(reflection.uniqueBindings.size());

        for (auto& kv : reflection.uniqueBindings)
        {
            kv.second.setIndex = 0u;
            reflection.sortedBindings.push_back(kv.second);
        }

        std::sort(reflection.sortedBindings.begin(), reflection.sortedBindings.end(), [](const ReflectBinding& a, const ReflectBinding& b)
        {
            const bool aUnbounded = a.count >= PK_ASSET_MAX_UNBOUNDED_SIZE;
            const bool bUnbounded = b.count >= PK_ASSET_MAX_UNBOUNDED_SIZE;

            if (aUnbounded != bUnbounded)
            {
                return aUnbounded < bUnbounded;
            }

            return a.name < b.name;
        });

        uint32_t bindingCounter = 0u;

        for (auto& binding : reflection.sortedBindings)
        {
            const uint32_t bindId = bindingCounter++;

            for (auto i = 0u; i < (int)PKShaderStage::EnumCount; ++i)
            {
                if (binding.wordOffsetBinding[i] != 0u)
                {
                    reflection.modules[i]->_internal->spirv_code[binding.wordOffsetBinding[i]] = bindId;
                }

                if (binding.wordOffsetSet[i] != 0u)
                {
                    reflection.modules[i]->_internal->spirv_code[binding.wordOffsetSet[i]] = 0u;
                }
            }
        }
    }


    int WriteShader(const char* pathSrc, const char* pathDst, const size_t pathStemOffset)
    {
        std::string source;
        
        auto lastWriteTime = PKVersionUtilities::GetLastWriteTime(pathDst);
        auto isOutOfDate = PKVersionUtilities::IsFileOutOfDate(pathSrc, pathDst);
        isOutOfDate |= ReadFile(pathSrc, source, lastWriteTime);

        if (!isOutOfDate)
        {
            return 1;
        }

        auto filename = StringUtilities::ReadFileName(pathSrc);
        auto buffer = PKAssetBuffer();
        buffer.header->type = PKAssetType::Shader;
        WriteName(buffer.header->name, filename.c_str());

        auto shader = buffer.Allocate<PKShader>();

        ShaderCompiler compiler;
        std::vector<std::string> variantDefines;
        std::vector<PKShaderKeyword> keywords;
        std::vector<PKMaterialProperty> materialProperties;
        std::vector<EntryPointInfo> entryPoints;
        auto enableInstancing = false;
        auto nofragInstancing = false;
        auto generateDebugInfo = false;
        auto logVerbose = false;
        StringUtilities::ConvertTabsToSpaces(source);
        ExtractLogVerbose(source, &logVerbose);
        ExtractGenerateDebugInfo(source, &generateDebugInfo);

        CompileOptions options;
        options.SetAutoBindUniforms(true);
        options.SetAutoMapLocations(true);
        options.SetTargetEnvironment(shaderc_target_env_default, shaderc_env_version_vulkan_1_4);
        options.SetTargetSpirv(shaderc_spirv_version_1_6);
        options.SetOptimizationLevel(shaderc_optimization_level::shaderc_optimization_level_performance);
        options.SetGenerateDebugInfo();

        // Sadly this happens after includes :/
        if (logVerbose)
        {
            printf("Preprocessing shader: %s \n", filename.c_str());
        }

        ExtractMulticompiles(source, variantDefines, keywords);
        ExtractStateAttributes(source, &shader->attributes);
        Instancing::InsertMaterialAssembly(source, materialProperties, &enableInstancing, &nofragInstancing);

        if (ExtractEntryPoints(source, entryPoints) != 0)
        {
            return -1;
        }

        shader->variantcount = (uint32_t)variantDefines.size();
        shader->keywordCount = (uint32_t)keywords.size();
        shader->materialPropertyCount = (uint32_t)materialProperties.size();

        if (keywords.size() > 0)
        {
            auto pKeywords = buffer.Write<PKShaderKeyword>(keywords.data(), keywords.size());
            shader->keywords.Set(buffer.data(), pKeywords.get());
        }

        if (materialProperties.size() > 0)
        {
            auto pMaterialProperties = buffer.Write<PKMaterialProperty>(materialProperties.data(), materialProperties.size());
            shader->materialProperties.Set(buffer.data(), pMaterialProperties.get());
        }

        auto pVariants = buffer.Allocate<PKShaderVariant>(shader->variantcount);
        shader->variants.Set(buffer.data(), pVariants.get());

        for (uint32_t variantIndex = 0; variantIndex < shader->variantcount; ++variantIndex)
        {
            EntryPointInfo* stageEntries[(uint32_t)PKShaderStage::EnumCount]{};
            std::string stageSources[(uint32_t)PKShaderStage::EnumCount]{};
            SourcePushConstants sourceConstants;

            // 1. Preprocess & convert HLSL constructs per stage
            for (auto stageIndex = 0u; stageIndex < (uint32_t)PKShaderStage::EnumCount; ++stageIndex)
            {
                auto entryIndex = FindActiveEntryPointIndexForStage(entryPoints, variantDefines[variantIndex], (PKShaderStage)stageIndex);

                if (entryIndex == -1)
                {
                    continue;
                }

                auto& entry = entryPoints.at(entryIndex);
                stageEntries[stageIndex] = &entryPoints.at(entryIndex);
                stageSources[stageIndex] = std::string(source);

                if (RemoveEntryPointLocals(stageSources[stageIndex], entry.name, entry.stage) != 0)
                {
                    printf("Failed to remove entry point locals! \n");
                    return -1;
                }

                SetActiveEntryPoint(stageSources[stageIndex], entryPoints, entryIndex);
                Instancing::InsertEntryPoint(stageSources[stageIndex], entry.stage, enableInstancing, nofragInstancing);
                stageSources[stageIndex].insert(0, std::string("#define PK_ACTIVE_ENTRY_POINT_") + entry.name + "\n");
                stageSources[stageIndex].insert(0, PK_SHADER_STAGE_DEFINES[(uint32_t)entry.stage]);
                stageSources[stageIndex].insert(0, variantDefines[variantIndex]);
                InsertRequiredExtensions(stageSources[stageIndex], entry.stage);
                ConvertPrintf(stageSources[stageIndex]);
                ProcessShaderVersion(stageSources[stageIndex]);

                if (PreprocessGLSL(compiler, options, entry.stage, entry.name, stageSources[stageIndex]) != 0)
                {
                    return -1;
                }

                ConvertHLSLTypesToGLSL(stageSources[stageIndex]);
                ConvertHLSLBuffers(stageSources[stageIndex]);
                ConvertHLSLTextures(stageSources[stageIndex]);
                ConvertHLSLCBuffers(stageSources[stageIndex]);
                ConvertPKNumThreads(stageSources[stageIndex]);
                RemoveDescriptorSets(stageSources[stageIndex]);
                RemoveUnsupportedRayTracingFields(stageSources[stageIndex], entry.stage);
                RemoveInactiveGroupSizeLayouts(stageSources[stageIndex], entry.stage);
            }

            // 2. Drop unused inter-stage outputs, then strip dead functions/resources and extract push constants
            RemoveUnusedStageOutputs(stageSources);

            for (auto stageIndex = 0u; stageIndex < (uint32_t)PKShaderStage::EnumCount; ++stageIndex)
            {
                if (stageEntries[stageIndex] != nullptr)
                {
                    RemoveUnusedResourcesAndFunctions(stageSources[stageIndex]);
                    ExtractPushConstants(stageSources[stageIndex], (PKShaderStage)stageIndex, sourceConstants);
                }
            }

            CompilePushConstantBlock(stageSources, sourceConstants);
            AddLineCounter(stageSources);

            ReflectionData reflectionData{};
            reflectionData.logVerbose = logVerbose;

            // 3. Single-pass SPIR-V compilation
            for (auto stageIndex = 0u; stageIndex < (uint32_t)PKShaderStage::EnumCount; ++stageIndex)
            {
                if (stageEntries[stageIndex] != nullptr)
                {
                    const auto& entry = *stageEntries[stageIndex];
                    const auto& stageSource = stageSources[stageIndex];

                    if (logVerbose)
                    {
                        printf("Compiling %s:%i %s \n", entry.name.c_str(), variantIndex, PK_SHADER_STAGE_NAMES[stageIndex]);
                    }

                    auto* module = CompileToSPIRV(compiler, options, entry.stage, entry.name, stageSource);
                    if (module == nullptr)
                    {
                        ReleaseReflectionData(reflectionData);
                        return -1;
                    }

                    reflectionData.modules[stageIndex] = module;
                    GetUniqueBindings(reflectionData, entry.stage);
                    GetVertexAttributes(reflectionData, entry.stage);
                    GetPushConstants(reflectionData, entry.stage);
                    GetComputeGroupSize(reflectionData, entry.stage, pVariants[variantIndex].groupSize, logVerbose);
                }
            }

            CompressBindIndices(reflectionData);

            for (auto stageIndex = 0u; stageIndex < (uint32_t)PKShaderStage::EnumCount; ++stageIndex)
            {
                if (reflectionData.modules[stageIndex])
                {
                    const auto* mod = reflectionData.modules[stageIndex];
                    const auto cleanedSpirv = CleanSPIRV(
                        spvReflectGetCode(mod),
                        (uint32_t)(spvReflectGetCodeSize(mod) / sizeof(uint32_t)),
                        reflectionData.deadVariableIds[stageIndex],
                        !generateDebugInfo);

                    auto pSpirv = buffer.Write(cleanedSpirv.data(), cleanedSpirv.size());
                    pVariants[variantIndex].sprivSizes[stageIndex] = (uint32_t)(cleanedSpirv.size() * sizeof(uint32_t));
                    pVariants[variantIndex].sprivBuffers[stageIndex].Set(buffer.data(), pSpirv.get());
                }
            }

            if (reflectionData.vertexAttributes.size() > 0)
            {
                pVariants[variantIndex].vertexAttributeCount = (uint16_t)reflectionData.vertexAttributes.size();
                auto pVertexAttributes = buffer.Write<PKVertexInputAttribute>(reflectionData.vertexAttributes.data(), reflectionData.vertexAttributes.size());
                pVariants[variantIndex].vertexAttributes.Set(buffer.data(), pVertexAttributes.get());
            }

            if (reflectionData.uniqueConstants.size() > 0)
            {
                pVariants[variantIndex].constantCount = (uint16_t)reflectionData.uniqueConstants.size();
                pVariants[variantIndex].constantRange = (uint16_t)reflectionData.constantRangeSize;
                auto pConstantVariables = buffer.Allocate<PKConstantVariable>(reflectionData.uniqueConstants.size());
                pVariants[variantIndex].constants.Set(buffer.data(), pConstantVariables.get());

                auto j = 0u;
                for (auto& kv : reflectionData.uniqueConstants)
                {
                    if (j >= PK_ASSET_MAX_PUSH_CONSTANTS)
                    {
                        printf("Warning! Shader has more push constants than supported (%i / %i) \n", j, PK_ASSET_MAX_PUSH_CONSTANTS);
                        continue;
                    }

                    WriteName(pConstantVariables[j].name, kv.first.c_str());
                    pConstantVariables[j].offset = kv.second.offset;
                    pConstantVariables[j++].size = kv.second.size;
                }
            }

            pVariants[variantIndex].descriptorCount = (uint16_t)reflectionData.sortedBindings.size();

            if (pVariants[variantIndex].descriptorCount > 0)
            {
                auto pDescriptors = buffer.Allocate<PKDescriptor>(pVariants[variantIndex].descriptorCount);
                pVariants[variantIndex].descriptors.Set(buffer.data(), pDescriptors.get());

                if (pVariants[variantIndex].descriptorCount > PK_ASSET_MAX_DESCRIPTORS_PER_SET)
                {
                    printf("Warning! Shader has a descriptors outside of supported range (%i / %i) \n", pVariants[variantIndex].descriptorCount, PK_ASSET_MAX_DESCRIPTORS_PER_SET);
                }

                for (auto j = 0u; j < reflectionData.sortedBindings.size(); ++j)
                {
                    auto& binding = reflectionData.sortedBindings.at(j);
                    pDescriptors[j].type = binding.type;
                    pDescriptors[j].__padding = 0u;
                    pDescriptors[j].count = binding.count;
                    pDescriptors[j].writeMask = (PKShaderStageFlags)binding.writeStageMask;
                    pDescriptors[j].accessMask = (PKShaderStageFlags)binding.accessStageMask;
                    WriteName(pDescriptors[j].name, binding.name.c_str());
                }
            }

            ReleaseReflectionData(reflectionData);
        }

        return WriteAsset(pathDst, pathStemOffset, buffer, false);
    }
}