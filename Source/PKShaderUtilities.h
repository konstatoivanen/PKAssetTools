#pragma once
#include <shaderc/shaderc.hpp>
#include <SPIRV-Reflect/spirv_reflect.h>
#include <map>
#include <unordered_map>
#include <PKAsset.h>

namespace PKAssets::Shader
{
    constexpr const static char* PK_SHADER_STAGE_NAMES[(uint32_t)PKShaderStage::MaxCount] =
    {
        "STAGE_VERTEX",
        "STAGE_TESSELATION_CONTROL",
        "STAGE_TESSELATION_EVALUATE",
        "STAGE_GEOMETRY",
        "STAGE_FRAGMENT",
        "STAGE_COMPUTE",
        "STAGE_MESH_TASK",
        "STAGE_MESH_ASSEMBLY",

        "STAGE_RAY_GENERATION",
        "STAGE_RAY_MISS",
        "STAGE_RAY_CLOSEST_HIT",
        "STAGE_RAY_ANY_HIT",
        "STAGE_RAY_INTERSECTION"
    };

    constexpr const static char* PK_SHADER_STAGE_DEFINES[(uint32_t)PKShaderStage::MaxCount] =
    {
        "#define SHADER_STAGE_VERTEX\n",
        "#define SHADER_STAGE_TESSELATION_CONTROL\n",
        "#define SHADER_STAGE_TESSELATION_EVALUATE\n",
        "#define SHADER_STAGE_GEOMETRY\n",
        "#define SHADER_STAGE_FRAGMENT\n",
        "#define SHADER_STAGE_COMPUTE\n",
        "#define SHADER_STAGE_MESH_TASK\n",
        "#define SHADER_STAGE_MESH_ASSEMBLY\n",

        "#define SHADER_STAGE_RAY_GENERATION\n",
        "#define SHADER_STAGE_RAY_MISS\n",
        "#define SHADER_STAGE_RAY_CLOSEST_HIT\n",
        "#define SHADER_STAGE_RAY_ANY_HIT\n",
        "#define SHADER_STAGE_RAY_INTERSECTION\n"
    };

    constexpr const static char* PK_GL_EXTENSIONS_COMMON =
        "#extension GL_KHR_shader_subgroup_basic : require\n"
        "#extension GL_KHR_shader_subgroup_ballot : require\n"
        "#extension GL_KHR_shader_subgroup_vote : require\n"
        "#extension GL_EXT_samplerless_texture_functions : require \n"
        "#extension GL_EXT_shader_16bit_storage : require \n"
        "#extension GL_EXT_control_flow_attributes : require \n"
        "#extension GL_EXT_shader_explicit_arithmetic_types : require \n"
        "#extension GL_EXT_nonuniform_qualifier : require \n"
        "#extension GL_ARB_shader_viewport_layer_array : require \n"
        "#extension GL_EXT_shader_image_load_formatted : require \n";

    constexpr const static char* PK_GL_EXTENSIONS_RAYTRACING = 
        "#extension GL_EXT_ray_tracing : require \n"
        "#extension GL_EXT_ray_tracing_position_fetch : require \n";
    
    constexpr const static char* PK_GL_EXTENSIONS_MESHSHADING = 
        "#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require\n"
        "#extension GL_EXT_mesh_shader : require \n";

    struct SourcePushConstant
    {
        std::string name;
        std::string field;
        uint32_t stageFlags = 0u;
    };

    struct EntryPointInfo
    {
        PKShaderStage stage;
        std::string name;
        std::string functionName;
        std::string keyword;
    };

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
        uint16_t offset = 0u;
        uint16_t size = 0u;
    };

    struct ReflectionData
    {
        SpvReflectShaderModule* modulesRel[(int)PKShaderStage::MaxCount]{};
        SpvReflectShaderModule* modulesDeb[(int)PKShaderStage::MaxCount]{};
        std::vector<PKVertexInputAttribute> vertexAttributes;
        std::vector<ReflectBinding> sortedBindings;
        std::map<std::string, ReflectBinding> uniqueBindings;
        std::map<std::string, ReflectPushConstant> uniqueConstants;
        uint32_t constantRangeSize = 0u;
        uint32_t constantRangeStageFlags = 0u; //PKShaderStageFlags
        bool logVerbose;
    };

    typedef std::vector<SourcePushConstant> SourcePushConstants;
    typedef shaderc::Compiler ShaderCompiler;
    typedef shaderc::CompileOptions CompileOptions;

    PKElementType GetElementType(SpvReflectFormat format);
    std::string GetGLSLType(PKElementType type);
    PKDescriptorType GetResourceType(SpvReflectDescriptorType type);
    shaderc_shader_kind ConvertToShadercKind(PKShaderStage stage);
    std::string ReflectBindingName(SpvReflectDescriptorBinding* binding);
    void FindLineRange(const std::string& name, const std::string& message, int* outMin, int* outMax);
    void ExtractLogVerbose(std::string& source, bool* outValue);
    void ExtractGenerateDebugInfo(std::string& source, bool* outValue);
    void ExtractStateAttributes(std::string& source, PKShaderFixedStateAttributes* attributes);
    void InsertRequiredExtensions(std::string& source, PKShaderStage stage);
    int RemoveEntryPointLocals(std::string& source, const std::string& entryPointName, PKShaderStage stage);
    void RemoveInactiveGroupSizeLayouts(std::string& source, PKShaderStage stage);
    void ProcessShaderVersion(std::string& source);
    void RemoveDescriptorSets(std::string& source);
    void RemoveUnsupportedRayTracingFields(std::string& source, PKShaderStage stage);
    void ConvertPKNumThreads(std::string& source);
    void ConvertHLSLBuffers(std::string& source);
    void ConvertHLSLCBuffers(std::string& source);
    void ConvertHLSLTypesToGLSL(std::string& source);
    void ExtractPushConstants(std::string& source, PKShaderStage stage, SourcePushConstants& outConstants);
    void CompilePushConstantBlock(std::string* stageSources, const SourcePushConstants& constants);
}
