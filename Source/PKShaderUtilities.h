#pragma once
#include <slang/slang.h>
#include <slang/slang-com-ptr.h>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include <PKAsset.h>
#include "PKSPVUtilities.h"

namespace PKAssets::Shader
{
    constexpr const static char* PKShaderStage_NAMES[] =
    {
        "Vertex",
        "TesselationControl",
        "TesselationEvaluation",
        "Geometry",
        "MeshTask",
        "MeshAssembly",
        "Fragment",
        "Compute",
        "RayGeneration",
        "RayMiss",
        "RayClosestHit",
        "RayAnyHit",
        "RayIntersection",
        "EnumCount"
    };

    constexpr const static char* PK_SLANG_PRELUDE = R"(
        typedef uint8_t byte;
        typedef vector<uint8_t,2> byte2;
        typedef vector<uint8_t,3> byte3;
        typedef vector<uint8_t,4> byte4;
        typedef int8_t sbyte;
        typedef vector<int8_t,2> sbyte2;
        typedef vector<int8_t,3> sbyte3;
        typedef vector<int8_t,4> sbyte4;
        typedef int16_t short;
        typedef vector<int16_t,2> short2;
        typedef vector<int16_t,3> short3;
        typedef vector<int16_t,4> short4;
        typedef uint16_t ushort;
        typedef vector<uint16_t,2> ushort2;
        typedef vector<uint16_t,3> ushort3;
        typedef vector<uint16_t,4> ushort4;
        typedef int64_t long;
        typedef vector<int64_t,2> long2;
        typedef vector<int64_t,3> long3;
        typedef vector<int64_t,4> long4;
        typedef uint64_t ulong;
        typedef vector<uint64_t,2> ulong2;
        typedef vector<uint64_t,3> ulong3;
        typedef vector<uint64_t,4> ulong4;

        #define float3x4 row_major matrix<float,3,4>

        property float4 gl_FragCoord            { get { return spirv_asm { result:$$float4 = OpLoad builtin(FragCoord:float4); }; } } 
        property uint3  gl_GlobalInvocationID   { get { return spirv_asm { result:$$uint3 = OpLoad builtin(GlobalInvocationId:uint3); }; } }
        property uint3  gl_LocalInvocationID    { get { return spirv_asm { result:$$uint3 = OpLoad builtin(LocalInvocationId:uint3); }; } } 
        property uint3  gl_WorkGroupID          { get { return spirv_asm { result:$$uint3 = OpLoad builtin(WorkgroupId:uint3); }; } }
        property uint   gl_LocalInvocationIndex { get { return spirv_asm { result:$$uint = OpLoad builtin(LocalInvocationIndex:uint); }; } } 
        property uint3  gl_NumWorkGroups        { get { return spirv_asm { result:$$uint3 = OpLoad builtin(NumWorkgroups:uint3); }; } }
        property uint3  gl_WorkGroupSize        { get { return WorkgroupSize(); } }
        property uint   gl_SubgroupID           { get { return spirv_asm { OpCapability GroupNonUniform; result:$$uint = OpLoad builtin(SubgroupId:uint); }; } } 
        property uint   gl_NumSubgroups         { get { return spirv_asm { OpCapability GroupNonUniform; result:$$uint = OpLoad builtin(NumSubgroups:uint); }; } }
        property uint   gl_SubgroupSize         { get { return WaveGetLaneCount(); } }
        property uint   gl_SubgroupInvocationID { get { return WaveGetLaneIndex(); } }
        property uint3  gl_LaunchIDEXT          { get { return spirv_asm { OpCapability RayTracingKHR; result:$$uint3 = OpLoad builtin(LaunchIdKHR:uint3); }; } } 
        property uint   gl_PrimitiveID          { get { return spirv_asm { result:$$uint = OpLoad builtin(PrimitiveId:uint); }; } }
        property uint   gl_VertexIndex          { get { return spirv_asm { result:$$uint = OpLoad builtin(VertexIndex:uint); }; } } 
        property uint   gl_InstanceIndex        { get { return spirv_asm { result:$$uint = OpLoad builtin(InstanceIndex:uint); }; } }

        enum PKShaderStage : uint
        {
            Vertex = 0,
            TesselationControl = 1,
            TesselationEvaluation = 2,
            Geometry = 3,
            MeshTask = 4,
            MeshAssembly = 5,
            Fragment = 6,
            Compute = 7,
            RayGeneration = 8,
            RayMiss = 9,
            RayClosestHit = 10,
            RayAnyHit = 11,
            RayIntersection = 12,
            Invalid = 13
        };
        
        PKShaderStage PK_GetShaderStage()
        {
            __stage_switch
            {
                case vertex:        return PKShaderStage::Vertex;
                case hull:          return PKShaderStage::TesselationControl;
                case domain:        return PKShaderStage::TesselationEvaluation;
                case geometry:      return PKShaderStage::Geometry;
                case amplification: return PKShaderStage::MeshTask;
                case mesh:          return PKShaderStage::MeshAssembly;
                case fragment:      return PKShaderStage::Fragment;
                case compute:       return PKShaderStage::Compute;
                case raygen:        return PKShaderStage::RayGeneration;
                case miss:          return PKShaderStage::RayMiss;
                case closesthit:    return PKShaderStage::RayClosestHit;
                case anyhit:        return PKShaderStage::RayAnyHit;
                case intersection:  return PKShaderStage::RayIntersection;
                default:            return PKShaderStage::Invalid;
            }
        }

        float4 PK_GetProgramCoord()
        {
            __stage_switch
            {
                case fragment:
                    return gl_FragCoord;
                case compute:
                case amplification:
                case mesh:
                    return float4(gl_GlobalInvocationID, 0u);
                case raygen:
                case miss:
                case closesthit:
                case anyhit:
                case intersection:
                    return float4(gl_LaunchIDEXT, 0u);
                default:
                    return 0.0f.xxxx;
            }
        }

        struct SamplerHandle<T : IOpaqueDescriptor>
        {
            uint8_t index;
            __init(uint8_t idx = 0) { this.index = idx; }
            T Get() { return defaultGetDescriptorFromHandle(nonuniform(DescriptorHandle<T>(uint2(uint(index), 0u)))); }
        };
        
        struct ResourceHandle<T : IOpaqueDescriptor>
        {
            uint16_t index;
            __init(uint16_t idx = 0) { this.index = idx; }
            T Get() { return defaultGetDescriptorFromHandle(nonuniform(DescriptorHandle<T>(uint2(uint(index), 0u)))); }
        };

        typealias PKStructuredBuffer<T> = StructuredBuffer<T, ScalarDataLayout>;
        typealias PKRWStructuredBuffer<T> = RWStructuredBuffer<T, ScalarDataLayout>;
        typealias PKCBuffer<T> = ConstantBuffer<T, ScalarDataLayout>;
        
        typealias SamplerRef = SamplerHandle<SamplerState>;
        typealias SamplerCmpRef = SamplerHandle<SamplerComparisonState>;
        typealias Texture2DRef<T : ITexelElement = float4> = ResourceHandle<Texture2D<T>>;
        typealias Texture3DRef<T : ITexelElement = float4> = ResourceHandle<Texture3D<T>>;
        typealias TextureCubeRef<T : ITexelElement = float4> = ResourceHandle<TextureCube<T>>;
        typealias Texture2DArrayRef<T : ITexelElement = float4> = ResourceHandle<Texture2DArray<T>>;
        typealias TextureCubeArrayRef<T : ITexelElement = float4> = ResourceHandle<TextureCubeArray<T>>;
        typealias Texture2DMSRef<T : ITexelElement = float4> = ResourceHandle<Texture2DMS<T>>;
        typealias RWTexture2DRef<T : ITexelElement = float4> = ResourceHandle<RWTexture2D<T>>;
        typealias RWTexture2DArrayRef<T : ITexelElement = float4> = ResourceHandle<RWTexture2DArray<T>>;
        typealias RWTexture3DRef<T : ITexelElement = float4> = ResourceHandle<RWTexture3D<T>>;
        typealias StructuredBufferRef<T> = ResourceHandle<PKStructuredBuffer<T>>;
        typealias RWStructuredBufferRef<T> = ResourceHandle<PKRWStructuredBuffer<T>>;
        typealias CBufferRef<T> = ResourceHandle<PKCBuffer<T>>;
        typealias ByteAddressBufferRef = ResourceHandle<ByteAddressBuffer>;
        typealias RWByteAddressBufferRef = ResourceHandle<RWByteAddressBuffer>;

        struct AccelerationStructureRef
        {
            uint64_t index;
            __init(uint64_t addr = 0) { this.index = addr; }
            RaytracingAccelerationStructure Get()
            {
                return spirv_asm
                {
                    OpCapability RayTracingKHR;
                    result:$$RaytracingAccelerationStructure = OpConvertUToAccelerationStructureKHR $index;
                };
            }
        };
        
        extension<T : ITexelElement> RWTexture2D<T>
        {
            uint2 GetSize() { uint w, h; this.GetDimensions(w, h); return uint2(w, h); }
        };
        
        extension<T : ITexelElement> RWTexture2DArray<T>
        {
            uint3 GetSize() { uint w, h, e; this.GetDimensions(w, h, e); return uint3(w, h, e); }
        };
        
        extension<T : ITexelElement> RWTexture3D<T>
        {
            uint3 GetSize() { uint w, h, d; this.GetDimensions(w, h, d); return uint3(w, h, d); }
        };
        
        extension<T : ITexelElement> Texture2D<T>
        {
            uint2 GetSize(uint mip = 0) { uint w, h, l; this.GetDimensions(mip, w, h, l); return uint2(w, h); }
            T Load(int2 coord, int mip = 0) { return this.Load(int3(coord, mip)); }
        };
        
        extension<T : ITexelElement> Texture3D<T>
        {
            uint3 GetSize(uint mip = 0) { uint w, h, d, l; this.GetDimensions(mip, w, h, d, l); return uint3(w, h, d); }
            T Load(int3 coord, int mip = 0) { return this.Load(int4(coord, mip)); }
        };
        
        extension<T : ITexelElement> Texture2DArray<T>
        {
            uint3 GetSize(uint mip = 0) { uint w, h, e, l; this.GetDimensions(mip, w, h, e, l); return uint3(w, h, e); }
            T Load(int3 coord, int mip = 0) { return this.Load(int4(coord, mip)); }
        };
        
        extension<T : ITexelElement> Texture2DMS<T>
        {
            uint2 GetSize() { uint w, h, s; this.GetDimensions(w, h, s); return uint2(w, h); }
            uint GetSampleCount() { uint w, h, s; this.GetDimensions(w, h, s); return s; }
        };
    )";

    struct SourcePushConstant
    {
        std::string name;
        std::string field;
        PKElementType type = PKElementType::Invalid;
        uint32_t arraySize = 1u;
        bool isReadOnly = true;
    };

    struct EntryPointInfo
    {
        PKShaderStage stage;
        std::string name;
        std::string keyword;
    };

    struct ReflectConstantInfo
    {
        std::string name;
        uint16_t offset = 0u;
        uint16_t arraySize = 0u;
        uint32_t writeStageMask = (uint32_t)PKShaderStageFlags::None;
        uint32_t accessStageMask = (uint32_t)PKShaderStageFlags::None;
        PKElementType type = PKElementType::Invalid;
    };

    struct ReflectionData
    {
        SpvStageReflection stages[(int)PKShaderStage::EnumCount]{};
        uint32_t stageMask = 0u;
        std::vector<PKVertexInputAttribute> vertexAttributes;
        std::vector<ReflectConstantInfo> sortedConstants;
        std::map<std::string, ReflectConstantInfo> uniqueConstants;
        uint32_t constantRangeSize = 0u;
        uint16_t groupSize[3]{};
        bool logVerbose = false;
    };

    typedef std::vector<SourcePushConstant> SourcePushConstants;

    PKShaderStage StringToPKShaderStage(const char* str);
    PKComparison StringToPKComparison(const char* str);
    PKCullMode StringToPKCullMode(const char* str);
    PKBlendFactor StringToPKBlendFactor(const char* str);
    PKBlendOp StringToPKBlendOp(const char* str);
    PKColorMask StringToPKColorMask(const char* str);
    PKRasterMode StringToPKRasterMode(const char* str);
    PKElementType StringToPKElementType(const char* str);
    SlangStage PKShaderStageToSlangStage(PKShaderStage stage);
    const char* PKShaderStageToSlangAttribute(PKShaderStage);

    PKElementType ReflectElementType(std::string_view string, bool* outReadonly = nullptr, std::string* outTargetType = nullptr);
    std::string FormatErrorMessage(const std::string& name, const std::string& source, const std::string& error);
    void ExtractLogVerbose(std::string& source, bool* outValue);
    void ExtractGenerateDebugInfo(std::string& source, bool* outValue);
    void ExtractStateAttributes(std::string& source, PKShaderFixedStateAttributes* attributes);
    void ExtractPushConstants(std::string& source, SourcePushConstants& outConstants);
    std::string CompilePushConstantBlock(const SourcePushConstants& constants);
}