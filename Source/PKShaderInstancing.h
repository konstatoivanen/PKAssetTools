#pragma once
#include <string>
#include <vector>
#include <PKAsset.h>
#include "PKShaderUtilities.h"

namespace PKAssets::Shader::Instancing
{
    constexpr const static char* Instancing_Common_Resources_SLANG = R"(
        #define PK_INSTANCING_ENABLED
        struct PKTransform { float3x4 m; };
        uniform StructuredBufferRef<PKTransform> pk_Instancing_Transforms;
        uniform StructuredBufferRef<uint4> pk_Instancing_Indices;
        static float3x4 pk_ObjectToWorld;
        static float pk_Instancing_UniformScale;
        static uint pk_Instancing_Material;
        static uint pk_Instancing_Transform;
        static uint pk_Instancing_Submesh;
        static uint pk_Instancing_Userdata;
        void PK_INSTANCING_BROADCAST_LOCALS()
        {
           pk_ObjectToWorld[0] = WaveReadLaneFirst(pk_ObjectToWorld[0]);
           pk_ObjectToWorld[1] = WaveReadLaneFirst(pk_ObjectToWorld[1]);
           pk_ObjectToWorld[2] = WaveReadLaneFirst(pk_ObjectToWorld[2]);
           pk_Instancing_UniformScale = WaveReadLaneFirst(pk_Instancing_UniformScale);
           pk_Instancing_Material = WaveReadLaneFirst(pk_Instancing_Material);
           pk_Instancing_Transform = WaveReadLaneFirst(pk_Instancing_Transform);
           pk_Instancing_Submesh = WaveReadLaneFirst(pk_Instancing_Submesh);
           pk_Instancing_Userdata = WaveReadLaneFirst(pk_Instancing_Userdata);
        }
        void PK_INSTANCING_ASSIGN_LOCALS(uint index)
        {
            uint4 data = pk_Instancing_Indices[index];
            pk_Instancing_UniformScale = f16tof32(data.x >> 16u);
            pk_Instancing_Material = data.x & 0xFFFFu;
            pk_Instancing_Transform = data.y;
            pk_Instancing_Submesh = data.z;
            pk_Instancing_Userdata = data.w;
            pk_ObjectToWorld = pk_Instancing_Transforms[pk_Instancing_Transform].m;
    )";

    constexpr const static char* Instancing_Material_Resources_SLANG = R"(
        uniform StructuredBufferRef<PK_MaterialPropertyBlock> pk_Instancing_Properties;
        static PK_MaterialPropertyBlock pk_Instancing_PropertyBlock;
    )";

    constexpr const static char* Instancing_Material_Assign_SLANG = "pk_Instancing_PropertyBlock = pk_Instancing_Properties[pk_Instancing_Material];\n}\n";

    constexpr const static char* Instancing_Default_SLANG = R"(
        #define PK_INSTANCING_VARYING_ID nointerpolation uint vs_INSTANCE_ID : PK_INSTANCE_ID;
        #define PK_INSTANCING_ASSIGN_VARYING_ID(vout, instanceId) vout.vs_INSTANCE_ID = instanceId
        #define PK_INSTANCING_ASSIGN_FRAGMENT_LOCALS(vout) PK_INSTANCING_ASSIGN_LOCALS(vout.vs_INSTANCE_ID)
    )";

    constexpr const static char* Instancing_NoFrag_SLANG = R"(
        #define PK_INSTANCING_VARYING_ID
        #define PK_INSTANCING_ASSIGN_VARYING_ID(vout, instanceId)
        #define PK_INSTANCING_ASSIGN_FRAGMENT_LOCALS(vout) 
    )";

    constexpr const static char* Instancing_Disabled_SLANG = R"(
        uniform float3x4 pk_ObjectToWorld; 
        #define PK_INSTANCING_VARYING_ID
        #define PK_INSTANCING_ASSIGN_VARYING_ID(vout, instanceId)
        #define PK_INSTANCING_ASSIGN_FRAGMENT_LOCALS(vout) 
        #define PK_INSTANCING_BROADCAST_LOCALS() 
        #define PK_INSTANCING_ASSIGN_LOCALS(index) 
    )";

    void InsertMaterialAssembly(std::string& source, std::string& outHeader, std::vector<PKMaterialProperty>& outProperties, bool* outEnabled);
}