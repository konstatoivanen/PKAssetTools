#pragma once
#include <string>
#include <vector>
#include <PKAsset.h>

namespace PK::Assets::Shader::Instancing
{
    // NOTE that for optimization purposes PK_Draw uses uint4(material, transform, global meshlet submesh, userdata) internally (this saves a redundant struct descriptor)
    constexpr const static char* Instancing_Standalone_GLSL =
        "#define PK_INSTANCING_ENABLED                                                                                                          \n"
        "#define PK_Draw uint4                                                                                                                  \n"
        "layout(std430, set = 0, binding = 100) readonly restrict buffer pk_Instancing_Transforms { mat3x4 pk_Instancing_Transforms_Data[]; };  \n"
        "layout(std430, set = 3, binding = 101) readonly restrict buffer pk_Instancing_Indices { PK_Draw pk_Instancing_Indices_Data[]; };       \n"
        "mat3x4 pk_ObjectToWorld;                                                                                                               \n"
        "uint pk_Instancing_Material;                                                                                                           \n"
        "uint pk_Instancing_Transform;                                                                                                          \n"
        "uint pk_Instancing_Submesh;                                                                                                            \n"
        "uint pk_Instancing_Userdata;                                                                                                           \n"
        "void PK_INSTANCING_BROADCAST_LOCALS_MANUAL()                                                                                           \n"
        "{                                                                                                                                      \n"
        "   pk_ObjectToWorld[0] = subgroupBroadcastFirst(pk_ObjectToWorld[0]);                                                                  \n"
        "   pk_ObjectToWorld[1] = subgroupBroadcastFirst(pk_ObjectToWorld[1]);                                                                  \n"
        "   pk_ObjectToWorld[2] = subgroupBroadcastFirst(pk_ObjectToWorld[2]);                                                                  \n"
        "   pk_Instancing_Material = subgroupBroadcastFirst(pk_Instancing_Material);                                                            \n"
        "   pk_Instancing_Transform = subgroupBroadcastFirst(pk_Instancing_Transform);                                                          \n"
        "   pk_Instancing_Submesh = subgroupBroadcastFirst(pk_Instancing_Submesh);                                                              \n"
        "   pk_Instancing_Userdata = subgroupBroadcastFirst(pk_Instancing_Userdata);                                                            \n"
        "}                                                                                                                                      \n"
        "void PK_INSTANCING_ASSIGN_LOCALS(uint index)                                                                                           \n"
        "{                                                                                                                                      \n"
        "    PK_Draw draw = pk_Instancing_Indices_Data[index];                                                                                  \n"
        "    pk_ObjectToWorld = pk_Instancing_Transforms_Data[draw.y];                                                                          \n"
        "    pk_Instancing_Material = draw.x;                                                                                                   \n"
        "    pk_Instancing_Transform = draw.y;                                                                                                  \n"
        "    pk_Instancing_Submesh = draw.z;                                                                                                    \n"
        "    pk_Instancing_Userdata = draw.w;                                                                                                   \n"
        "}                                                                                                                                      \n";

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
        "uint pk_Instancing_Material;                                                                                                                               \n"
        "uint pk_Instancing_Transform;                                                                                                                              \n"
        "uint pk_Instancing_Submesh;                                                                                                                                \n"
        "uint pk_Instancing_Userdata;                                                                                                                               \n"
        "void PK_INSTANCING_BROADCAST_LOCALS_MANUAL()                                                                                                               \n"
        "{                                                                                                                                                          \n"
        "   pk_ObjectToWorld[0] = subgroupBroadcastFirst(pk_ObjectToWorld[0]);                                                                                      \n"
        "   pk_ObjectToWorld[1] = subgroupBroadcastFirst(pk_ObjectToWorld[1]);                                                                                      \n"
        "   pk_ObjectToWorld[2] = subgroupBroadcastFirst(pk_ObjectToWorld[2]);                                                                                      \n"
        "   pk_Instancing_Material = subgroupBroadcastFirst(pk_Instancing_Material);                                                                                \n"
        "   pk_Instancing_Transform = subgroupBroadcastFirst(pk_Instancing_Transform);                                                                              \n"
        "   pk_Instancing_Submesh = subgroupBroadcastFirst(pk_Instancing_Submesh);                                                                                  \n"
        "   pk_Instancing_Userdata = subgroupBroadcastFirst(pk_Instancing_Userdata);                                                                                \n"
        "}                                                                                                                                                          \n"
        "void PK_INSTANCING_ASSIGN_LOCALS(uint index)                                                                                                               \n"
        "{                                                                                                                                                          \n"
        "    PK_Draw draw = pk_Instancing_Indices_Data[index];                                                                                                      \n"
        "    pk_ObjectToWorld = pk_Instancing_Transforms_Data[draw.y];                                                                                              \n"
        "    pk_Instancing_Material = draw.x;                                                                                                                       \n"
        "    pk_Instancing_Transform = draw.y;                                                                                                                      \n"
        "    pk_Instancing_Submesh = draw.z;                                                                                                                        \n"
        "    pk_Instancing_Userdata = draw.w;                                                                                                                       \n"
        "    PK_MaterialPropertyBlock prop = pk_Instancing_Properties_Data[draw.x];                                                                                 \n";

    constexpr const static char* Instancing_Stage_GLSL = "PK_INSTANCING_ASSIGN_STAGE_LOCALS \n";

    constexpr const static char* Instancing_Vertex_GLSL =
        "out flat uint vs_INSTANCE_ID;                                                                                          \n"
        "#define PK_INSTANCE_ID uint(gl_InstanceIndex)                                                                          \n"
        "#define PK_INSTANCING_ASSIGN_STAGE_LOCALS PK_INSTANCING_ASSIGN_LOCALS(PK_INSTANCE_ID); vs_INSTANCE_ID = PK_INSTANCE_ID;\n";

    constexpr const static char* Instancing_MeshTask_GLSL = "#define PK_INSTANCING_ASSIGN_STAGE_LOCALS\n";

    constexpr const static char* Instancing_MeshAssembly_GLSL =
        "out flat uint vs_INSTANCE_ID[];                                                                       \n"
        "#define PK_SET_VERTEX_INSTANCE_ID(vertexIndex, instanceId) vs_INSTANCE_ID[vertexIndex] = instanceId;  \n"
        "#define PK_INSTANCING_ASSIGN_STAGE_LOCALS                                                             \n";

    constexpr const static char* Instancing_MeshAssembly_NoFrag_GLSL =
        "#define PK_SET_VERTEX_INSTANCE_ID(vertexIndex, instanceId) \n"
        "#define PK_INSTANCING_ASSIGN_STAGE_LOCALS                  \n";

    constexpr const static char* Instancing_Fragment_GLSL =
        "in flat uint vs_INSTANCE_ID;                                                           \n"
        "#define PK_INSTANCING_ASSIGN_STAGE_LOCALS PK_INSTANCING_ASSIGN_LOCALS(vs_INSTANCE_ID); \n";

    constexpr const static char* Instancing_Fragment_NoFrag_GLSL =
        "#define vs_INSTANCE_ID 0u                 \n"
        "#define PK_INSTANCING_ASSIGN_STAGE_LOCALS \n";

    void InsertEntryPoint(std::string& source, PKShaderStage stage, bool enableInstancing, bool noFragmentInstancing);
    void InsertMaterialAssembly(std::string& source, std::vector<PKMaterialProperty>& materialProperties, bool* outUseInstancing, bool* outNoFragInstancing);
}