#pragma once
#include <shaderc/shaderc.hpp>
#include <SPIRV-Reflect/spirv_reflect.h>
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
    
    constexpr const static char* AtomicCounter_GLSL =
        "layout(std430, set = 3) buffer pk_BuiltInAtomicCounter { uint pk_BuiltInAtomicCounter_Data; };\n"
        "uint PK_AtomicCounterAdd(uint increment) { return atomicAdd(pk_BuiltInAtomicCounter_Data, increment); }\n"
        "uint PK_AtomicCounterNext() { return atomicAdd(pk_BuiltInAtomicCounter_Data, 1u); }\n";

    constexpr const static char* PK_GL_EXTENSIONS_COMMON =
        "#extension GL_KHR_shader_subgroup_basic : require\n"
        "#extension GL_KHR_shader_subgroup_ballot : require\n"
        "#extension GL_KHR_shader_subgroup_vote : require\n"
        "#extension GL_EXT_samplerless_texture_functions : require \n"
        "#extension GL_EXT_shader_16bit_storage : require \n"
        "#extension GL_EXT_control_flow_attributes : require \n"
        "#extension GL_EXT_shader_explicit_arithmetic_types : require \n"
        "#extension GL_EXT_nonuniform_qualifier : require \n"
        "#extension GL_ARB_shader_viewport_layer_array : require \n";

    constexpr const static char* PK_GL_EXTENSIONS_RAYTRACING = "#extension GL_EXT_ray_tracing : require \n";
    constexpr const static char* PK_GL_EXTENSIONS_MESHSHADING = 
        "#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require\n"
        "#extension GL_EXT_mesh_shader : require \n";

    constexpr const static char* PK_GL_STAGE_BEGIN = "#pragma PROGRAM_";

    PKElementType GetElementType(SpvReflectFormat format);
    std::string GetGLSLType(PKElementType type);
    PKComparison GetZTestFromString(const std::string& ztest);
    PKBlendFactor GetBlendFactorFromString(const std::string& blendMode);
    PKBlendOp GetBlendOpFromString(const std::string& blendOp);
    uint8_t GetColorMaskFromString(const std::string& colorMask);
    PKCullMode GetCullModeFromString(const std::string& cull);
    PKRasterMode GetRasterModeFromString(const std::string& rasterMode);
    PKShaderStage GetShaderStageFromString(const std::string& type);
    PKDescriptorType GetResourceType(SpvReflectDescriptorType type);
    shaderc_shader_kind ConvertToShadercKind(PKShaderStage stage);
    void FindLineRange(const std::string& name, const std::string& message, int* outMin, int* outMax);
    void ConvertHLSLTypesToGLSL(std::string& source);
}
