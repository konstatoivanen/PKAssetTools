#include <unordered_map>
#include <map>
#include <stdexcept>
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
#undef min
#undef max
#endif

namespace PKAssets::Shader
{
    static slang::CompilerOptionEntry MakeIntOption(slang::CompilerOptionName name, int32_t value)
    {
        slang::CompilerOptionEntry entry{};
        entry.name = name;
        entry.value.kind = slang::CompilerOptionValueKind::Int;
        entry.value.intValue0 = value;
        return entry;
    }
    
    static void AddCapabilityOption(slang::IGlobalSession* globalSession, std::vector<slang::CompilerOptionEntry>& options, const char* capabilityName)
    {
        const auto cap = globalSession->findCapability(capabilityName);

        if (cap != SLANG_CAPABILITY_UNKNOWN)
        {
            options.push_back(MakeIntOption(slang::CompilerOptionName::Capability, (int32_t)cap));
        }
    }
    
    static slang::ISession* GetSlangSession(bool generateDebugInfo, bool logDisassembly)
    {
        const auto sessionIndex = (generateDebugInfo ? 1u : 0u) | (logDisassembly ? 2u : 0u);
        static Slang::ComPtr<slang::ISession> s_sessions[4];
        static Slang::ComPtr<slang::IGlobalSession> s_globalSession;

        if (s_sessions[sessionIndex])
        {
            return s_sessions[sessionIndex].get();
        }

        if (!s_globalSession)
        {
            slang::createGlobalSession(s_globalSession.writeRef());
        }

        auto globalSession = s_globalSession.get();

        if (!globalSession)
        {
            return nullptr;
        }

        const auto debugLevel = generateDebugInfo ? SLANG_DEBUG_INFO_LEVEL_STANDARD : SLANG_DEBUG_INFO_LEVEL_NONE;

        std::vector<slang::CompilerOptionEntry> options =
        {
            MakeIntOption(slang::CompilerOptionName::AllowGLSL, 1),
            MakeIntOption(slang::CompilerOptionName::MatrixLayoutColumn, 1),
            MakeIntOption(slang::CompilerOptionName::GLSLForceScalarLayout, 1),
            MakeIntOption(slang::CompilerOptionName::DefaultImageFormatUnknown, 1),
            MakeIntOption(slang::CompilerOptionName::VulkanUseEntryPointName, 0),
            MakeIntOption(slang::CompilerOptionName::Optimization, SLANG_OPTIMIZATION_LEVEL_MAXIMAL),
            MakeIntOption(slang::CompilerOptionName::SPIRVUnifiedDescriptorHeapStride, 1),
            MakeIntOption(slang::CompilerOptionName::DebugInformation, debugLevel),
        };

        AddCapabilityOption(globalSession, options, "vk_mem_model");
        AddCapabilityOption(globalSession, options, "spvDescriptorHeapEXT");
        AddCapabilityOption(globalSession, options, "spvImageGatherExtended");
        AddCapabilityOption(globalSession, options, "GL_KHR_shader_subgroup_basic");
        AddCapabilityOption(globalSession, options, "GL_KHR_shader_subgroup_vote");
        AddCapabilityOption(globalSession, options, "GL_KHR_shader_subgroup_arithmetic");
        AddCapabilityOption(globalSession, options, "GL_KHR_shader_subgroup_ballot");
        AddCapabilityOption(globalSession, options, "GL_KHR_shader_subgroup_shuffle");
        AddCapabilityOption(globalSession, options, "GL_KHR_shader_subgroup_quad");
        AddCapabilityOption(globalSession, options, "GL_EXT_shader_quad_control");
        AddCapabilityOption(globalSession, options, "GL_EXT_samplerless_texture_functions");
        AddCapabilityOption(globalSession, options, "GL_EXT_shader_16bit_storage");
        AddCapabilityOption(globalSession, options, "GL_EXT_shader_8bit_storage");
        AddCapabilityOption(globalSession, options, "GL_EXT_scalar_block_layout");
        AddCapabilityOption(globalSession, options, "GL_EXT_shader_image_load_formatted");
        AddCapabilityOption(globalSession, options, "GL_EXT_shader_explicit_arithmetic_types");
        AddCapabilityOption(globalSession, options, "GL_EXT_shader_explicit_arithmetic_types_int8");
        AddCapabilityOption(globalSession, options, "GL_EXT_nonuniform_qualifier");
        AddCapabilityOption(globalSession, options, "GL_EXT_control_flow_attributes");
        AddCapabilityOption(globalSession, options, "GL_EXT_ray_tracing");
        AddCapabilityOption(globalSession, options, "GL_EXT_ray_tracing_position_fetch");
        AddCapabilityOption(globalSession, options, "GL_EXT_mesh_shader");
        AddCapabilityOption(globalSession, options, "GL_EXT_debug_printf");

        slang::TargetDesc targets[2]{};
        targets[0].format = SLANG_SPIRV;
        targets[0].profile = globalSession->findProfile("spirv_1_6");
        targets[0].flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;
        targets[0].forceGLSLScalarBufferLayout = true;
        targets[0].compilerOptionEntries = options.data();
        targets[0].compilerOptionEntryCount = (uint32_t)options.size();

        targets[1] = targets[0];
        targets[1].format = SLANG_SPIRV_ASM;

        slang::SessionDesc sessionDesc{};
        sessionDesc.targets = targets;
        sessionDesc.targetCount = logDisassembly ? 2 : 1;
        sessionDesc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;
        sessionDesc.allowGLSLSyntax = true;

        globalSession->createSession(sessionDesc, s_sessions[sessionIndex].writeRef());
        return s_sessions[sessionIndex].get();
    }


    static void PrintError(slang::IBlob* diagnostics, const std::string& name, const std::string& source, const char* header, bool formatLines)
    {
        if (!diagnostics || diagnostics->getBufferSize() == 0)
        {
            return;
        }

        const std::string errorStr(static_cast<const char*>(diagnostics->getBufferPointer()), diagnostics->getBufferSize());

        #if defined(WIN32)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 12);
        #endif

        printf("\n ----------BEGIN ERROR---------- \n");
        printf("\n %s (%s)\n\n", header, name.c_str());
        printf("%s", (formatLines ? FormatErrorMessage(name, source, errorStr) : errorStr).c_str());
        printf("\n ----------END ERROR---------- \n\n");

        #if defined(WIN32)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 15);
        #endif
    }

    static bool ReadFile(const std::string& filepath, std::string& ouput, std::filesystem::file_time_type lastDestWriteTime)
    {
        std::vector<std::string> includes;
        ouput = StringUtilities::ReadFileRecursiveInclude(filepath, includes);
        return PKVersionUtilities::IsFileAnyOutOfDate(includes, lastDestWriteTime);
    }

    static void ReleaseReflectionData(ReflectionData& reflection)
    {
        for (auto i = 0u; i < (int)PKShaderStage::EnumCount; ++i)
        {
            if (reflection.modules[i])
            {
                spvReflectDestroyShaderModule(reflection.modules[i]);
                delete reflection.modules[i];
                reflection.modules[i] = nullptr;
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

            std::string attrName = variable->name ? variable->name : "";
            const auto dotPos = attrName.find_last_of('.');
            if (dotPos != std::string::npos)
            {
                attrName = attrName.substr(dotPos + 1ull);
            }

            PKVertexInputAttribute attribute{};
            WriteName(attribute.name, attrName.c_str());
            attribute.location = variable->location;
            attribute.type = SpvFormatToPKUniform(variable->format);
            reflection.vertexAttributes.push_back(attribute);

            if (reflection.logVerbose)
            {
                printf(" %s ", attrName.c_str());
            }
        }

        if (count > 0 && reflection.logVerbose)
        {
            printf("\n");
        }
    }

    static void GetPushConstants(ReflectionData& reflection, PKShaderStage stage)
    {
        const auto stageIdx = (uint32_t)stage;
        const auto* module = reflection.modules[stageIdx];
        const auto* block = spvReflectGetEntryPointPushConstantBlock(module, module->entry_point_name, nullptr);

        if (block == nullptr)
        {
            return;
        }

        reflection.pushConstantStructIds[stageIdx] = block->type_description ? block->type_description->id : 0u;
        reflection.constantRangeSize = std::max(reflection.constantRangeSize, block->size);

        const auto memberCount = std::min((size_t)block->member_count, reflection.sourceConstants.size());

        for (auto i = 0u; i < memberCount; ++i)
        {
            const auto* member = block->members + i;

            if (member->flags & SPV_REFLECT_VARIABLE_FLAGS_UNUSED)
            {
                continue;
            }

            auto& constant = reflection.sourceConstants[i];
            constant.accessMask |= 1u << stageIdx;

            if (ReflectPushConstantResourceWrite(module->_internal->spirv_code, module->_internal->spirv_word_count, block->spirv_id, i, constant.type))
            {
                constant.writeMask |= 1u << stageIdx;
            }
        }
    }

    static void CompactPushConstants(ReflectionData& reflection)
    {
        if (reflection.sourceConstants.empty() || reflection.constantRangeSize == 0u)
        {
            return;
        }

        std::vector<uint32_t> memberOffsets(reflection.sourceConstants.size(), 0u);
        reflection.activeConstants.clear();
        reflection.activeConstants.reserve(reflection.sourceConstants.size());

        auto currentOffset = 0u;

        // Place active constants first (preserving alignment/size sort order)
        for (auto i = 0u; i < reflection.sourceConstants.size(); ++i)
        {
            auto& constant = reflection.sourceConstants[i];

            if (constant.accessMask)
            {
                const auto align = PKUniformToScalarSize(constant.type);
                const auto size = PKUniformToSize(constant.type) * constant.count;
                currentOffset = (currentOffset + align - 1u) & ~(align - 1u);

                constant.offset = (uint8_t)currentOffset;
                memberOffsets[i] = currentOffset;
                currentOffset += size;

                reflection.activeConstants.push_back(constant);

                if (reflection.logVerbose)
                {
                    printf("    Constant: %s, %u, %s %s\n", constant.writeMask ? "Write" : "Read", constant.offset, constant.targetType.c_str(), constant.name.c_str());
                }
            }
        }

        // Move dead constants to the tail of the SPIR-V block
        for (auto i = 0u; i < reflection.sourceConstants.size(); ++i)
        {
            const auto& constant = reflection.sourceConstants[i];
            
            if (constant.accessMask == 0u)
            {
                const auto align = PKUniformToScalarSize(constant.type);
                const auto size = PKUniformToSize(constant.type) * constant.count;
                currentOffset = (currentOffset + align - 1u) & ~(align - 1u);

                if (reflection.logVerbose)
                {
                    printf("    Constant: Unused, %u, %s %s\n", currentOffset, constant.targetType.c_str(), constant.name.c_str());
                }

                memberOffsets[i] = currentOffset;
                currentOffset += size;
            }
        }

        reflection.constantRangeSize = std::max(reflection.constantRangeSize, currentOffset);

        for (auto stageIdx = 0u; stageIdx < (uint32_t)PKShaderStage::EnumCount; ++stageIdx)
        {
            if (reflection.modules[stageIdx] && reflection.pushConstantStructIds[stageIdx] != 0u)
            {
                PatchPushConstantMemberOffsets(
                    reflection.modules[stageIdx]->_internal->spirv_code,
                    reflection.modules[stageIdx]->_internal->spirv_word_count,
                    reflection.pushConstantStructIds[stageIdx],
                    memberOffsets);
            }
        }
    }

    static void GetComputeGroupSize(ReflectionData& reflection, PKShaderStage stage)
    {
        auto* module = reflection.modules[(uint32_t)stage];
        auto* entryPoint = module->entry_points;

        uint32_t size[3];

        if (ReflectLocalSize(module->_internal->spirv_code, module->_internal->spirv_word_count, size))
        {
            reflection.groupSize[0] = (uint16_t)size[0];
            reflection.groupSize[1] = (uint16_t)size[1];
            reflection.groupSize[2] = (uint16_t)size[2];
        }
        else
        {
            reflection.groupSize[0] = (uint16_t)entryPoint->local_size.x;
            reflection.groupSize[1] = (uint16_t)entryPoint->local_size.y;
            reflection.groupSize[2] = (uint16_t)entryPoint->local_size.z;
        }

        if (reflection.logVerbose && (reflection.groupSize[0] || reflection.groupSize[1] || reflection.groupSize[2]))
        {
            printf("    GroupSize: %i,%i,%i\n", reflection.groupSize[0], reflection.groupSize[1], reflection.groupSize[2]);
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

            // Skip BuiltIn ResourceHeapEXT / SamplerHeapEXT variables (no Binding decoration)
            if (desc->word_offset.binding == 0u)
            {
                continue;
            }

            if (!desc->accessed)
            {
                reflection.deadVariableIds[stageIdx].insert(desc->spirv_id);
                continue;
            }
            
            if (desc->type_description->op == SpvOpTypeRuntimeArray || desc->count == 0u)
            {
                printf("Warning: Trying to use an unbounded runtime array. This is no longer supported, please use resource/sampler heaps instead.");
                continue;
            }

            auto name = ReflectBindingName(desc);
            auto& binding = reflection.uniqueBindings[name];
            binding.name = name;
            binding.accessStageMask |= 1u << stageIdx;
            binding.type = GetResourceType(desc->descriptor_type);
            binding.bindingIndex = std::max(binding.bindingIndex, desc->binding);
            binding.count = desc->count;

            const auto isWritten = ReflectResourceWrite(module->_internal->spirv_code, module->_internal->spirv_word_count, desc->spirv_id, binding.type);

            if (isWritten)
            {
                binding.writeStageMask |= 1u << stageIdx;
            }

            binding.wordOffsetBinding[stageIdx] = desc->word_offset.binding;
            binding.wordOffsetSet[stageIdx] = desc->word_offset.set;
        }
    }

    static void CompressBindIndices(ReflectionData& reflection)
    {
        reflection.sortedBindings.clear();
        reflection.sortedBindings.reserve(reflection.uniqueBindings.size());

        for (auto& kv : reflection.uniqueBindings)
        {
            reflection.sortedBindings.push_back(kv.second);
        }

        std::sort(reflection.sortedBindings.begin(), reflection.sortedBindings.end(), [](const ReflectBinding& a, const ReflectBinding& b) { return a.name < b.name; });

        auto bindingCounter = 0u;

        for (auto& binding : reflection.sortedBindings)
        {
            const auto bindId = bindingCounter++;

            if (reflection.logVerbose)
            {
                const auto* typeName = PKUniform_NAMES[(uint32_t)binding.type];
                printf("    Resource: %s, %s %s\n", binding.writeStageMask ? "Write" : "Read", typeName, binding.name.c_str());
            }

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

    static bool CompileVariantToSPIRV(const std::string& name, const std::string& variantSource, EntryPointInfo** stageEntries, ReflectionData& reflectionData)
    {
        auto* session = GetSlangSession(reflectionData.generateDebugInfo, reflectionData.logDisassembly);

        if (!session)
        {
            return false;
        }

        static auto s_moduleCounter = 0ull;
        const auto uniqueName = name + "_" + std::to_string(s_moduleCounter++);

        Slang::ComPtr<slang::IBlob> diagnostics;
        Slang::ComPtr<slang::IModule> module(session->loadModuleFromSourceString(uniqueName.c_str(), uniqueName.c_str(), variantSource.c_str(), diagnostics.writeRef()));
        std::vector<Slang::ComPtr<slang::IEntryPoint>> entryPoints;
        std::vector<uint32_t> activeStages;

        if (!module)
        {
            PrintError(diagnostics, uniqueName, variantSource, "Source load failed:", true);
            return false;
        }

        for (auto stageIndex = 0u; stageIndex < (uint32_t)PKShaderStage::EnumCount; ++stageIndex)
        {
            if (stageEntries[stageIndex])
            {
                const auto& entry = *stageEntries[stageIndex];

                if (reflectionData.logVerbose)
                {
                    printf("Compiling %s %s \n", entry.name.c_str(), PKShaderStage_NAMES[stageIndex]);
                }

                Slang::ComPtr<slang::IEntryPoint> entryPoint;
                diagnostics = nullptr;
                module->findAndCheckEntryPoint(entry.name.c_str(), PKShaderStageToSlangStage(entry.stage), entryPoint.writeRef(), diagnostics.writeRef());

                if (!entryPoint)
                {
                    PrintError(diagnostics, name, variantSource, "Compilation failed:", false);
                    return false;
                }

                entryPoints.push_back(entryPoint);
                activeStages.push_back(stageIndex);
            }
        }

        for (auto i = 0u; i < activeStages.size(); ++i)
        {
            slang::IComponentType* stageComponents[2] = { module.get(), entryPoints[i].get() };
            Slang::ComPtr<slang::IComponentType> composedProgram;
            Slang::ComPtr<slang::IComponentType> linkedProgram;

            diagnostics = nullptr;
            session->createCompositeComponentType(stageComponents, 2, composedProgram.writeRef(), diagnostics.writeRef());
            if (!composedProgram)
            {
                PrintError(diagnostics, name, variantSource, "Composition failed:", false);
                return false;
            }

            diagnostics = nullptr;
            composedProgram->link(linkedProgram.writeRef(), diagnostics.writeRef());

            if (!linkedProgram)
            {
                PrintError(diagnostics, name, variantSource, "Linking failed:", false);
                return false;
            }

            Slang::ComPtr<slang::IBlob> spirvBlob;
            diagnostics = nullptr;
            linkedProgram->getEntryPointCode(0, 0, spirvBlob.writeRef(), diagnostics.writeRef());

            if (!spirvBlob || !spirvBlob->getBufferSize())
            {
                PrintError(diagnostics, name, variantSource, "SPIRV emission failed:", false);
                return false;
            }

            const auto stageIndex = activeStages[i];
            const auto stage = (PKShaderStage)stageIndex;

            auto* reflectModule = new SpvReflectShaderModule();
            const auto codeStatus = spvReflectCreateShaderModule(spirvBlob->getBufferSize(), spirvBlob->getBufferPointer(), reflectModule);

            if (codeStatus != SPV_REFLECT_RESULT_SUCCESS)
            {
                printf("Failed to extract reflection data from SPIR-V module!\n");
                delete reflectModule;
                return false;
            }

            if (reflectionData.logDisassembly)
            {
                Slang::ComPtr<slang::IBlob> spirvAsmBlob;
                linkedProgram->getEntryPointCode(0, 1, spirvAsmBlob.writeRef(), diagnostics.writeRef());
                if (spirvAsmBlob)
                {
                    printf("DISASSEMBLY:\n %.*s\n", (int)spirvAsmBlob->getBufferSize(), static_cast<const char*>(spirvAsmBlob->getBufferPointer()));
                }
            }

            reflectionData.modules[stageIndex] = reflectModule;
            GetUniqueBindings(reflectionData, stage);
            GetVertexAttributes(reflectionData, stage);
            GetPushConstants(reflectionData, stage);
            GetComputeGroupSize(reflectionData, stage);
        }

        return true;
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

        std::vector<std::string> variantDefines;
        std::vector<PKShaderKeyword> keywords;
        std::vector<PKMaterialProperty> materialProperties;
        std::vector<EntryPointInfo> entryPoints;
        auto enableInstancing = false;
        auto nofragInstancing = false;
        auto generateDebugInfo = false;
        auto logVerbose = false;
        auto logDisassembly = false;

        StringUtilities::ConvertTabsToSpaces(source);
        ExtractLogVerbose(source, &logVerbose);
        ExtractLogDisassembly(source, &logDisassembly);
        ExtractGenerateDebugInfo(source, &generateDebugInfo);

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

        for (auto variantIndex = 0u; variantIndex < shader->variantcount; ++variantIndex)
        {
            EntryPointInfo* stageEntries[(uint32_t)PKShaderStage::EnumCount]{};
            uint32_t activeStageMask = 0u;
            std::string activeHeaderDefines;
            FindActiveEntryPoints(entryPoints, variantDefines[variantIndex], stageEntries, &activeStageMask, activeHeaderDefines);

            std::string variantSource = activeHeaderDefines + source;
            std::vector<std::string> filenames;
            simplecpp::TokenList variantTokens(filenames);

            if (PreprocessSource(filename, variantSource, filenames, variantTokens) != 0)
            {
                return -1;
            }

            ReflectionData reflectionData{};
            reflectionData.generateDebugInfo = generateDebugInfo;
            reflectionData.logVerbose = logVerbose;
            reflectionData.logDisassembly = logDisassembly;

            SetActiveEntryPoints(variantTokens, entryPoints, stageEntries);
            RemoveUnusedResourcesAndFunctions(variantTokens, stageEntries);
            ExtractPushConstants(variantTokens, activeStageMask, reflectionData.sourceConstants);

            variantSource = variantTokens.stringify();
            CompilePushConstantBlock(variantSource, reflectionData.sourceConstants);
            variantSource.insert(0ull, PK_SLANG_PRELUDE);

            if (!CompileVariantToSPIRV(filename, variantSource, stageEntries, reflectionData))
            {
                ReleaseReflectionData(reflectionData);
                return -1;
            }

            CompactPushConstants(reflectionData);
            CompressBindIndices(reflectionData);

            for (auto stageIndex = 0u; stageIndex < (uint32_t)PKShaderStage::EnumCount; ++stageIndex)
            {
                if (reflectionData.modules[stageIndex])
                {
                    const auto* mod = reflectionData.modules[stageIndex];
                    const auto code = spvReflectGetCode(mod);
                    const auto wordCount = (uint32_t)(spvReflectGetCodeSize(mod) / sizeof(uint32_t));
                    const auto cleanedSpirv = CleanSPIRV(code, wordCount, reflectionData.deadVariableIds[stageIndex], !generateDebugInfo);
                    auto pSpirv = buffer.Write(cleanedSpirv.data(), cleanedSpirv.size());
                    pVariants[variantIndex].sprivSizes[stageIndex] = (uint32_t)(cleanedSpirv.size() * sizeof(uint32_t));
                    pVariants[variantIndex].sprivBuffers[stageIndex].Set(buffer.data(), pSpirv.get());
                }
            }

            pVariants[variantIndex].groupSize[0] = reflectionData.groupSize[0];
            pVariants[variantIndex].groupSize[1] = reflectionData.groupSize[1];
            pVariants[variantIndex].groupSize[2] = reflectionData.groupSize[2];

            if (reflectionData.vertexAttributes.size() > 0)
            {
                pVariants[variantIndex].vertexAttributeCount = (uint16_t)reflectionData.vertexAttributes.size();
                auto pVertexAttributes = buffer.Write<PKVertexInputAttribute>(reflectionData.vertexAttributes.data(), reflectionData.vertexAttributes.size());
                pVariants[variantIndex].vertexAttributes.Set(buffer.data(), pVertexAttributes.get());
            }

            if (reflectionData.activeConstants.size() > 0)
            {
                const auto constantCount = std::min((uint32_t)reflectionData.activeConstants.size(), PK_ASSET_MAX_PUSH_CONSTANTS);

                if (reflectionData.activeConstants.size() > PK_ASSET_MAX_PUSH_CONSTANTS)
                {
                    printf("Warning! Shader has more push constants than supported (%i / %i) \n", (int)reflectionData.activeConstants.size(), PK_ASSET_MAX_PUSH_CONSTANTS);
                }

                auto pConstantVariables = buffer.Allocate<PKConstantVariable>(constantCount);
                pVariants[variantIndex].constantCount = (uint16_t)constantCount;
                pVariants[variantIndex].constantRange = (uint16_t)reflectionData.constantRangeSize;
                pVariants[variantIndex].constants.Set(buffer.data(), pConstantVariables.get());

                for (auto j = 0u; j < constantCount; ++j)
                {
                    const auto& constant = reflectionData.activeConstants[j];
                    WriteName(pConstantVariables[j].name, constant.name.c_str());
                    pConstantVariables[j].writeMask = (PKShaderStageFlags)constant.writeMask;
                    pConstantVariables[j].accessMask = (PKShaderStageFlags)constant.accessMask;
                    pConstantVariables[j].type = constant.type;
                    pConstantVariables[j].offset = constant.offset;
                    pConstantVariables[j].arraySize = constant.count;
                }
            }

            pVariants[variantIndex].descriptorCount = (uint16_t)reflectionData.sortedBindings.size();

            if (pVariants[variantIndex].descriptorCount > 0)
            {
                auto pDescriptors = buffer.Allocate<PKDescriptor>(pVariants[variantIndex].descriptorCount);
                pVariants[variantIndex].descriptors.Set(buffer.data(), pDescriptors.get());

                if (pVariants[variantIndex].descriptorCount > PK_ASSET_MAX_DESCRIPTORS_PER_SET)
                {
                    printf("Warning! Shader has descriptors outside of supported range (%i / %i) \n", pVariants[variantIndex].descriptorCount, PK_ASSET_MAX_DESCRIPTORS_PER_SET);
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
