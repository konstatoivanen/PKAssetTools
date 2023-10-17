#pragma once

#include "PKAssets/PKAsset.h"
#include <shaderc/shaderc.hpp>
#include <SPIRV-Reflect/spirv_reflect.h>
#include <sstream>

namespace PK::Assets::Shader
{
    constexpr const static char* PK_SHADER_STAGE_DEFINES[(uint32_t)PKShaderStage::MaxCount] =
    {
        "#define SHADER_STAGE_VERTEX\n",                //Vertex,
        "#define SHADER_STAGE_TESSELATION_CONTROL\n",   //TesselationControl,
        "#define SHADER_STAGE_TESSELATION_EVALUATE\n",  //TesselationEvaluation,
        "#define SHADER_STAGE_GEOMETRY\n",              //Geometry,
        "#define SHADER_STAGE_FRAGMENT\n",              //Fragment,
        "#define SHADER_STAGE_COMPUTE\n",               //Compute,

        "#define SHADER_STAGE_RAY_GENERATION\n",        //RayGeneration,
        "#define SHADER_STAGE_RAY_MISS\n",              //RayMiss,
        "#define SHADER_STAGE_RAY_CLOSEST_HIT\n",       //RayClosestHit,
        "#define SHADER_STAGE_RAY_ANY_HIT\n",           //RayAnyHit,
        "#define SHADER_STAGE_RAY_INTERSECTION\n"       //RayIntersection,
    };
    
    // @TODO NOTE that for optimization purposes PK_Draw uses uint4(material, transform, mesh, userdata) internally (this saves a redundant struct descriptor)
    constexpr const static char* Instancing_Standalone_GLSL =
        "#define PK_INSTANCING_ENABLED                                                                                                                  \n"
        "#define PK_Draw uint4                                                                                                                          \n"
        "layout(std430, set = 0, binding = 100) readonly restrict buffer pk_Instancing_Transforms { mat3x4 pk_Instancing_Transforms_Data[]; };          \n"
        "layout(std430, set = 3, binding = 101) readonly restrict buffer pk_Instancing_Indices { PK_Draw pk_Instancing_Indices_Data[]; };               \n"
        "mat3x4 pk_ObjectToWorld;                                                                                                                       \n"
        "uint pk_Instancing_Userdata;                                                                                                                   \n"
        "void PK_INSTANCING_ASSIGN_LOCALS(uint index)                                                                                                   \n"
        "{                                                                                                                                              \n"
        "    PK_Draw draw = pk_Instancing_Indices_Data[index];                                                                                          \n"
        "    pk_ObjectToWorld = pk_Instancing_Transforms_Data[draw.y];                                                                                  \n"
        "    pk_Instancing_Userdata = draw.w;                                                                                                           \n"
        "}                                                                                                                                              \n";

    constexpr const static char* Instancing_Base_GLSL =
        "#define PK_INSTANCING_ENABLED                                                                                                                              \n"
        "#define PK_Draw uint4                                                                                                                                      \n"
        "layout(std430, set = 0, binding = 100) readonly restrict buffer pk_Instancing_Transforms { mat3x4 pk_Instancing_Transforms_Data[]; };                      \n"
        "layout(std430, set = 3, binding = 101) readonly restrict buffer pk_Instancing_Indices { PK_Draw pk_Instancing_Indices_Data[]; };                           \n"
        "layout(std430, set = 3, binding = 102) readonly restrict buffer pk_Instancing_Properties { PK_MaterialPropertyBlock pk_Instancing_Properties_Data[]; };    \n"
        "layout(set = 3, binding = 103) uniform texture2D pk_Instancing_Textures2D[];                                                                               \n"
        "layout(set = 3, binding = 104) uniform texture3D pk_Instancing_Textures3D[];                                                                               \n"
        "layout(set = 3, binding = 105) uniform textureCube pk_Instancing_TexturesCube[];                                                                           \n"
        "mat3x4 pk_ObjectToWorld;                                                                                                                                   \n"
        "uint pk_Instancing_Userdata;                                                                                                                               \n"
        "void PK_INSTANCING_ASSIGN_LOCALS(uint index)                                                                                                               \n"
        "{                                                                                                                                                          \n"
        "    PK_Draw draw = pk_Instancing_Indices_Data[index];                                                                                                      \n"
        "    pk_ObjectToWorld = pk_Instancing_Transforms_Data[draw.y];                                                                                              \n"
        "    pk_Instancing_Userdata = draw.w;                                                                                                                       \n"
        "    PK_MaterialPropertyBlock prop = pk_Instancing_Properties_Data[draw.x];                                                                                 \n";

    constexpr const static char* Instancing_Stage_GLSL = "PK_INSTANCING_ASSIGN_STAGE_LOCALS \n";

    constexpr const static char* Instancing_Vertex_GLSL =
        "out flat uint vs_INSTANCE_ID;                                                                                                          \n"
        "#define PK_INSTANCING_ASSIGN_STAGE_LOCALS PK_INSTANCING_ASSIGN_LOCALS(uint(gl_InstanceIndex)); vs_INSTANCE_ID = uint(gl_InstanceIndex);\n";

    constexpr const static char* Instancing_Fragment_GLSL =
        "in flat uint vs_INSTANCE_ID;                                                           \n"
        "#define PK_INSTANCING_ASSIGN_STAGE_LOCALS PK_INSTANCING_ASSIGN_LOCALS(vs_INSTANCE_ID); \n";

    constexpr const static char* AtomicCounter_GLSL =
        "layout(std430, set = 3) buffer pk_BuiltInAtomicCounter { uint pk_BuiltInAtomicCounter_Data; };\n"
        "uint PK_AtomicCounterAdd(uint increment) { return atomicAdd(pk_BuiltInAtomicCounter_Data, increment); }\n"
        "uint PK_AtomicCounterNext() { return atomicAdd(pk_BuiltInAtomicCounter_Data, 1u); }\n";

    constexpr const static char* PK_GL_EXTENSIONS_COMMON =
        "#extension GL_EXT_samplerless_texture_functions : require \n"
        "#extension GL_EXT_shader_16bit_storage : require \n"
        "#extension GL_EXT_control_flow_attributes : require \n"
        "#extension GL_EXT_shader_explicit_arithmetic_types : require \n"
        "#extension GL_EXT_nonuniform_qualifier : require \n"
        "#extension GL_ARB_shader_viewport_layer_array : require \n";

    constexpr const static char* PK_GL_EXTENSIONS_RAYTRACING = "#extension GL_EXT_ray_tracing : enable \n";

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
