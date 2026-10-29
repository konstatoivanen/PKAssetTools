#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include "PKAssetWriter.h"
#include "PKSPVUtilities.h"

namespace PKAssets::Shader
{
    enum SpvOp : uint16_t
    {
        SpvOpName = 5,
        SpvOpMemberName = 6,
        SpvOpEntryPoint = 15,
        SpvOpExecutionMode = 16,
        SpvOpCapability = 17,
        SpvOpTypeInt = 21,
        SpvOpTypeFloat = 22,
        SpvOpTypeVector = 23,
        SpvOpTypeImage = 25,
        SpvOpTypeStruct = 30,
        SpvOpTypePointer = 32,
        SpvOpConstant = 43,
        SpvOpConstantNull = 46,
        SpvOpSpecConstant = 50,
        SpvOpFunction = 54,
        SpvOpVariable = 59,
        SpvOpImageTexelPointer = 60,
        SpvOpLoad = 61,
        SpvOpStore = 62,
        SpvOpAccessChain = 65,
        SpvOpInBoundsAccessChain = 66,
        SpvOpPtrAccessChain = 67,
        SpvOpInBoundsPtrAccessChain = 70,
        SpvOpDecorate = 71,
        SpvOpMemberDecorate = 72,
        SpvOpVectorInsertDynamic = 78,
        SpvOpCompositeConstruct = 80,
        SpvOpCompositeExtract = 81,
        SpvOpCompositeInsert = 82,
        SpvOpCopyObject = 83,
        SpvOpImageWrite = 99,
        SpvOpUConvert = 113,
        SpvOpSConvert = 114,
        SpvOpBitcast = 124,
        SpvOpIAdd = 128,
        SpvOpISub = 130,
        SpvOpIMul = 132,
        SpvOpUDiv = 134,
        SpvOpSDiv = 135,
        SpvOpUMod = 137,
        SpvOpSRem = 138,
        SpvOpSMod = 139,
        SpvOpShiftRightLogical = 194,
        SpvOpShiftRightArithmetic = 195,
        SpvOpShiftLeftLogical = 196,
        SpvOpBitwiseOr = 197,
        SpvOpBitwiseXor = 198,
        SpvOpBitwiseAnd = 199,
        SpvOpBitFieldInsert = 201,
        SpvOpBitFieldSExtract = 202,
        SpvOpBitFieldUExtract = 203,
        SpvOpAtomicStore = 228,
        SpvOpAtomicExchange = 229,
        SpvOpAtomicCompareExchange = 230,
        SpvOpAtomicCompareExchangeWeak = 231,
        SpvOpAtomicIIncrement = 232,
        SpvOpAtomicIDecrement = 233,
        SpvOpAtomicIAdd = 234,
        SpvOpAtomicISub = 235,
        SpvOpAtomicSMin = 236,
        SpvOpAtomicUMin = 237,
        SpvOpAtomicSMax = 238,
        SpvOpAtomicUMax = 239,
        SpvOpAtomicAnd = 240,
        SpvOpAtomicOr = 241,
        SpvOpAtomicXor = 242,
        SpvOpExecutionModeId = 331,
        SpvOpTypeUntypedPointerKHR = 4417,
        SpvOpUntypedVariableKHR = 4418,
        SpvOpUntypedAccessChainKHR = 4419,
        SpvOpUntypedInBoundsAccessChainKHR = 4420,
        SpvOpUntypedPtrAccessChainKHR = 4423,
        SpvOpUntypedInBoundsPtrAccessChainKHR = 4424,
        SpvOpTypeBufferEXT = 5115,
        SpvOpBufferPointerEXT = 5119,
        SpvOpUntypedImageTexelPointerEXT = 5126,
        SpvOpAtomicFMinEXT = 5614,
        SpvOpAtomicFMaxEXT = 5615,
        SpvOpAtomicFAddEXT = 6035
    };

    enum SpvStorageClass : uint32_t
    {
        SpvStorageClassUniformConstant = 0,
        SpvStorageClassInput = 1,
        SpvStorageClassUniform = 2,
        SpvStorageClassPushConstant = 9,
        SpvStorageClassStorageBuffer = 12
    };

    enum SpvDecoration : uint32_t
    {
        SpvDecorationBuiltIn = 11,
        SpvDecorationSample = 17,
        SpvDecorationLocation = 30,
        SpvDecorationOffset = 35
    };

    enum SpvExecutionMode : uint32_t
    {
        SpvExecutionModeLocalSize = 17,
        SpvExecutionModeLocalSizeId = 38
    };

    struct SpvTypeDesc
    {
        SpvOp op = (SpvOp)0;
        uint32_t elemTypeId = 0u;
        uint32_t elemCount = 0u;
        uint32_t width = 0u;
        uint32_t signedness = 0u;
    };

    struct SpvVarDesc
    {
        uint32_t id = 0u;
        uint32_t ptrTypeId = 0u;
        uint32_t storageClass = 0u;
        uint32_t dataTypeId = 0u;
    };

    struct SpvMemberOffsetSlot
    {
        uint32_t offset = 0u;
        uint32_t wordOffset = 0u;
    };

    struct SpvPushAccessPatch
    {
        uint32_t instWordIndex = 0u;
        uint32_t operandWordOffset = 0u;
        uint32_t memberIndex = 0u;
        uint32_t origConstId = 0u;
        uint32_t newConstId = 0u;
        bool isLiteral = false;
    };

    struct SpvNewConstant
    {
        uint32_t typeId = 0u;
        uint32_t resultId = 0u;
        uint32_t value = 0u;
    };

    struct SpirvParserContext
    {
        std::unordered_map<uint32_t, std::string> names;
        std::unordered_map<uint64_t, std::string> memberNames;
        std::unordered_map<uint64_t, SpvMemberOffsetSlot> memberOffsets;
        std::unordered_map<uint32_t, uint32_t> constUints;
        std::unordered_map<uint32_t, uint32_t> locations;
        std::unordered_set<uint32_t> builtIns;
        std::unordered_set<uint32_t> globalHeapVarIds;
        std::unordered_set<uint32_t> accessedInputVarIds;
        std::unordered_map<uint32_t, SpvTypeDesc> types;
        std::vector<SpvVarDesc> variables;
        std::unordered_map<uint32_t, uint32_t> constTypes;
        std::unordered_map<uint64_t, uint32_t> intConstMap;
        std::vector<SpvPushAccessPatch> pushAccessPatches;

        uint32_t localSizeId[3]{};
        uint32_t pushVarId = 0u;
        uint32_t pushStructTypeId = 0u;
        uint32_t pushMemberCount = 0u;
        uint32_t outerPushStructTypeId = 0u;
        bool hasPushConstants = false;
        bool isWrappedPushStruct = false;
        bool hasSampleRateCapability = false;
        bool needsSampleRateCapability = false;

        std::unordered_set<uint32_t> innerPushPtrIds;
        std::unordered_set<uint32_t> loadedPushStructIds;
        std::vector<std::unordered_set<uint32_t>> memberHandleIds;
        std::vector<std::unordered_set<uint32_t>> memberResourceIds;
        std::vector<bool> memberAccessed;
        std::vector<bool> memberWritten;
    };

    static uint64_t MakeMemberKey(uint32_t structId, uint32_t memberIdx)
    {
        return (uint64_t(structId) << 32ull) | uint64_t(memberIdx);
    }

    static std::string CleanVariableName(std::string name)
    {
        constexpr auto entryParamPrefix = "entryPointParam_";
        const auto dotPos = name.rfind('.');
        const auto prefixLen = strlen(entryParamPrefix);
        const auto suffixLen = strlen(PK_SHADER_ATTRIB_ALIAS);

        if (dotPos != std::string::npos)
        {
            name = name.substr(dotPos + 1ull);
        }

        if (name.compare(0, prefixLen, entryParamPrefix) == 0)
        {
            name.erase(0, prefixLen);
        }

        if (name.size() > suffixLen && name.compare(name.size() - suffixLen, suffixLen, PK_SHADER_ATTRIB_ALIAS) == 0)
        {
            name.erase(name.size() - suffixLen);
        }

        return name;
    }

    static PKElementType ReflectVertexInputType(uint32_t typeId, const std::unordered_map<uint32_t, SpvTypeDesc>& types)
    {
        auto iterator = types.find(typeId);

        if (iterator == types.end())
        {
            return PKElementType::Invalid;
        }

        auto* scalar = &iterator->second;
        auto count = 1u;

        if (scalar->op == SpvOpTypeVector)
        {
            count = scalar->elemCount;
            iterator = types.find(scalar->elemTypeId);

            if (iterator == types.end())
            {
                return PKElementType::Invalid;
            }

            scalar = &iterator->second;
        }

        if (count < 1u || count > 4u)
        {
            return PKElementType::Invalid;
        }

        if (scalar->op == SpvOpTypeFloat)
        {
            switch (scalar->width)
            {
                case 16u: return (PKElementType)((uint8_t)PKElementType::Half + count - 1u);
                case 32u: return (PKElementType)((uint8_t)PKElementType::Float + count - 1u);
                case 64u: return (PKElementType)((uint8_t)PKElementType::Double + count - 1u);
                default:  return PKElementType::Invalid;
            }
        }

        if (scalar->op == SpvOpTypeInt)
        {
            const auto isSigned = scalar->signedness != 0u;

            switch (scalar->width)
            {
                case 8u:  return (PKElementType)((uint8_t)(isSigned ? PKElementType::Sbyte : PKElementType::Byte) + count - 1u);
                case 16u: return (PKElementType)((uint8_t)(isSigned ? PKElementType::Short : PKElementType::Ushort) + count - 1u);
                case 32u: return (PKElementType)((uint8_t)(isSigned ? PKElementType::Int : PKElementType::Uint) + count - 1u);
                case 64u: return (PKElementType)((uint8_t)(isSigned ? PKElementType::Long : PKElementType::Ulong) + count - 1u);
                default:  return PKElementType::Invalid;
            }
        }

        return PKElementType::Invalid;
    }

    static void ProcessHeaderInstruction(SpvOp op, uint16_t wc, const uint32_t* p, size_t instructionIndex, SpirvParserContext& ctx, SpvStageReflection& reflection)
    {
        switch (op)
        {
            case SpvOpExecutionMode:
            {
                if (wc >= 6u && p[1] == SpvExecutionModeLocalSize)
                {
                    reflection.groupSize[0] = (uint16_t)p[2];
                    reflection.groupSize[1] = (uint16_t)p[3];
                    reflection.groupSize[2] = (uint16_t)p[4];
                }
                break;
            }
            case SpvOpExecutionModeId:
            {
                if (wc >= 6u && p[1] == SpvExecutionModeLocalSizeId)
                {
                    ctx.localSizeId[0] = p[2];
                    ctx.localSizeId[1] = p[3];
                    ctx.localSizeId[2] = p[4];
                }
                break;
            }
            case SpvOpCapability:
            {
                // SpvCapabilitySampleRateShading = 35
                if (wc >= 2u && p[0] == 35u) 
                {
                    ctx.hasSampleRateCapability = true;
                }
                break;
            }
            case SpvOpDecorate:
            {
                if (wc >= 3u && p[1] == SpvDecorationBuiltIn)
                {
                    ctx.builtIns.insert(p[0]);
                }
                else if (wc >= 3u && p[1] == SpvDecorationSample)
                {
                    ctx.needsSampleRateCapability = true;
                }
                else if (wc >= 4u && p[1] == SpvDecorationLocation)
                {
                    ctx.locations[p[0]] = p[2];
                }
                break;
            }
            case SpvOpVariable:
            case SpvOpUntypedVariableKHR:
            {
                if (wc >= 4u)
                {
                    const auto dataTypeId = (op == SpvOpUntypedVariableKHR && wc >= 5u) ? p[3] : 0u;
                    ctx.variables.push_back({ p[1], p[0], p[2], dataTypeId });

                    if (p[2] == SpvStorageClassUniformConstant || p[2] == SpvStorageClassUniform || p[2] == SpvStorageClassStorageBuffer)
                    {
                        ctx.globalHeapVarIds.insert(p[1]);
                    }
                }
                break;
            }
            case SpvOpConstant:
            case SpvOpSpecConstant:
            {
                if (wc >= 4u)
                {
                    ctx.constUints[p[1]] = p[2];
                    ctx.constTypes[p[1]] = p[0];

                    if (op == SpvOpConstant && wc == 4u && ctx.types[p[0]].op == SpvOpTypeInt)
                    {
                        ctx.intConstMap[MakeMemberKey(p[0], p[2])] = p[1];
                    }
                }
                break;
            }
            case SpvOpConstantNull:
            {
                if (wc >= 3u)
                {
                    ctx.constUints[p[1]] = 0u;
                    ctx.constTypes[p[1]] = p[0];
                }
                break;
            }
            case SpvOpName:
            {
                if (wc >= 3u)
                {
                    ctx.names[p[0]] = reinterpret_cast<const char*>(&p[1]);
                }
                break;
            }
            case SpvOpMemberName:
            {
                if (wc >= 4u)
                {
                    ctx.memberNames[MakeMemberKey(p[0], p[1])] = reinterpret_cast<const char*>(&p[2]);
                }
                break;
            }
            case SpvOpMemberDecorate:
            {
                if (wc >= 5u && p[2] == SpvDecorationOffset)
                {
                    ctx.memberOffsets[MakeMemberKey(p[0], p[1])] = { p[3], (uint32_t)(instructionIndex + 4u) };
                }
                break;
            }
            case SpvOpTypeInt:
            {
                if (wc >= 4u)
                {
                    ctx.types[p[0]] = { op, 0u, 0u, p[1], p[2] };
                }
                break;
            }
            case SpvOpTypeFloat:
            {
                if (wc >= 3u)
                {
                    ctx.types[p[0]] = { op, 0u, 0u, p[1], 0u };
                }
                break;
            }
            case SpvOpTypeVector:
            {
                if (wc >= 4u)
                {
                    ctx.types[p[0]] = { op, p[1], p[2], 0u, 0u };
                }
                break;
            }
            case SpvOpTypeStruct:
            {
                if (wc >= 2u)
                {
                    ctx.types[p[0]] = { op, wc >= 3u ? p[1] : 0u, (uint32_t)(wc - 2u), 0u, 0u };
                }
                break;
            }
            case SpvOpTypePointer:
            {
                if (wc >= 4u)
                {
                    ctx.types[p[0]] = { op, p[2], 0u, 0u, 0u };
                }
                break;
            }
            case SpvOpTypeUntypedPointerKHR:
            case SpvOpTypeBufferEXT:
            case SpvOpTypeImage:
            {
                ctx.types[p[0]] = { op };
                break;
            }
            default: break;
        }
    }

    static void InitializePushConstantTracking(SpirvParserContext& ctx)
    {
        for (const auto& var : ctx.variables)
        {
            if (var.storageClass == SpvStorageClassPushConstant)
            {
                ctx.pushVarId = var.id;
                ctx.pushStructTypeId = var.dataTypeId ? var.dataTypeId : ctx.types[var.ptrTypeId].elemTypeId;
                const auto& outerType = ctx.types[ctx.pushStructTypeId];

                if (outerType.op == SpvOpTypeStruct &&
                    outerType.elemCount == 1u &&
                    ctx.types[outerType.elemTypeId].op == SpvOpTypeStruct)
                {
                    ctx.outerPushStructTypeId = ctx.pushStructTypeId;
                    ctx.pushStructTypeId = outerType.elemTypeId;
                    ctx.isWrappedPushStruct = true;
                }

                ctx.innerPushPtrIds.insert(ctx.pushVarId);
                ctx.pushMemberCount = ctx.types[ctx.pushStructTypeId].elemCount;
                ctx.hasPushConstants = ctx.pushMemberCount > 0u;
                ctx.memberHandleIds.resize(ctx.pushMemberCount);
                ctx.memberResourceIds.resize(ctx.pushMemberCount);
                ctx.memberAccessed.assign(ctx.pushMemberCount, false);
                ctx.memberWritten.assign(ctx.pushMemberCount, false);
                break;
            }
        }
    }

    static void ProcessFunctionInstruction(SpvOp op, uint16_t wc, const uint32_t* p, PKShaderStage stage, SpirvParserContext& ctx)
    {
        if (stage == PKShaderStage::Vertex)
        {
            for (auto k = 0u; k + 1u < wc; ++k)
            {
                if (ctx.locations.count(p[k]))
                {
                    ctx.accessedInputVarIds.insert(p[k]);
                }
            }
        }

        if (!ctx.hasPushConstants)
        {
            return;
        }

        const auto isTypedAccess = (op == SpvOpAccessChain || op == SpvOpInBoundsAccessChain);
        const auto isUntypedAccess = (op == SpvOpUntypedAccessChainKHR || op == SpvOpUntypedInBoundsAccessChainKHR);

        if (isUntypedAccess && wc >= 6u)
        {
            const uint32_t baseType = p[2];
            const uint32_t basePtr  = p[3];
            const uint32_t idx0     = p[4];

            if (ctx.innerPushPtrIds.count(basePtr))
            {
                if (ctx.isWrappedPushStruct && baseType == ctx.outerPushStructTypeId)
                {
                    if (wc == 6u)
                    {
                        ctx.innerPushPtrIds.insert(p[1]);
                        return;
                    }
                    const uint32_t idx1 = p[5];
                    if (wc >= 7u && ctx.constUints.count(idx1) && ctx.constUints[idx1] < ctx.pushMemberCount)
                    {
                        const auto m = ctx.constUints[idx1];
                        ctx.memberAccessed[m] = true;
                        ctx.memberHandleIds[m].insert(p[1]);
                        return;
                    }
                }
                else if (baseType == ctx.pushStructTypeId)
                {
                    if (ctx.constUints.count(idx0) && ctx.constUints[idx0] < ctx.pushMemberCount)
                    {
                        const auto m = ctx.constUints[idx0];
                        ctx.memberAccessed[m] = true;
                        ctx.memberHandleIds[m].insert(p[1]);
                        return;
                    }
                }
                else
                {
                    ctx.memberAccessed[0] = true;
                    ctx.memberHandleIds[0].insert(p[1]);
                    return;
                }
            }
        }
        else if (isTypedAccess && wc >= 5u)
        {
            const uint32_t basePtr = p[2];
            const uint32_t idx0    = p[3];

            if (ctx.isWrappedPushStruct && basePtr == ctx.pushVarId)
            {
                if (wc == 5u)
                {
                    ctx.innerPushPtrIds.insert(p[1]);
                    return;
                }
                const uint32_t idx1 = p[4];
                if (wc >= 6u && ctx.constUints.count(idx1) && ctx.constUints[idx1] < ctx.pushMemberCount)
                {
                    const auto m = ctx.constUints[idx1];
                    ctx.memberAccessed[m] = true;
                    ctx.memberHandleIds[m].insert(p[1]);
                    return;
                }
            }
            else if (ctx.innerPushPtrIds.count(basePtr) && ctx.constUints.count(idx0) && ctx.constUints[idx0] < ctx.pushMemberCount)
            {
                const auto m = ctx.constUints[idx0];
                ctx.memberAccessed[m] = true;
                ctx.memberHandleIds[m].insert(p[1]);
                return;
            }
        }
        else if (op == SpvOpLoad && wc >= 4u && ctx.innerPushPtrIds.count(p[2]))
        {
            if (p[0] == ctx.pushStructTypeId || (ctx.isWrappedPushStruct && p[0] == ctx.outerPushStructTypeId))
            {
                ctx.loadedPushStructIds.insert(p[1]);
            }
            else
            {
                ctx.memberAccessed[0] = true;
                ctx.memberHandleIds[0].insert(p[1]);
            }
            return;
        }
        else if (op == SpvOpCompositeExtract && wc >= 5u && ctx.loadedPushStructIds.count(p[2]) && p[3] < ctx.pushMemberCount)
        {
            const auto m = p[3];
            ctx.memberAccessed[m] = true;
            ctx.memberHandleIds[m].insert(p[1]);
            return;
        }

        if (op == SpvOpCopyObject && wc >= 4u)
        {
            if (ctx.innerPushPtrIds.count(p[2]))
            {
                ctx.innerPushPtrIds.insert(p[1]);
            }
            if (ctx.globalHeapVarIds.count(p[2]))
            {
                ctx.globalHeapVarIds.insert(p[1]);
            }
        }

        for (auto m = 0u; m < ctx.pushMemberCount; ++m)
        {
            auto& handles = ctx.memberHandleIds[m];
            auto& resources = ctx.memberResourceIds[m];

            if (handles.empty() && resources.empty())
            {
                continue;
            }

            switch (op)
            {
                case SpvOpLoad:
                {
                    if (wc >= 4u)
                    {
                        const auto resOp = ctx.types[p[0]].op;

                        if (handles.count(p[2]))
                        {
                            handles.insert(p[1]);
                        }
                        else if (resources.count(p[2]) && (resOp == SpvOpTypeImage || resOp == SpvOpTypePointer || resOp == SpvOpTypeUntypedPointerKHR || resOp == SpvOpTypeBufferEXT))
                        {
                            resources.insert(p[1]);
                        }
                    }
                    break;
                }
                case SpvOpCopyObject:
                {
                    if (wc >= 4u)
                    {
                        if (handles.count(p[2]))
                        {
                            handles.insert(p[1]);
                        }
                        if (resources.count(p[2]))
                        {
                            resources.insert(p[1]);
                        }
                    }
                    break;
                }
                case SpvOpCompositeConstruct:
                case SpvOpCompositeInsert:
                case SpvOpVectorInsertDynamic:
                case SpvOpIAdd:
                case SpvOpISub:
                case SpvOpIMul:
                case SpvOpUDiv:
                case SpvOpSDiv:
                case SpvOpUMod:
                case SpvOpSRem:
                case SpvOpSMod:
                case SpvOpShiftRightLogical:
                case SpvOpShiftRightArithmetic:
                case SpvOpShiftLeftLogical:
                case SpvOpBitwiseOr:
                case SpvOpBitwiseXor:
                case SpvOpBitwiseAnd:
                case SpvOpBitFieldInsert:
                case SpvOpBitFieldSExtract:
                case SpvOpBitFieldUExtract:
                {
                    for (auto k = 2u; k + 1u < wc; ++k)
                    {
                        if (handles.count(p[k]))
                        {
                            handles.insert(p[1]);
                            break;
                        }
                    }
                    break;
                }
                case SpvOpAccessChain:
                case SpvOpInBoundsAccessChain:
                case SpvOpPtrAccessChain:
                case SpvOpInBoundsPtrAccessChain:
                case SpvOpUntypedAccessChainKHR:
                case SpvOpUntypedInBoundsAccessChainKHR:
                case SpvOpUntypedPtrAccessChainKHR:
                case SpvOpUntypedInBoundsPtrAccessChainKHR:
                case SpvOpImageTexelPointer:
                case SpvOpUntypedImageTexelPointerEXT:
                {
                    const bool isUntypedOp =
                        op == SpvOpUntypedAccessChainKHR ||
                        op == SpvOpUntypedInBoundsAccessChainKHR ||
                        op == SpvOpUntypedPtrAccessChainKHR ||
                        op == SpvOpUntypedInBoundsPtrAccessChainKHR ||
                        op == SpvOpUntypedImageTexelPointerEXT;

                    const auto baseIdx = isUntypedOp ? 3u : 2u;

                    if (wc > baseIdx)
                    {
                        if (resources.count(p[baseIdx]))
                        {
                            resources.insert(p[1]);
                        }
                        else if (handles.count(p[baseIdx]))
                        {
                            handles.insert(p[1]);
                        }
                        else if (ctx.globalHeapVarIds.count(p[baseIdx]))
                        {
                            for (auto k = baseIdx + 1u; k + 1u < wc; ++k)
                            {
                                if (handles.count(p[k]))
                                {
                                    resources.insert(p[1]);
                                    break;
                                }
                            }
                        }
                    }
                    break;
                }
                case SpvOpAtomicExchange:
                case SpvOpAtomicCompareExchange:
                case SpvOpAtomicCompareExchangeWeak:
                case SpvOpAtomicIIncrement:
                case SpvOpAtomicIDecrement:
                case SpvOpAtomicIAdd:
                case SpvOpAtomicISub:
                case SpvOpAtomicSMin:
                case SpvOpAtomicUMin:
                case SpvOpAtomicSMax:
                case SpvOpAtomicUMax:
                case SpvOpAtomicAnd:
                case SpvOpAtomicOr:
                case SpvOpAtomicXor:
                case SpvOpAtomicFMinEXT:
                case SpvOpAtomicFMaxEXT:
                case SpvOpAtomicFAddEXT:
                {
                    if (wc >= 4u && resources.count(p[2]))
                    {
                        ctx.memberWritten[m] = true;
                    }
                    break;
                }
                case SpvOpUConvert:
                case SpvOpSConvert:
                case SpvOpBitcast:
                case SpvOpCompositeExtract:
                {
                    if (wc >= 4u && handles.count(p[2]))
                    {
                        handles.insert(p[1]);
                    }
                    break;
                }
                case SpvOpStore:
                case SpvOpAtomicStore:
                case SpvOpImageWrite:
                {
                    if (wc >= 3u)
                    {
                        if (resources.count(p[0]))
                        {
                            ctx.memberWritten[m] = true;
                        }
                        else if (op == SpvOpStore && handles.count(p[1]))
                        {
                            handles.insert(p[0]);
                        }
                        else if (op == SpvOpStore && resources.count(p[1]))
                        {
                            resources.insert(p[0]);
                        }
                    }
                    break;
                }
                case SpvOpBufferPointerEXT:
                {
                    if (wc >= 4u && (handles.count(p[2]) || resources.count(p[2])))
                    {
                        resources.insert(p[1]);
                    }
                    break;
                }
                default: break;
            }
        }
    }

    bool ReflectSPIRVStage(const uint32_t* code, size_t wordCount, PKShaderStage stage, SpvStageReflection* outReflection)
    {
        if (!code || wordCount < 5u || !outReflection)
        {
            return false;
        }

        SpirvParserContext ctx{};
        auto inFunctionSection = false;

        for (auto i = 5ull; i < wordCount;)
        {
            const auto op = (SpvOp)(code[i] & 0xFFFFu);
            const auto wc = (uint16_t)((code[i] >> 16u) & 0xFFFFu);
            const auto* p = code + i + 1u;

            if (wc == 0u)
            {
                break;
            }

            if (op == SpvOpFunction && !inFunctionSection)
            {
                inFunctionSection = true;
                InitializePushConstantTracking(ctx);
            }

            if (!inFunctionSection)
            {
                ProcessHeaderInstruction(op, wc, p, i, ctx, *outReflection);
            }
            else
            {
                ProcessFunctionInstruction(op, wc, p, stage, ctx);
            }

            i += wc;
        }

        outReflection->spirv.assign(code, code + wordCount);

        uint32_t insertedWords = 0u;
        if (ctx.needsSampleRateCapability && !ctx.hasSampleRateCapability)
        {
            const uint32_t capWords[2] = { (2u << 16u) | uint32_t(SpvOpCapability), 35u };
            outReflection->spirv.insert(outReflection->spirv.begin() + 5, capWords, capWords + 2);
            insertedWords = 2u;
        }

        if (ctx.localSizeId[0] != 0u)
        {
            outReflection->groupSize[0] = (uint16_t)ctx.constUints[ctx.localSizeId[0]];
            outReflection->groupSize[1] = (uint16_t)ctx.constUints[ctx.localSizeId[1]];
            outReflection->groupSize[2] = (uint16_t)ctx.constUints[ctx.localSizeId[2]];
        }

        if (stage == PKShaderStage::Vertex)
        {
            for (const auto& var : ctx.variables)
            {
                if (var.storageClass == SpvStorageClassInput && ctx.accessedInputVarIds.count(var.id) && !ctx.builtIns.count(var.id))
                {
                    PKVertexInputAttribute attribute{};
                    const auto name = CleanVariableName(ctx.names[var.id]);
                    WriteName(attribute.name, name.c_str());
                    attribute.location = (uint16_t)ctx.locations[var.id];
                    attribute.type = ReflectVertexInputType(ctx.types[var.ptrTypeId].elemTypeId, ctx.types);

                    if (attribute.type != PKElementType::Invalid)
                    {
                        outReflection->vertexAttributes.push_back(attribute);
                    }
                }
            }

            std::sort(outReflection->vertexAttributes.begin(), outReflection->vertexAttributes.end(), [](const PKVertexInputAttribute& a, const PKVertexInputAttribute& b)
            {
                return a.location < b.location;
            });
        }

        for (auto m = 0u; m < ctx.pushMemberCount; ++m)
        {
            const auto key = MakeMemberKey(ctx.pushStructTypeId, m);
            const auto name = CleanVariableName(ctx.memberNames[key]);
            const auto& offset = ctx.memberOffsets[key];

            if (!name.empty())
            {
                const uint32_t wordOffset = offset.wordOffset ? (offset.wordOffset + insertedWords) : 0u;
                outReflection->pushConstantMembers.push_back({ name, wordOffset, (uint16_t)offset.offset, ctx.memberAccessed[m], ctx.memberWritten[m] });
            }
        }

        return true;
    }
}
