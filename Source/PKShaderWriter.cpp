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

            if (dcount >= PK_ASSET_MAX_SHADER_DIRECTIVES)
            {
                printf("Warning maximum number of shader multicompile directives reached! consider combining directives.");
                continue;
            }

            auto directive = StringUtilities::Split(output, " ");

            for (auto i = 0u; i < directive.size(); ++i)
            {
                auto& keyword = directive.at(i);

                if (keyword != "_")
                {
                    PKShaderKeyword pkkw{};
                    pkkw.offsets = (uint32_t)((((dcount << 4) | (i & 0xF)) << 24) | (vcount & 0xFFFFFF));
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

            auto stage = PKAssets::StringToPKShaderStage(directives[0].c_str());

            if (stage == PKShaderStage::MaxCount)
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
            int minLine, maxLine;
            FindLineRange(name, result.GetErrorMessage(), &minLine, &maxLine);

            #if defined(WIN32)
            SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 12);
            #endif

            printf("\n ----------BEGIN ERROR---------- \n");
            printf("\n Stage: %s\n\n", PK_SHADER_STAGE_NAMES[(uint32_t)stage]);
            printf("%s", result.GetErrorMessage().c_str());
            printf("\n");

            std::istringstream iss(source);
            auto index = 0;
            const auto linePadding = 5;
            minLine -= linePadding;
            maxLine += linePadding;

            for (std::string line; std::getline(iss, line);)
            {
                if (index > minLine && index < maxLine)
                {
                    printf("%i: %s \n", index, line.c_str());
                }

                index++;
            }

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
        for (auto i = 0u; i < (int)PKShaderStage::MaxCount; ++i)
        {
            if (reflection.modulesRel[i])
            {
                spvReflectDestroyShaderModule(reflection.modulesRel[i]);
                spvReflectDestroyShaderModule(reflection.modulesDeb[i]);
                delete reflection.modulesRel[i];
                delete reflection.modulesDeb[i];
            }
        }
    }

    static void GetVertexAttributes(ReflectionData& reflection, PKShaderStage stage)
    {
        if (stage != PKShaderStage::Vertex)
        {
            return;
        }

        auto* moduleDeb = reflection.modulesDeb[(uint32_t)stage];

        auto count = 0u;
        spvReflectEnumerateEntryPointInputVariables(moduleDeb, moduleDeb->entry_point_name, &count, nullptr);

        std::vector<SpvReflectInterfaceVariable*> variables;
        variables.resize(count);

        spvReflectEnumerateEntryPointInputVariables(moduleDeb, moduleDeb->entry_point_name, &count, variables.data());

        if (count > 0 && reflection.logVerbose)
        {
            printf("    Interface:");
        }

        for (auto* variable : variables)
        {
            // Ignore built in variables
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
        auto stageFlag = (1u << (uint32_t)stage); // PKShaderStageFlags
        const auto* moduleRel = reflection.modulesRel[(uint32_t)stage];
        const auto* moduleDeb = reflection.modulesDeb[(uint32_t)stage];
        const auto* blockDeb = spvReflectGetEntryPointPushConstantBlock(moduleDeb, "main", nullptr);
        const auto* blockRel = spvReflectGetEntryPointPushConstantBlock(moduleRel, "main", nullptr);

        if (blockDeb == nullptr || blockRel == nullptr)
        {
            return;
        }

        if (reflection.logVerbose)
        {
            printf("    Constants:");
        }

        reflection.constantRangeStageFlags |= stageFlag;
        reflection.constantRangeSize = blockRel->size;

        for (auto i = 0u; i < blockDeb->member_count; ++i)
        {
            const auto memberDeb = blockDeb->members + i;
            const auto memberRel = blockRel->members + i;

            if ((memberDeb->flags & SPV_REFLECT_VARIABLE_FLAGS_UNUSED) && (memberRel->flags & SPV_REFLECT_VARIABLE_FLAGS_UNUSED))
            {
                continue;
            }

            const auto name = std::string(memberDeb->name);
            reflection.uniqueConstants[name] = { (uint16_t)memberRel->absolute_offset, (uint16_t)memberRel->size };
                
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
        auto moduleDeb = reflection.modulesDeb[(uint32_t)stage];
        auto entryPoint = moduleDeb->entry_points;

        uint32_t size[3];

        // Spv reflect has a bug that doesnt correctly reflect the entry point size for all execution modes.
        // Manually reflect alternative execution modes first. if not found assume Spv succeeded.
        if (ReflectLocalSize(moduleDeb->_internal->spirv_code, moduleDeb->_internal->spirv_word_count, size))
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
        auto* moduleRel = reflection.modulesRel[(uint32_t)stage];
        auto* moduleDeb = reflection.modulesDeb[(uint32_t)stage];

        uint32_t bindingCount = 0u;

        spvReflectEnumerateDescriptorBindings(moduleDeb, &bindingCount, nullptr);

        std::vector<SpvReflectDescriptorBinding*> activeBindings;
        activeBindings.resize(bindingCount);
        spvReflectEnumerateDescriptorBindings(moduleDeb, &bindingCount, activeBindings.data());
        
        for (auto i = 0u; i < bindingCount; ++i)
        {
            auto desc = activeBindings.at(i);

            if (!desc->accessed)
            {
                continue;
            }

            auto releaseBinding = spvReflectGetDescriptorBinding(moduleRel, desc->binding, desc->set, nullptr);

            if (releaseBinding == nullptr)
            {
                continue;
            }

            auto name = ReflectBindingName(desc);
            auto& binding = reflection.uniqueBindings[name];
            binding.firstStage = binding.firstStage > (uint32_t)stage ? (int)stage : binding.firstStage;
            binding.maxBinding = binding.maxBinding > releaseBinding->binding ? binding.maxBinding : releaseBinding->binding;
            binding.name = name;
            binding.count = desc->type_description->op == SpvOpTypeRuntimeArray ? PKAssets::PK_ASSET_MAX_UNBOUNDED_SIZE : desc->count;
            auto isWritten = ReflectResourceWrite(moduleDeb->_internal->spirv_code, moduleDeb->_internal->spirv_word_count, desc->spirv_id, GetResourceType(desc->descriptor_type));

            if (isWritten)
            {
                binding.writeStageMask |= 1 << (int)stage;
            }

            binding.bindings[(int)stage] = releaseBinding;

            if (reflection.logVerbose)
            {
                printf("    Resource %s: %s \n", isWritten ? "Write" : "Read", name.c_str());
            }
        }
    }

    static void CompressBindIndices(ReflectionData& reflection)
    {
        std::multimap<uint32_t, ReflectBinding> sortedBindingMap;

        for (auto& kv : reflection.uniqueBindings)
        {
            // Ensure bind arrays are last bindings
            const auto sortValue = kv.second.count == PK_ASSET_MAX_UNBOUNDED_SIZE ? 0xFFFFFFFFu : kv.second.maxBinding;
            sortedBindingMap.insert(std::make_pair(sortValue, kv.second));
        }

        reflection.sortedBindings.clear();

        for (auto& kv : sortedBindingMap)
        {
            reflection.sortedBindings.push_back(kv.second);
        }

        uint32_t bindingCounter = 0u;

        for (auto& binding : reflection.sortedBindings)
        {
            auto bindId = bindingCounter++;

            for (auto i = 0u; i < (int)PKShaderStage::MaxCount; ++i)
            {
                if (binding.bindings[i] != nullptr)
                {
                    spvReflectChangeDescriptorBindingNumbers(reflection.modulesRel[i], binding.bindings[i], bindId, 0u);
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

        CompileOptions optionsDebug;
        optionsDebug.SetAutoBindUniforms(true);
        optionsDebug.SetAutoMapLocations(true);
        optionsDebug.SetTargetEnvironment(shaderc_target_env_default, shaderc_env_version_vulkan_1_4);
        optionsDebug.SetTargetSpirv(shaderc_spirv_version_1_6);

        CompileOptions optionsRelease;
        optionsRelease.SetAutoBindUniforms(true);
        optionsRelease.SetAutoMapLocations(true);
        optionsRelease.SetTargetEnvironment(shaderc_target_env_default, shaderc_env_version_vulkan_1_4);
        optionsRelease.SetTargetSpirv(shaderc_spirv_version_1_6);
        optionsRelease.SetOptimizationLevel(shaderc_optimization_level::shaderc_optimization_level_performance);

        if (generateDebugInfo)
        {
            optionsRelease.SetGenerateDebugInfo();
        }

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
            EntryPointInfo* stageEntries[(uint32_t)PKShaderStage::MaxCount]{};
            std::string stageSources[(uint32_t)PKShaderStage::MaxCount]{};
            SourcePushConstants sourceConstants;

            // Preprocess GLSL source text.
            for (auto stageIndex = 0u; stageIndex < (uint32_t)PKShaderStage::MaxCount; ++stageIndex)
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
                ProcessShaderVersion(stageSources[stageIndex]);

                if (PreprocessGLSL(compiler, optionsDebug, entry.stage, entry.name, stageSources[stageIndex]) != 0)
                {
                    return -1;
                }

                ConvertHLSLTypesToGLSL(stageSources[stageIndex]);
                ConvertHLSLBuffers(stageSources[stageIndex]);
                ConvertHLSLCBuffers(stageSources[stageIndex]);
                ConvertPKNumThreads(stageSources[stageIndex]);
                RemoveDescriptorSets(stageSources[stageIndex]);
                RemoveUnsupportedRayTracingFields(stageSources[stageIndex], entry.stage);
                RemoveInactiveGroupSizeLayouts(stageSources[stageIndex], entry.stage);
                ExtractPushConstants(stageSources[stageIndex], entry.stage, sourceConstants);
            }

            CompilePushConstantBlock(stageSources, sourceConstants);

            ReflectionData reflectionData{};
            reflectionData.logVerbose = logVerbose;

            // Compile to SPRIV
            for (auto stageIndex = 0u; stageIndex < (uint32_t)PKShaderStage::MaxCount; ++stageIndex)
            {
                if (stageEntries[stageIndex] != nullptr)
                {
                    const auto& entry = *stageEntries[stageIndex];
                    const auto& stageSource = stageSources[stageIndex];

                    if (logVerbose)
                    {
                        printf("Compiling %s:%i %s \n", entry.name.c_str(), variantIndex, PK_SHADER_STAGE_NAMES[stageIndex]);
                    }

                    // Need to do double compile as debug mode will have variable names but release mode wont.
                    auto moduleDeb = CompileToSPIRV(compiler, optionsDebug, entry.stage, entry.name, stageSource);
                    auto moduleRel = moduleDeb ? CompileToSPIRV(compiler, optionsRelease, entry.stage, entry.name, stageSource) : nullptr;

                    if (moduleDeb == nullptr)
                    {
                        return -1;
                    }

                    reflectionData.modulesDeb[stageIndex] = moduleDeb;
                    reflectionData.modulesRel[stageIndex] = moduleRel;
                    GetUniqueBindings(reflectionData, entry.stage);
                    GetVertexAttributes(reflectionData, entry.stage);
                    GetPushConstants(reflectionData, entry.stage);
                    GetComputeGroupSize(reflectionData, entry.stage, pVariants[variantIndex].groupSize, logVerbose);
                }
            }

            CompressBindIndices(reflectionData);

            for (auto stageIndex = 0u; stageIndex < (uint32_t)PKShaderStage::MaxCount; ++stageIndex)
            {
                if (reflectionData.modulesRel[stageIndex])
                {
                    auto size = spvReflectGetCodeSize(reflectionData.modulesRel[stageIndex]);
                    auto code = spvReflectGetCode(reflectionData.modulesRel[stageIndex]);
                    auto pSpirv = buffer.Write(code, size / sizeof(uint32_t));
                    pVariants[variantIndex].sprivSizes[stageIndex] = size;
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
                pVariants[variantIndex].constantStageFlags = (PKShaderStageFlags)reflectionData.constantRangeStageFlags;

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
                    auto desc = binding.get();
                    pDescriptors[j].count = binding.count;
                    pDescriptors[j].type = GetResourceType(desc->descriptor_type);
                    pDescriptors[j].writeStageMask = (PKShaderStageFlags)binding.writeStageMask;
                    WriteName(pDescriptors[j].name, binding.name.c_str());
                }
            }

            ReleaseReflectionData(reflectionData);
        }

        return WriteAsset(pathDst, pathStemOffset, buffer, false);
    }
}