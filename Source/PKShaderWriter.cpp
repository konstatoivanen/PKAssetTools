#include <unordered_map>
#include <unordered_set>
#include <map>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <slang/slang.h>
#include <slang/slang-com-ptr.h>
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
#undef max
#undef min
#endif

namespace PKAssets::Shader
{
    static slang::IGlobalSession* GetSlangGlobalSession()
    {
        static Slang::ComPtr<slang::IGlobalSession> s_globalSession;

        if (!s_globalSession)
        {
            if (SLANG_FAILED(slang::createGlobalSession(s_globalSession.writeRef())))
            {
                printf("Failed to create Slang global session!\n");
                return nullptr;
            }
        }
        
        return s_globalSession.get();
    }

    static void PrintError(Slang::ComPtr<slang::IBlob>& diagnostics, const std::string& filename, const std::string& userSource, const std::string& context, bool format)
    {
        #if defined(WIN32)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 12);
        #endif

        printf("\n ----------BEGIN ERROR---------- \n");

        if (!context.empty())
        {
            printf("%s\n", context.c_str());
        }

        if (diagnostics && format)
        {
            std::string err(static_cast<const char*>(diagnostics->getBufferPointer()), diagnostics->getBufferSize());
            printf("%s", FormatErrorMessage(filename, userSource, err).c_str());
        }
        
        if (diagnostics && !format)
        {
            printf("%s\n", static_cast<const char*>(diagnostics->getBufferPointer()));
        }

        printf("\n ----------END ERROR---------- \n\n");

        #if defined(WIN32)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 15);
        #endif
    }

    static bool ReadFile(const std::string& filepath, std::string& output, std::filesystem::file_time_type lastDestWriteTime)
    {
        std::vector<std::string> includes;
        output = StringUtilities::ReadFileRecursiveInclude(filepath, includes);
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
                    pkkw.directive = (uint8_t)dcount;
                    pkkw.value = (uint8_t)i;
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
        std::unordered_set<std::string> processedEntryPoints;
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
                printf("Unsupported shader stage specified! \n");
                return -1;
            }

            EntryPointInfo info;
            info.name = directives[1];
            info.stage = stage;
            info.keyword = directives.size() == 3 ? directives[2] : "";

            if (!processedEntryPoints.count(info.name))
            {
                const auto attribute = PKShaderStageToSlangAttribute(stage);

                if (!StringUtilities::InjectFunctionAttribute(source, info.name, attribute))
                {
                    printf("Definition for declared entry point %s was not found!\n", info.name.c_str());
                    return -1;
                }

                processedEntryPoints.insert(info.name);
            }

            entryPoints.push_back(info);
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

    static slang::CompilerOptionEntry MakeIntOption(slang::CompilerOptionName name, int value)
    {
        slang::CompilerOptionEntry entry{};
        entry.name = name;
        entry.value.kind = slang::CompilerOptionValueKind::Int;
        entry.value.intValue0 = value;
        return entry;
    }

    static void AddCapabilityOption(slang::IGlobalSession* globalSession, std::vector<slang::CompilerOptionEntry>& options, const char* capabilityName)
    {
        const auto capability = globalSession->findCapability(capabilityName);

        if (capability != SLANG_CAPABILITY_UNKNOWN)
        {
            options.push_back(MakeIntOption(slang::CompilerOptionName::Capability, (int32_t)capability));
        }
    }

    static void MergeStageReflection(ReflectionData& reflection, PKShaderStage stage, const SourcePushConstants& sourceConstants)
    {
        reflection.stageMask |= 1u << (uint32_t)stage;
        const auto& stageReflection = reflection.stages[(uint32_t)stage];

        if (stage == PKShaderStage::Vertex && !stageReflection.vertexAttributes.empty())
        {
            if (reflection.logVerbose)
            {
                printf("    Interface:");
            }

            for (const auto& attr : stageReflection.vertexAttributes)
            {
                if (reflection.vertexAttributes.size() >= PK_ASSET_MAX_VERTEX_ATTRIBUTES)
                {
                    printf("Warning! Shader has more vertex attributes than supported (%i / %i) \n", (int)reflection.vertexAttributes.size(), PK_ASSET_MAX_VERTEX_ATTRIBUTES);
                    continue;
                }

                reflection.vertexAttributes.push_back(attr);

                if (reflection.logVerbose)
                {
                    printf(" %s ", attr.name);
                }
            }

            if (reflection.logVerbose)
            {
                printf("\n");
            }
        }

        for (const auto& constant : stageReflection.pushConstantMembers)
        {
            if (constant.isAccessed)
            {
                auto& info = reflection.uniqueConstants[constant.name];
                info.name = constant.name;
                info.offset = constant.offset;
                info.accessStageMask |= 1u << (uint32_t)stage;

                bool isReadOnly = true;

                for (const auto& source : sourceConstants)
                {
                    if (source.name == constant.name)
                    {
                        info.type = source.type;
                        info.arraySize = source.arraySize;
                        isReadOnly = source.isReadOnly;
                        break;
                    }
                }

                if (constant.isWritten && !isReadOnly && (info.type == PKElementType::Image || info.type == PKElementType::StorageBuffer))
                {
                    info.writeStageMask |= 1u << (uint32_t)stage;
                }
            }
        }

        if (stageReflection.groupSize[0] || 
            stageReflection.groupSize[1] || 
            stageReflection.groupSize[2])
        {
            reflection.groupSize[0] = stageReflection.groupSize[0];
            reflection.groupSize[1] = stageReflection.groupSize[1];
            reflection.groupSize[2] = stageReflection.groupSize[2];

            if (reflection.logVerbose)
            {
                printf("    GroupSize: %i,%i,%i\n", reflection.groupSize[0], reflection.groupSize[1], reflection.groupSize[2]);
            }
        }
    }

static void CompactPushConstants(ReflectionData& reflection, const SourcePushConstants& sourceConstants)
    {
        reflection.sortedConstants.clear();
        reflection.constantRangeSize = 0u;

        if (reflection.uniqueConstants.empty())
        {
            return;
        }

        for (const auto& kv : reflection.uniqueConstants)
        {
            reflection.sortedConstants.push_back(kv.second);
        }

        std::sort(reflection.sortedConstants.begin(), reflection.sortedConstants.end(), [](const ReflectConstantInfo& a, const ReflectConstantInfo& b)
        {
            return a.offset < b.offset;
        });

        std::unordered_map<std::string, uint16_t> compactedOffsets;
        auto nextOffset = 0u;

        if (reflection.logVerbose)
        {
            printf("    Constants:\n");
        }

        for (auto& constant : reflection.sortedConstants)
        {
            const auto align = PKElementTypeToScalarSize(constant.type);
            const auto size = PKElementTypeToSize(constant.type) * constant.arraySize;
            nextOffset = (nextOffset + align - 1u) & ~(align - 1u);
            constant.offset = (uint16_t)nextOffset;
            compactedOffsets[constant.name] = constant.offset;
            nextOffset += size;

            if (reflection.logVerbose)
            {
                printf("        %s : size: %uB : offset: %u, arraySize: %u \n", constant.name.c_str(), size, constant.offset, constant.arraySize);
            }
        }

        auto unusedOffset = (nextOffset + 3u) & ~3u;

        if (reflection.logVerbose)
        {
            printf("    Unused:\n");
        }

        for (const auto& src : sourceConstants)
        {
            if (compactedOffsets.find(src.name) == compactedOffsets.end())
            {
                const auto align = PKElementTypeToScalarSize(src.type);
                const auto size = PKElementTypeToSize(src.type) * src.arraySize;
                unusedOffset = (unusedOffset + align - 1u) & ~(align - 1u);
                compactedOffsets[src.name] = (uint16_t)unusedOffset;

                if (reflection.logVerbose)
                {
                    printf("        %s: size: %uB, offset: %u, arraySize: %u \n", src.name.c_str(), size, unusedOffset, src.arraySize);
                }

                unusedOffset += size;
            }
        }

        reflection.constantRangeSize = (unusedOffset + 3u) & ~3u;

        for (auto stage = 0u; stage < (uint32_t)PKShaderStage::EnumCount; ++stage)
        {
            if (reflection.stageMask & (1u << stage))
            {
                auto& stageReflection = reflection.stages[stage];

                for (const auto& member : stageReflection.pushConstantMembers)
                {
                    if (member.wordOffset)
                    {
                        auto offset = compactedOffsets.find(member.name);

                        if (offset != compactedOffsets.end())
                        {
                            stageReflection.spirv[member.wordOffset] = (uint32_t)offset->second;
                        }
                    }
                }
            }
        }
    }
    static bool CompileVariantToSPIRV(
        const std::string& name,
        const std::string& variantSource,
        const std::string& userSource,
        EntryPointInfo** stageEntries,
        const SourcePushConstants& sourceConstants,
        bool generateDebugInfo,
        ReflectionData& reflectionData)
    {
        slang::IGlobalSession* globalSession = GetSlangGlobalSession();

        if (!globalSession)
        {
            return false;
        }

        slang::TargetDesc targetDesc{};
        targetDesc.format = SLANG_SPIRV;
        targetDesc.profile = globalSession->findProfile("spirv_1_6");
        targetDesc.flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;

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
        AddCapabilityOption(globalSession, options, "GL_EXT_shader_image_load_formatted");
        AddCapabilityOption(globalSession, options, "GL_EXT_ray_tracing");
        AddCapabilityOption(globalSession, options, "GL_EXT_ray_tracing_position_fetch");
        AddCapabilityOption(globalSession, options, "GL_EXT_mesh_shader");
        AddCapabilityOption(globalSession, options, "GL_EXT_debug_printf");

        targetDesc.compilerOptionEntries = options.data();
        targetDesc.compilerOptionEntryCount = (uint32_t)options.size();

        slang::SessionDesc sessionDesc{};
        sessionDesc.targets = &targetDesc;
        sessionDesc.targetCount = 1;
        sessionDesc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;

        slang::ISession* sessionPtr = nullptr;
        globalSession->createSession(sessionDesc, &sessionPtr);
        
        Slang::ComPtr<slang::ISession> session(sessionPtr);
        Slang::ComPtr<slang::IBlob> diagnostics;
        Slang::ComPtr<slang::IModule> module(session->loadModuleFromSourceString(name.c_str(), name.c_str(), variantSource.c_str(), diagnostics.writeRef()));
        std::vector<Slang::ComPtr<slang::IEntryPoint>> entryPoints;
        std::vector<slang::IComponentType*> components = { module };
        Slang::ComPtr<slang::IComponentType> composedProgram;
        Slang::ComPtr<slang::IComponentType> linkedProgram;
        std::vector<uint32_t> activeStages;

        if (!module)
        {
            PrintError(diagnostics, name, userSource, "Source load failed:", true);
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
                module->findAndCheckEntryPoint(entry.name.c_str(), PKShaderStageToSlangStage(entry.stage), entryPoint.writeRef(), diagnostics.writeRef());

                if (!entryPoint)
                {
                    PrintError(diagnostics, name, userSource, "Compilation failed:", false);
                    return false;
                }

                components.push_back(entryPoint);
                entryPoints.push_back(entryPoint);
                activeStages.push_back(stageIndex);
            }
        }

        session->createCompositeComponentType(components.data(), (SlangInt)components.size(), composedProgram.writeRef(), diagnostics.writeRef());
        composedProgram->link(linkedProgram.writeRef(), diagnostics.writeRef());

        if (!linkedProgram)
        {
            PrintError(diagnostics, name, userSource, "Linking failed:", false);
            return false;
        }

        for (auto i = 0u; i < activeStages.size(); ++i)
        {
            Slang::ComPtr<slang::IBlob> spirvBlob;
            linkedProgram->getEntryPointCode((SlangInt)i, 0, spirvBlob.writeRef(), diagnostics.writeRef());

            if (!spirvBlob || !spirvBlob->getBufferSize())
            {
                PrintError(diagnostics, name, userSource, "SPIRV emission failed:", false);
                return false;
            }

            const auto stage = activeStages[i];
            const auto* spriv = static_cast<const uint32_t*>(spirvBlob->getBufferPointer());
            const auto wordCount = spirvBlob->getBufferSize() / sizeof(uint32_t);

            if (!ReflectSPIRVStage(spriv, wordCount, (PKShaderStage)stage, &reflectionData.stages[stage]))
            {
                printf("Failed to extract reflection data from SPIR-V module!\n");
                return false;
            }

            MergeStageReflection(reflectionData, (PKShaderStage)stage, sourceConstants);
        }

        CompactPushConstants(reflectionData, sourceConstants);
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
        std::string instancingHeader;
        auto enableInstancing = false;
        auto generateDebugInfo = false;
        auto logVerbose = false;

        StringUtilities::ConvertTabsToSpaces(source);
        StringUtilities::RemoveCommentsAndPadScalarSwizzles(source);
        ExtractLogVerbose(source, &logVerbose);
        ExtractGenerateDebugInfo(source, &generateDebugInfo);

        if (logVerbose)
        {
            printf("Preprocessing shader: %s \n", filename.c_str());
        }

        ExtractMulticompiles(source, variantDefines, keywords);
        ExtractStateAttributes(source, &shader->attributes);
        Instancing::InsertMaterialAssembly(source, instancingHeader, materialProperties, &enableInstancing);

        if (ExtractEntryPoints(source, entryPoints) != 0)
        {
            return -1;
        }

        SourcePushConstants sourceConstants;
        ExtractPushConstants(instancingHeader, sourceConstants);
        ExtractPushConstants(source, sourceConstants);
        
        const auto pushConstantBlock = CompilePushConstantBlock(sourceConstants);

        shader->variantcount = (uint32_t)variantDefines.size();
        shader->keywordCount = (uint32_t)keywords.size();
        shader->materialPropertyCount = (uint32_t)materialProperties.size();

        if (!keywords.empty())
        {
            auto pKeywords = buffer.Write<PKShaderKeyword>(keywords.data(), keywords.size());
            shader->keywords.Set(buffer.data(), pKeywords.get());
        }

        if (!materialProperties.empty())
        {
            auto pMaterialProperties = buffer.Write<PKMaterialProperty>(materialProperties.data(), materialProperties.size());
            shader->materialProperties.Set(buffer.data(), pMaterialProperties.get());
        }

        auto pVariants = buffer.Allocate<PKShaderVariant>(shader->variantcount);
        shader->variants.Set(buffer.data(), pVariants.get());

        for (auto variantIndex = 0u; variantIndex < shader->variantcount; ++variantIndex)
        {
            EntryPointInfo* stageEntries[(uint32_t)PKShaderStage::EnumCount]{};

            for (auto stageIndex = 0u; stageIndex < (uint32_t)PKShaderStage::EnumCount; ++stageIndex)
            {
                auto entryIndex = FindActiveEntryPointIndexForStage(entryPoints, variantDefines[variantIndex], (PKShaderStage)stageIndex);

                if (entryIndex != -1)
                {
                    stageEntries[stageIndex] = &entryPoints.at(entryIndex);
                }
            }

            std::string variantSource;
            variantSource.reserve(strlen(PK_SLANG_PRELUDE) + instancingHeader.size() + pushConstantBlock.size() + source.size() + 128ull);
            variantSource.append(PK_SLANG_PRELUDE);
            variantSource.append("\n");
            variantSource.append(variantDefines[variantIndex]);
            variantSource.append("\n");
            variantSource.append(pushConstantBlock);
            variantSource.append("\n");
            variantSource.append(instancingHeader);
            variantSource.append("\n#line 1 \"");
            variantSource.append(filename);
            variantSource.append("\"\n");
            variantSource.append(source);

            ReflectionData reflectionData{};
            reflectionData.logVerbose = logVerbose;

            if (!CompileVariantToSPIRV(filename, variantSource, source, stageEntries, sourceConstants, generateDebugInfo, reflectionData))
            {
                return -1;
            }

            pVariants[variantIndex].groupSize[0] = reflectionData.groupSize[0];
            pVariants[variantIndex].groupSize[1] = reflectionData.groupSize[1];
            pVariants[variantIndex].groupSize[2] = reflectionData.groupSize[2];

            for (auto stageIndex = 0u; stageIndex < (uint32_t)PKShaderStage::EnumCount; ++stageIndex)
            {
                if (reflectionData.stageMask & (1u << stageIndex))
                {
                    const auto& stageRefl = reflectionData.stages[stageIndex];
                    auto pSpirv = buffer.Write(stageRefl.spirv.data(), stageRefl.spirv.size());
                    pVariants[variantIndex].sprivSizes[stageIndex] = (uint32_t)(stageRefl.spirv.size() * sizeof(uint32_t));
                    pVariants[variantIndex].sprivBuffers[stageIndex].Set(buffer.data(), pSpirv.get());
                }
            }

            if (!reflectionData.vertexAttributes.empty())
            {
                pVariants[variantIndex].vertexAttributeCount = (uint16_t)reflectionData.vertexAttributes.size();
                auto pVertexAttributes = buffer.Write<PKVertexInputAttribute>(reflectionData.vertexAttributes.data(), reflectionData.vertexAttributes.size());
                pVariants[variantIndex].vertexAttributes.Set(buffer.data(), pVertexAttributes.get());
            }

            if (!reflectionData.sortedConstants.empty())
            {
                pVariants[variantIndex].constantCount = (uint16_t)reflectionData.sortedConstants.size();
                pVariants[variantIndex].constantRange = (uint16_t)reflectionData.constantRangeSize;

                auto pConstantVariables = buffer.Allocate<PKConstantVariable>(reflectionData.sortedConstants.size());
                pVariants[variantIndex].constants.Set(buffer.data(), pConstantVariables.get());

                for (auto j = 0u; j < reflectionData.sortedConstants.size(); ++j)
                {
                    if (j >= PK_ASSET_MAX_PUSH_CONSTANTS)
                    {
                        printf("Warning! Shader has more push constants than supported (%i / %i) \n", j, PK_ASSET_MAX_PUSH_CONSTANTS);
                        break;
                    }

                    const auto& c = reflectionData.sortedConstants[j];
                    WriteName(pConstantVariables[j].name, c.name.c_str());
                    pConstantVariables[j].writeMask = (PKShaderStageFlags)c.writeStageMask;
                    pConstantVariables[j].accessMask = (PKShaderStageFlags)c.accessStageMask;
                    pConstantVariables[j].type = c.type;
                    pConstantVariables[j].offset = (uint8_t)c.offset;
                    pConstantVariables[j].arraySize = c.arraySize;
                }
            }
        }

        return WriteAsset(pathDst, pathStemOffset, buffer, false);
    }
}
