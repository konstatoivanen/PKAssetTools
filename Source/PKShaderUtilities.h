#pragma once
#include <slang/slang.h>
#include <slang/slang-com-ptr.h>
#include <SPIRV-Reflect/spirv_reflect.h>
#include <simplecpp/simplecpp.h>
#include <map>
#include <unordered_map>
#include <vector>
#include <string>
#include <PKAsset.h>

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

    constexpr const static char* PK_SHADER_STAGE_DEFINES[(uint32_t)PKShaderStage::EnumCount] =
    {
        "#define SHADER_VERTEX\n",
        "#define SHADER_TESSELATION_CONTROL\n",
        "#define SHADER_TESSELATION_EVALUATE\n",
        "#define SHADER_GEOMETRY\n",
        "#define SHADER_MESH_TASK\n",
        "#define SHADER_MESH_ASSEMBLY\n",
        "#define SHADER_FRAGMENT\n",
        "#define SHADER_COMPUTE\n",

        "#define SHADER_RAY_GENERATION\n",
        "#define SHADER_RAY_MISS\n",
        "#define SHADER_RAY_CLOSEST_HIT\n",
        "#define SHADER_RAY_ANY_HIT\n",
        "#define SHADER_RAY_INTERSECTION\n"
    };

    constexpr const static char* PK_SLANG_PRELUDE = R"(
        typedef uint8_t   byte;
        typedef uint8_t2  byte2;
        typedef uint8_t3  byte3;
        typedef uint8_t4  byte4;
        typedef int8_t    sbyte;
        typedef int8_t2   sbyte2;
        typedef int8_t3   sbyte3;
        typedef int8_t4   sbyte4;
        typedef int16_t   short;
        typedef int16_t2  short2;
        typedef int16_t3  short3;
        typedef int16_t4  short4;
        typedef uint16_t  ushort;
        typedef uint16_t2 ushort2;
        typedef uint16_t3 ushort3;
        typedef uint16_t4 ushort4;
        typedef int64_t   long;
        typedef int64_t2  long2;
        typedef int64_t3  long3;
        typedef int64_t4  long4;
        typedef uint64_t  ulong;
        typedef uint64_t2 ulong2;
        typedef uint64_t3 ulong3;
        typedef uint64_t4 ulong4;
        
        #define float3x4 row_major matrix<float, 3, 4>

        property float4 gl_FragCoord{ get { return spirv_asm { result:$$float4 = OpLoad builtin(FragCoord : float4); }; } }
        property uint3  gl_GlobalInvocationID{ get { return spirv_asm { result:$$uint3 = OpLoad builtin(GlobalInvocationId : uint3); }; } }
        property uint3  gl_LocalInvocationID{ get { return spirv_asm { result:$$uint3 = OpLoad builtin(LocalInvocationId : uint3); }; } }
        property uint3  gl_WorkGroupID{ get { return spirv_asm { result:$$uint3 = OpLoad builtin(WorkgroupId : uint3); }; } }
        property uint   gl_LocalInvocationIndex{ get { return spirv_asm { result:$$uint = OpLoad builtin(LocalInvocationIndex : uint); }; } }
        property uint3  gl_NumWorkGroups{ get { return spirv_asm { result:$$uint3 = OpLoad builtin(NumWorkgroups : uint3); }; } }
        property uint3  gl_WorkGroupSize{ get { return WorkgroupSize(); } }
        property uint   gl_SubgroupID{ get { return spirv_asm { OpCapability GroupNonUniform; result:$$uint = OpLoad builtin(SubgroupId : uint); }; } }
        property uint   gl_NumSubgroups{ get { return spirv_asm { OpCapability GroupNonUniform; result:$$uint = OpLoad builtin(NumSubgroups : uint); }; } }
        property uint   gl_SubgroupSize{ get { return WaveGetLaneCount(); } }
        property uint   gl_SubgroupInvocationID{ get { return WaveGetLaneIndex(); } }
        property uint3  gl_LaunchIDEXT{ get { return spirv_asm { OpCapability RayTracingKHR; result:$$uint3 = OpLoad builtin(LaunchIdKHR : uint3); }; } }
        property uint   gl_PrimitiveID{ get { return spirv_asm { result:$$uint = OpLoad builtin(PrimitiveId : uint); }; } }
        property uint   gl_VertexIndex{ get { return spirv_asm { result:$$uint = OpLoad builtin(VertexIndex : uint); }; } }
        property uint   gl_InstanceIndex{ get { return spirv_asm { result:$$uint = OpLoad builtin(InstanceIndex : uint); }; } }

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

        float4 operator*(float4x4 m, float4 v) { return mul(m, v); }
        float4 operator*(float4 v, float4x4 m) { return mul(v, m); }
        float4x4 operator*(float4x4 a, float4x4 b) { return mul(a, b); }

        __generic<T : __BuiltinFloatingPointType, let R : int, let C : int>
        vector<T, C> operator*(matrix<T, R, C> m, vector<T, R> v) { return mul(v, m); }

        __generic<T : __BuiltinFloatingPointType, let R : int, let C : int>
        vector<T, R> operator*(vector<T, C> v, matrix<T, R, C> m) { return mul(m, v); }

        __generic<T : __BuiltinFloatingPointType, let R : int, let N : int, let C : int>
        matrix<T, R, C> operator*(matrix<T, N, C> a, matrix<T, R, N> b) { return mul(b, a); }

        extension<T : ITexelElement> RWTexture2D<T> { uint2 GetSize() { uint w, h; this.GetDimensions(w, h); return uint2(w, h);}};
        extension<T : ITexelElement> RWTexture2DArray<T> { uint3 GetSize() { uint w, h, e; this.GetDimensions(w, h, e); return uint3(w, h, e);}};
        extension<T : ITexelElement> RWTexture3D<T> { uint3 GetSize() { uint w, h, d; this.GetDimensions(w, h, d); return uint3(w, h, d);}};
        extension<T : ITexelElement> Texture2D<T> { uint2 GetSize(uint mip = 0) { uint w, h, l; this.GetDimensions(mip, w, h, l); return uint2(w, h);}};
        extension<T : ITexelElement> Texture3D<T> { uint3 GetSize(uint mip = 0) { uint w, h, d, l; this.GetDimensions(mip, w, h, d, l); return uint3(w, h, d);}};
        extension<T : ITexelElement> Texture2DArray<T> { uint3 GetSize(uint mip = 0) { uint w, h, e, l; this.GetDimensions(mip, w, h, e, l); return uint3(w, h, e);}};
        
        extension<T : ITexelElement> Texture2DMS<T>
        {
            uint2 GetSize() { uint w, h, s; this.GetDimensions(w, h, s); return uint2(w, h); }
            uint GetSampleCount() { uint w, h, s; this.GetDimensions(w, h, s); return s; }
        };

        struct PKDescriptorHeap<T : IOpaqueDescriptor>
        {
            __subscript(uint index) -> DescriptorHandle<T>
            {
                get { return DescriptorHandle<T>(uint2(index, 0)); }
            }
        };

        static const PKDescriptorHeap<SamplerState> pk_Heap_Samplers = {};
        static const PKDescriptorHeap<Texture2D<float4>> pk_Heap_Textures2D = {};
        static const PKDescriptorHeap<Texture3D<float4>> pk_Heap_Textures3D = {};
        static const PKDescriptorHeap<TextureCube<float4>> pk_Heap_TexturesCube = {};
        )";

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
        std::string keyword;
    };

    struct ReflectBinding
    {
        std::string name;
        uint32_t writeStageMask = (uint32_t)PKShaderStageFlags::None;
        uint32_t accessStageMask = (uint32_t)PKShaderStageFlags::None;
        PKDescriptorType type = PKDescriptorType::Invalid;
        uint32_t bindingIndex = 0u;
        uint32_t count = 0u;
        uint32_t wordOffsetBinding[(int)PKShaderStage::EnumCount]{};
        uint32_t wordOffsetSet[(int)PKShaderStage::EnumCount]{};
    };

    struct ReflectPushConstant
    {
        uint16_t offset = 0u;
        uint16_t size = 0u;
    };

    struct ReflectionData
    {
        SpvReflectShaderModule* modules[(int)PKShaderStage::EnumCount]{};
        std::vector<uint32_t> deadVariableIds[(int)PKShaderStage::EnumCount]{};
        std::vector<PKVertexInputAttribute> vertexAttributes;
        std::vector<ReflectBinding> sortedBindings;
        std::map<std::string, ReflectBinding> uniqueBindings;
        std::map<std::string, ReflectPushConstant> uniqueConstants;
        uint32_t constantRangeSize = 0u;
        bool logVerbose;
    };

    typedef std::vector<SourcePushConstant> SourcePushConstants;

    PKUniform StringToPKUniform(const char* str);
    PKShaderStage StringToPKShaderStage(const char* str);
    PKComparison StringToPKComparison(const char* str);
    PKCullMode StringToPKCullMode(const char* str);
    PKBlendFactor StringToPKBlendFactor(const char* str);
    PKBlendOp StringToPKBlendOp(const char* str);
    PKColorMask StringToPKColorMask(const char* str);
    PKRasterMode StringToPKRasterMode(const char* str);

    PKUniform SpvFormatToPKUniform(SpvReflectFormat format);
    std::string PKUniformToSlangString(PKUniform type);

    PKDescriptorType GetResourceType(SpvReflectDescriptorType type);
    SlangStage PKShaderStageToSlangStage(PKShaderStage stage);
    const char* GetSlangStageAttribute(PKShaderStage stage);
    std::string ReflectBindingName(SpvReflectDescriptorBinding* binding);
    std::string FormatErrorMessage(const std::string& name, const std::string& source, const std::string& error);
    void ExtractLogVerbose(std::string& source, bool* outValue);
    void ExtractGenerateDebugInfo(std::string& source, bool* outValue);
    void ExtractStateAttributes(std::string& source, PKShaderFixedStateAttributes* attributes);
    void ExtractMulticompiles(std::string& source, std::vector<std::string>& outVariantDefines, std::vector<PKShaderKeyword>& outKeywords);
    int ExtractEntryPoints(std::string& source, std::vector<EntryPointInfo>& outEntryPoints);
    void FindActiveEntryPoints(std::vector<EntryPointInfo>& entries, const std::string& variantDefines, EntryPointInfo** outStageEntries, uint32_t* outActiveStageMask, std::string& outActiveDefines);
    int PreprocessSource(const std::string& name, const std::string& source, std::vector<std::string>& filenames, simplecpp::TokenList& outTokens);
    void SetActiveEntryPoints(simplecpp::TokenList& tokens, const std::vector<EntryPointInfo>& entries, EntryPointInfo* const* activeEntries);
    void RemoveUnusedResourcesAndFunctions(simplecpp::TokenList& tokens, EntryPointInfo* const* activeEntries);
    void ExtractPushConstants(simplecpp::TokenList& tokens, uint32_t stageMask, SourcePushConstants& outConstants);
    void CompilePushConstantBlock(std::string& source, const SourcePushConstants& constants);
}
