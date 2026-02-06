#pragma once
#include <string>
#include <vector>
#include <PKAsset.h>

namespace PKAssets::Shader::Instancing
{
    // NOTE that for optimization purposes PKDrawInfo uses uint4(material, uniform scale, transform, global meshlet submesh, userdata) internally (this saves a redundant struct descriptor)
    constexpr const static char* Instancing_Standalone_GLSL =
        "#define PK_INSTANCING_ENABLED                                                                                                          \n"
        "layout(std430, set = 0, binding = 100) readonly restrict buffer pk_Instancing_Transforms { mat3x4 pk_Instancing_Transforms_Data[]; };  \n"
        "layout(std430, set = 3, binding = 101) readonly restrict buffer pk_Instancing_Indices { uint4 pk_Instancing_Indices_Data[]; };         \n"
        "mat3x4 pk_ObjectToWorld;                                                                                                               \n"
        "float pk_Instancing_UniformScale;                                                                                                      \n"
        "uint pk_Instancing_Material;                                                                                                           \n"
        "uint pk_Instancing_Transform;                                                                                                          \n"
        "uint pk_Instancing_Submesh;                                                                                                            \n"
        "uint pk_Instancing_Userdata;                                                                                                           \n"
        "void PK_INSTANCING_BROADCAST_LOCALS_MANUAL()                                                                                           \n"
        "{                                                                                                                                      \n"
        "   pk_ObjectToWorld[0] = subgroupBroadcastFirst(pk_ObjectToWorld[0]);                                                                  \n"
        "   pk_ObjectToWorld[1] = subgroupBroadcastFirst(pk_ObjectToWorld[1]);                                                                  \n"
        "   pk_ObjectToWorld[2] = subgroupBroadcastFirst(pk_ObjectToWorld[2]);                                                                  \n"
        "   pk_Instancing_UniformScale = subgroupBroadcastFirst(pk_Instancing_UniformScale);                                                    \n"
        "   pk_Instancing_Material = subgroupBroadcastFirst(pk_Instancing_Material);                                                            \n"
        "   pk_Instancing_Transform = subgroupBroadcastFirst(pk_Instancing_Transform);                                                          \n"
        "   pk_Instancing_Submesh = subgroupBroadcastFirst(pk_Instancing_Submesh);                                                              \n"
        "   pk_Instancing_Userdata = subgroupBroadcastFirst(pk_Instancing_Userdata);                                                            \n"
        "}                                                                                                                                      \n"
        "void PK_INSTANCING_ASSIGN_LOCALS(uint index)                                                                                           \n"
        "{                                                                                                                                      \n"
        "    uint4 data = pk_Instancing_Indices_Data[index];                                                                                    \n"
        "    pk_Instancing_UniformScale = unpackHalf2x16(data.x).y;                                                                             \n"
        "    pk_Instancing_Material = data.x & 0xFFFFu;                                                                                         \n"
        "    pk_Instancing_Transform = data.y;                                                                                                  \n"
        "    pk_Instancing_Submesh = data.z;                                                                                                    \n"
        "    pk_Instancing_Userdata = data.w;                                                                                                   \n"
        "    pk_ObjectToWorld = pk_Instancing_Transforms_Data[pk_Instancing_Transform];                                                         \n"
        "}                                                                                                                                      \n";

    constexpr const static char* Instancing_Base_GLSL =
        "#define PK_INSTANCING_ENABLED                                                                                                                              \n"
        "layout(std430, set = 0, binding = 100) readonly restrict buffer pk_Instancing_Transforms { mat3x4 pk_Instancing_Transforms_Data[]; };                      \n"
        "layout(std430, set = 3, binding = 101) readonly restrict buffer pk_Instancing_Indices { uint4 pk_Instancing_Indices_Data[]; };                             \n"
        "layout(std430, set = 3, binding = 102) readonly restrict buffer pk_Instancing_Properties { PK_MaterialPropertyBlock pk_Instancing_Properties_Data[]; };    \n"
        "layout(set = 3, binding = 103) uniform texture2D pk_Instancing_Textures2D[];                                                                               \n"
        "layout(set = 3, binding = 104) uniform texture3D pk_Instancing_Textures3D[];                                                                               \n"
        "layout(set = 3, binding = 105) uniform textureCube pk_Instancing_TexturesCube[];                                                                           \n"
        "mat3x4 pk_ObjectToWorld;                                                                                                                                   \n"
        "float pk_Instancing_UniformScale;                                                                                                                          \n"
        "uint pk_Instancing_Material;                                                                                                                               \n"
        "uint pk_Instancing_Transform;                                                                                                                              \n"
        "uint pk_Instancing_Submesh;                                                                                                                                \n"
        "uint pk_Instancing_Userdata;                                                                                                                               \n"
        "void PK_INSTANCING_BROADCAST_LOCALS_MANUAL()                                                                                                               \n"
        "{                                                                                                                                                          \n"
        "   pk_ObjectToWorld[0] = subgroupBroadcastFirst(pk_ObjectToWorld[0]);                                                                                      \n"
        "   pk_ObjectToWorld[1] = subgroupBroadcastFirst(pk_ObjectToWorld[1]);                                                                                      \n"
        "   pk_ObjectToWorld[2] = subgroupBroadcastFirst(pk_ObjectToWorld[2]);                                                                                      \n"
        "   pk_Instancing_UniformScale = subgroupBroadcastFirst(pk_Instancing_UniformScale);                                                                        \n"
        "   pk_Instancing_Material = subgroupBroadcastFirst(pk_Instancing_Material);                                                                                \n"
        "   pk_Instancing_Transform = subgroupBroadcastFirst(pk_Instancing_Transform);                                                                              \n"
        "   pk_Instancing_Submesh = subgroupBroadcastFirst(pk_Instancing_Submesh);                                                                                  \n"
        "   pk_Instancing_Userdata = subgroupBroadcastFirst(pk_Instancing_Userdata);                                                                                \n"
        "}                                                                                                                                                          \n"
        "void PK_INSTANCING_ASSIGN_LOCALS(uint index)                                                                                                               \n"
        "{                                                                                                                                                          \n"
        "    uint4 data = pk_Instancing_Indices_Data[index];                                                                                                        \n"
        "    pk_Instancing_UniformScale = unpackHalf2x16(data.x).y;                                                                                                 \n"
        "    pk_Instancing_Material = data.x & 0xFFFFu;                                                                                                             \n"
        "    pk_Instancing_Transform = data.y;                                                                                                                      \n"
        "    pk_Instancing_Submesh = data.z;                                                                                                                        \n"
        "    pk_Instancing_Userdata = data.w;                                                                                                                       \n"
        "    pk_ObjectToWorld = pk_Instancing_Transforms_Data[pk_Instancing_Transform];                                                                             \n"
        "    PK_MaterialPropertyBlock prop = pk_Instancing_Properties_Data[pk_Instancing_Material];                                                                 \n";

    constexpr const static char* Instancing_Stage_GLSL = "\nPK_INSTANCING_ASSIGN_STAGE_LOCALS \n";

    constexpr const static char* Instancing_Vertex_GLSL =
        "out flat uint vs_INSTANCE_ID;                                                                                          \n"
        "#define PK_INSTANCE_ID uint(gl_InstanceIndex)                                                                          \n"
        "#define PK_INSTANCING_ASSIGN_STAGE_LOCALS PK_INSTANCING_ASSIGN_LOCALS(PK_INSTANCE_ID); vs_INSTANCE_ID = PK_INSTANCE_ID;\n";

    constexpr const static char* Instancing_MeshTask_GLSL = "#define PK_INSTANCING_ASSIGN_STAGE_LOCALS\n";

    constexpr const static char* Instancing_MeshAssembly_GLSL =
        "out flat uint vs_INSTANCE_ID[];                                                                                     \n"
        "#define PK_INSTANCING_ASSIGN_VERTEX_INSTANCE_ID(vertexIndex, instanceId) vs_INSTANCE_ID[vertexIndex] = instanceId;  \n"
        "#define PK_INSTANCING_ASSIGN_STAGE_LOCALS                                                                           \n";

    constexpr const static char* Instancing_MeshAssembly_NoFrag_GLSL =
        "#define PK_INSTANCING_ASSIGN_VERTEX_INSTANCE_ID(vertexIndex, instanceId)   \n"
        "#define PK_INSTANCING_ASSIGN_STAGE_LOCALS                                  \n";

    constexpr const static char* Instancing_Fragment_GLSL =
        "in flat uint vs_INSTANCE_ID;                                                           \n"
        "#define PK_INSTANCING_ASSIGN_STAGE_LOCALS PK_INSTANCING_ASSIGN_LOCALS(vs_INSTANCE_ID); \n";

    constexpr const static char* Instancing_Fragment_NoFrag_GLSL =
        "#define vs_INSTANCE_ID 0u                 \n"
        "#define PK_INSTANCING_ASSIGN_STAGE_LOCALS \n";

    void InsertEntryPoint(std::string& source, PKShaderStage stage, bool enableInstancing, bool noFragmentInstancing);
    void InsertMaterialAssembly(std::string& source, std::vector<PKMaterialProperty>& materialProperties, bool* outUseInstancing, bool* outNoFragInstancing);
}