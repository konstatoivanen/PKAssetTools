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
    using PKAssets::GetElementType;

    typedef shaderc::Compiler ShaderCompiler;

    struct ReflectBinding
    {
        uint32_t firstStage = (uint32_t)PKShaderStage::MaxCount;
        uint32_t maxBinding = 0u;
        uint32_t count = 0u;
        uint32_t writeStageMask = (uint32_t)PKShaderStageFlags::None;
        std::string name;
        const SpvReflectDescriptorBinding* bindings[(int)PKShaderStage::MaxCount]{};
        const SpvReflectDescriptorBinding* get() { return bindings[firstStage]; }
    };

    struct ReflectPushConstant
    {
        const SpvReflectBlockVariable* block = nullptr;
        uint32_t stageFlags = (uint32_t)PKShaderStageFlags::None;
    };

    typedef std::vector<uint32_t> ShaderSpriV;

    struct ReflectionData
    {
        SpvReflectShaderModule modules[(int)PKShaderStage::MaxCount]{};
        std::vector<PKVertexInputAttribute> vertexAttributes;
        std::vector<ReflectBinding> sortedBindings;
        std::map<std::string, ReflectBinding> uniqueBindings;
        std::map<std::string, ReflectPushConstant> uniqueVariables;
        std::map<uint32_t, uint32_t> setStageFlags; //PKShaderStageFlags
        uint32_t setCount = 0u;
        bool logVerbose;
    };

    static bool ReadFile(const std::string& filepath, std::string& ouput, std::filesystem::file_time_type lastDestWriteTime)
    {
        std::vector<std::string> includes;
        ouput = StringUtilities::ReadFileRecursiveInclude(filepath, includes);
        return PKVersionUtilities::IsFileAnyOutOfDate(includes, lastDestWriteTime);
    }

    static void ExtractMulticompiles(std::string& source,
        std::vector<std::vector<std::string>>& keywords,
        std::vector<PKShaderKeyword>& outKeywords,
        uint32_t* outVariantCount,
        uint32_t* outDirectiveCount)
    {
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

        *outDirectiveCount = (uint32_t)dcount;
        *outVariantCount = (uint32_t)vcount;
    }

    static void ExtractLogVerbose(std::string& source, bool* outValue)
    {
        auto value = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_LOGVERBOSE, source, true);
        *outValue = !value.empty();
    }

    static void ExtractGenerateDebugInfo(std::string& source, bool* outValue)
    {
        auto value = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_GENERATEDEBUGINFO, source, true);
        *outValue = !value.empty();
    }

    static void ExtractStateAttributes(std::string& source, PKShaderFixedStateAttributes* attributes)
    {
        auto valueZWrite = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_ZWRITE, source, false);
        auto valueZTest = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_ZTEST, source, false);
        auto valueBlendColor = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_BLENDCOLOR, source, false);
        auto valueBlendAlpha = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_BLENDALPHA, source, false);
        auto valueColorMask = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_COLORMASK, source, false);
        auto valueCull = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_CULL, source, false);
        auto valueOffset = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_OFFSET, source, false);
        auto valueRasterMode = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_RASTERMODE, source, false);

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
                attributes->blendSrcFactorColor = GetBlendFactorFromString(keywords.at(1));
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

        if (!valueRasterMode.empty())
        {
            auto keywords = StringUtilities::Split(valueRasterMode, " \n\r");

            attributes->rasterMode = PKRasterMode::Default;
            attributes->overEstimation = 0x0;

            if (keywords.size() > 0)
            {
                attributes->rasterMode = GetRasterModeFromString(keywords.at(0));
            }

            if (keywords.size() > 1)
            {
                attributes->overEstimation = (uint8_t)std::stoi(keywords.at(1));
            }
        }

        attributes->colorMask = GetColorMaskFromString(valueColorMask);
        attributes->cull = GetCullModeFromString(StringUtilities::Trim(valueCull));
    }

    static void ProcessAtomicCounter(std::string& source)
    {
        auto value = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_ATOMICCOUNTER, source, true);

        if (!value.empty())
        {
            source.insert(0, AtomicCounter_GLSL);
        }
    }

    static void InsertRequiredExtensions(std::string& source, PKShaderStage stage)
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

    static void GetSharedInclude(const std::string& source, std::string& sharedInclude)
    {
        auto typeToken = "#pragma PROGRAM_";
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

        for (auto i = 0u; i < keywords.size(); ++i)
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

    static int ProcessStageSources(const std::string& source,
        const std::string& sharedInclude,
        const std::string& variantDefines,
        std::unordered_map<PKShaderStage,
        std::string>& shaderSources,
        bool enableInstancing,
        bool nofragInstancing)
    {
        shaderSources.clear();

        auto typeTokenLength = strlen(PK_GL_STAGE_BEGIN);
        auto currentPos = source.find(PK_GL_STAGE_BEGIN, 0); //Start of shader type declaration line

        while (currentPos != std::string::npos)
        {
            auto eol = source.find_first_of("\r\n", currentPos); //End of shader type declaration line
            auto nextLinePos = source.find_first_not_of("\r\n", eol); //Start of shader code after shader type declaration line

            if (eol == std::string::npos || nextLinePos == std::string::npos)
            {
                printf("Syntax error! \n");
                return -1;
            }

            auto begin = currentPos + typeTokenLength; //Start of shader type name (after "#pragma PROGRAM_" keyword)
            auto type = source.substr(begin, eol - begin);
            auto stage = GetShaderStageFromString(type);

            if (stage == PKShaderStage::MaxCount)
            {
                printf("Unsupported shader stage specified! \n");
                return -1;
            }

            currentPos = source.find(PK_GL_STAGE_BEGIN, nextLinePos); //Start of next shader type declaration line
            shaderSources[stage] = currentPos == std::string::npos ? source.substr(nextLinePos) : source.substr(nextLinePos, currentPos - nextLinePos);
        }

        for (auto& kv : shaderSources)
        {
            kv.second.insert(0, sharedInclude);
            Instancing::InsertEntryPoint(kv.second, kv.first, enableInstancing, nofragInstancing);
            kv.second.insert(0, PK_SHADER_STAGE_DEFINES[(uint32_t)kv.first]);
            kv.second.insert(0, variantDefines);
            InsertRequiredExtensions(kv.second, kv.first);
            ProcessShaderVersion(kv.second);
        }

        return 0;
    }


    static int CompileGLSLToSpirV(const ShaderCompiler& compiler, 
                                  PKShaderStage stage, 
                                  const std::string& source_name, 
                                  const std::string& source, 
                                  bool generateDebugInfo,
                                  ShaderSpriV& spirvd, 
                                  ShaderSpriV& spirvr)
    {
        auto kind = ConvertToShadercKind(stage);

        shaderc::CompileOptions options;

        options.SetAutoBindUniforms(true);
        options.SetAutoMapLocations(true);
        options.SetTargetEnvironment(shaderc_target_env_default, shaderc_env_version_vulkan_1_3);
        options.SetTargetSpirv(shaderc_spirv_version_1_6);

        // Will crash to buffer owerflow on variable name syntax errors :/
        auto module = compiler.CompileGlslToSpv(source, kind, source_name.c_str(), options);
        auto status = module.GetCompilationStatus();

        if (status != shaderc_compilation_status_success || module.GetNumWarnings() > 0)
        {
            int minLine, maxLine;
            FindLineRange(source_name, module.GetErrorMessage(), &minLine, &maxLine);

            #if defined(WIN32)
            SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 12);
            #endif

            printf("\n ----------BEGIN ERROR---------- \n\n");
            printf("%s", module.GetErrorMessage().c_str());
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

        if (status != shaderc_compilation_status_success)
        {
            return -1;
        }

        spirvd = { module.cbegin(), module.cend() };

        options.SetOptimizationLevel(shaderc_optimization_level::shaderc_optimization_level_performance);
        
        if (generateDebugInfo)
        {
            options.SetGenerateDebugInfo();
        }

        module = compiler.CompileGlslToSpv(source, kind, source_name.c_str(), options);

        spirvr = { module.cbegin(), module.cend() };

        return 0;
    }

    static int GetReflectionModule(SpvReflectShaderModule* module, const ShaderSpriV& spriv)
    {
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

    static void GetUniqueBindings(ReflectionData& reflection, SpvReflectShaderModule* debugModule, PKShaderStage stage)
    {
        auto* module = &reflection.modules[(uint32_t)stage];

        uint32_t bindingCount = 0u;

        spvReflectEnumerateDescriptorBindings(debugModule, &bindingCount, nullptr);

        std::vector<SpvReflectDescriptorBinding*> activeBindings;
        activeBindings.resize(bindingCount);
        spvReflectEnumerateDescriptorBindings(debugModule, &bindingCount, activeBindings.data());

        for (auto i = 0u; i < bindingCount; ++i)
        {
            auto desc = activeBindings.at(i);

            if (!desc->accessed)
            {
                continue;
            }

            auto releaseBinding = spvReflectGetDescriptorBinding(module, desc->binding, desc->set, nullptr);

            if (releaseBinding == nullptr)
            {
                continue;
            }

            auto name = std::string(desc->name);

            if (name.empty())
            {
                name = std::string(desc->type_description->type_name);
            }

            reflection.setStageFlags[desc->set] |= 1 << (int)stage;

            auto& binding = reflection.uniqueBindings[name];
            binding.firstStage = binding.firstStage > (uint32_t)stage ? (int)stage : binding.firstStage;
            binding.maxBinding = binding.maxBinding > releaseBinding->binding ? binding.maxBinding : releaseBinding->binding;
            binding.name = name;
            binding.count = desc->type_description->op == SpvOpTypeRuntimeArray ? PKAssets::PK_ASSET_MAX_UNBOUNDED_SIZE : desc->count;
            auto isWritten = ReflectResourceWrite(debugModule->_internal->spirv_code, debugModule->_internal->spirv_word_count, desc->spirv_id, GetResourceType(desc->descriptor_type));

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

    static void GetVertexAttributes(ReflectionData& reflection, SpvReflectShaderModule* debugModule, PKShaderStage stage)
    {
        if (stage != PKShaderStage::Vertex)
        {
            return;
        }

        auto count = 0u;
        spvReflectEnumerateEntryPointInputVariables(debugModule, debugModule->entry_point_name, &count, nullptr);

        std::vector<SpvReflectInterfaceVariable*> variables;
        variables.resize(count);

        spvReflectEnumerateEntryPointInputVariables(debugModule, debugModule->entry_point_name, &count, variables.data());

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

    static void GetPushConstants(ReflectionData& reflection, SpvReflectShaderModule* debugModule, PKShaderStage stage)
    {
        auto stageFlag = (1u << (uint32_t)stage); // PKShaderStageFlags
        auto* module = &reflection.modules[(uint32_t)stage];

        auto count = 0u;
        spvReflectEnumerateEntryPointPushConstantBlocks(debugModule, debugModule->entry_point_name, &count, nullptr);

        std::vector<SpvReflectBlockVariable*> blocks;
        blocks.resize(count);

        spvReflectEnumerateEntryPointPushConstantBlocks(debugModule, debugModule->entry_point_name, &count, blocks.data());

        auto i = 0u;

        if (count > 0 && reflection.logVerbose)
        {
            printf("    Constants:");
        }

        for (auto* block : blocks)
        {
            // auto name = std::string(block->name);
            // This has moved down to a be under type description
            auto name = std::string(block->type_description->type_name);

            if (reflection.uniqueVariables.count(name) <= 0)
            {
                reflection.uniqueVariables[name] = { spvReflectGetPushConstantBlock(module, i, nullptr), stageFlag };
                
                if (reflection.logVerbose)
                {
                    printf(" %s", name.c_str());
                }
            }
            else
            {
                reflection.uniqueVariables[name].stageFlags |= stageFlag;
            }

            ++i;
        }

        if (count > 0 && reflection.logVerbose)
        {
            printf("\n");
        }
    }

    static void GetComputeGroupSize(SpvReflectShaderModule* shaderModule, uint32_t* outSize, bool logVerbose)
    {
        auto entryPoint = shaderModule->entry_points;

        if (ReflectLocalSize(shaderModule->_internal->spirv_code, shaderModule->_internal->spirv_word_count, outSize))
        {
            if (logVerbose)
            {
                printf("    GroupSize: %i,%i,%i\n", outSize[0], outSize[1], outSize[2]);
            }
        }

        outSize[0] = entryPoint->local_size.x;
        outSize[1] = entryPoint->local_size.y;
        outSize[2] = entryPoint->local_size.z;
    }

    static void CompressBindIndices(ReflectionData& reflection)
    {
        std::map<uint32_t, uint32_t> setRemap;
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

        reflection.setCount = setIndex;

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

        std::multimap<uint32_t, ReflectBinding> sortedBindingMap;

        for (auto& kv : reflection.uniqueBindings)
        {
            sortedBindingMap.insert(std::make_pair(kv.second.maxBinding, kv.second));
        }

        reflection.sortedBindings.clear();

        for (auto& kv : sortedBindingMap)
        {
            reflection.sortedBindings.push_back(kv.second);
        }

        uint32_t* setCounters = reinterpret_cast<uint32_t*>(alloca(sizeof(uint32_t) * reflection.setCount));
        memset(setCounters, 0, sizeof(uint32_t) * reflection.setCount);

        for (auto& binding : reflection.sortedBindings)
        {
            auto desc = binding.get();
            auto bindId = setCounters[desc->set];
            setCounters[desc->set]++;

            for (auto i = 0u; i < (int)PKShaderStage::MaxCount; ++i)
            {
                if (binding.bindings[i] != nullptr)
                {
                    spvReflectChangeDescriptorBindingNumbers(&reflection.modules[i], binding.bindings[i], bindId, desc->set);
                }
            }
        }
    }


    int WriteShader(const char* pathSrc, const char* pathDst)
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
        uint32_t directiveCount;
        std::string sharedInclude;
        std::string variantDefines;
        std::vector<std::vector<std::string>> mckeywords;
        std::vector<PKShaderKeyword> keywords;
        std::vector<PKMaterialProperty> materialProperties;
        std::unordered_map<PKShaderStage, std::string> shaderSources;
        auto enableInstancing = false;
        auto nofragInstancing = false;
        auto generateDebugInfo = false;
        auto logVerbose = false;
        ExtractLogVerbose(source, &logVerbose);
        ExtractGenerateDebugInfo(source, &generateDebugInfo);

        // Sadly this happens after includes :/
        if (logVerbose)
        {
            printf("Preprocessing shader: %s \n", filename.c_str());
        }

        ExtractMulticompiles(source, mckeywords, keywords, &shader->variantcount, &directiveCount);
        ExtractStateAttributes(source, &shader->attributes);
        Instancing::InsertMaterialAssembly(source, materialProperties, &enableInstancing, &nofragInstancing);
        ProcessAtomicCounter(source);
        ConvertHLSLTypesToGLSL(source);
        GetSharedInclude(source, sharedInclude);

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

        for (uint32_t i = 0; i < shader->variantcount; ++i)
        {
            GetVariantDefines(mckeywords, i, variantDefines);

            if (ProcessStageSources(source, sharedInclude, variantDefines, shaderSources, enableInstancing, nofragInstancing) != 0)
            {
                printf("Failed to preprocess shader variant glsl stage sources! \n");
                return -1;
            }

            ReflectionData reflectionData{};
            reflectionData.logVerbose = logVerbose;

            for (auto& kv : shaderSources)
            {
                if (logVerbose)
                {
                    printf("Compiling %s:%i %s \n", filename.c_str(), i, PK_SHADER_STAGE_NAMES[(int)kv.first]);
                }

                ShaderSpriV spirvDebug;
                ShaderSpriV spirvRelease;

                if (CompileGLSLToSpirV(compiler, kv.first, filename, kv.second, generateDebugInfo, spirvDebug, spirvRelease) != 0)
                {
                    printf("Failed to compile spirv from shader variant source! \n");
                    return -1;
                }

                auto* moduleRelease = &reflectionData.modules[(uint32_t)kv.first];
                // Moved to heap due to reduce stack size.
                auto* moduleDebug = new SpvReflectShaderModule();

                if (GetReflectionModule(moduleRelease, spirvRelease) != 0)
                {
                    printf("Failed to extract reflection data from shader variant source! \n");
                    return -1;
                }

                if (GetReflectionModule(moduleDebug, spirvDebug) != 0)
                {
                    printf("Failed to extract reflection data from shader variant source! \n");
                    return -1;
                }

                GetUniqueBindings(reflectionData, moduleDebug, kv.first);
                GetVertexAttributes(reflectionData, moduleDebug, kv.first);
                GetPushConstants(reflectionData, moduleDebug, kv.first);
                GetComputeGroupSize(moduleDebug, pVariants[i].groupSize, logVerbose);
                spvReflectDestroyShaderModule(moduleDebug);

                delete moduleDebug;
            }

            CompressBindIndices(reflectionData);

            for (auto& kv : shaderSources)
            {
                auto size = spvReflectGetCodeSize(&reflectionData.modules[(int)kv.first]);
                auto code = spvReflectGetCode(&reflectionData.modules[(int)kv.first]);
                auto pSpirv = buffer.Write(code, size / sizeof(uint32_t));
                pVariants[i].sprivSizes[(int)kv.first] = size;
                pVariants[i].sprivBuffers[(int)kv.first].Set(buffer.data(), pSpirv.get());
            }

            if (reflectionData.vertexAttributes.size() > 0)
            {
                pVariants[i].vertexAttributeCount = (uint32_t)reflectionData.vertexAttributes.size();
                auto pVertexAttributes = buffer.Write<PKVertexInputAttribute>(reflectionData.vertexAttributes.data(), reflectionData.vertexAttributes.size());
                pVariants[i].vertexAttributes.Set(buffer.data(), pVertexAttributes.get());
            }

            if (reflectionData.uniqueVariables.size() > 0)
            {
                pVariants[i].constantVariableCount = (uint32_t)reflectionData.uniqueVariables.size();
                auto pConstantVariables = buffer.Allocate<PKConstantVariable>(reflectionData.uniqueVariables.size());
                pVariants[i].constantVariables.Set(buffer.data(), pConstantVariables.get());

                auto j = 0u;
                for (auto& kv : reflectionData.uniqueVariables)
                {
                    if (j >= PK_ASSET_MAX_PUSH_CONSTANTS)
                    {
                        printf("Warning! Shader has more push constants than supported (%i / %i) \n", j, PK_ASSET_MAX_PUSH_CONSTANTS);
                        continue;
                    }

                    WriteName(pConstantVariables[j].name, kv.first.c_str());
                    pConstantVariables[j].offset = (unsigned short)kv.second.block->offset;
                    pConstantVariables[j].stageFlags = (PKShaderStageFlags)kv.second.stageFlags;
                    pConstantVariables[j++].size = kv.second.block->size;
                }
            }

            if (reflectionData.setCount > 0)
            {
                pVariants[i].descriptorSetCount = reflectionData.setCount;
                auto pDescriptorSets = buffer.Allocate<PKDescriptorSet>(reflectionData.setCount);
                pVariants[i].descriptorSets.Set(buffer.data(), pDescriptorSets.get());

                std::map<uint32_t, std::vector<PKDescriptor>> descriptors;

                for (auto& binding : reflectionData.sortedBindings)
                {
                    auto desc = binding.get();

                    if (desc->set >= PK_ASSET_MAX_DESCRIPTOR_SETS)
                    {
                        printf("Warning! Shader has a descriptor set outside of supported range (%i / %i) \n", desc->set, PK_ASSET_MAX_DESCRIPTOR_SETS);
                        continue;
                    }

                    PKDescriptor descriptor{};
                    descriptor.count = binding.count;
                    descriptor.type = GetResourceType(desc->descriptor_type);
                    descriptor.writeStageMask = (PKShaderStageFlags)binding.writeStageMask;
                    WriteName(descriptor.name, binding.name.c_str());
                    descriptors[desc->set].push_back(descriptor);
                }

                for (auto& kv : descriptors)
                {
                    pDescriptorSets[kv.first].descriptorCount = (uint32_t)kv.second.size();
                    pDescriptorSets[kv.first].stageflags = (PKShaderStageFlags)reflectionData.setStageFlags[kv.first];
                    auto pDescriptors = buffer.Write(kv.second.data(), kv.second.size());
                    pDescriptorSets[kv.first].descriptors.Set(buffer.data(), pDescriptors.get());
                }
            }

            ReleaseReflectionData(reflectionData);
        }

        return WriteAsset(pathDst, buffer, false);
    }
}