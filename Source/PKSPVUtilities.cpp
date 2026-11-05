#include <unordered_set>
#include <SPIRV-Reflect/spirv_reflect.h>
#include "PKSPVUtilities.h"

namespace PKAssets::Shader
{
    static bool FindOpConstUint(const uint32_t* code, const uint32_t wordCount, uint32_t constId, uint32_t* outValue)
    {
        auto i = 5u;

        while (i < wordCount)
        {
            auto opcode = (SpvOp)(code[i] & 0xFFFFu);
            auto wcount = (uint16_t)((code[i] >> 16) & 0xFFFFu);
            const auto* params = code + i + 1u;

            if (opcode == SpvOp::SpvOpConstant && wcount >= 4u && params[1] == constId)
            {
                *outValue = params[2];
                return true;
            }

            if (opcode == SpvOp::SpvOpConstantNull && wcount >= 3u && params[1] == constId)
            {
                *outValue = 0u;
                return true;
            }

            i += wcount;
        }

        return false;
    }

    static bool IsRemovablePureInstruction(SpvOp opcode, uint16_t wcount, const uint32_t* params)
    {
        if (wcount < 4u)
        {
            return false;
        }

        switch (opcode)
        {
            case SpvOpLoad:
                return wcount == 4u || (params[3] & SpvMemoryAccessVolatileMask) == 0u;
            case SpvOpAccessChain:
            case SpvOpInBoundsAccessChain:
            case SpvOpPtrAccessChain:
            case SpvOpInBoundsPtrAccessChain:
            case SpvOpUntypedAccessChainKHR:
            case SpvOpUntypedInBoundsAccessChainKHR:
            case SpvOpUntypedPtrAccessChainKHR:
            case SpvOpUntypedInBoundsPtrAccessChainKHR:
            case SpvOpBufferPointerEXT:
            case SpvOpBitFieldUExtract:
            case SpvOpBitFieldSExtract:
            case SpvOpUConvert:
            case SpvOpSConvert:
            case SpvOpFConvert:
            case SpvOpBitcast:
            case SpvOpCopyObject:
            case SpvOpCopyLogical:
            case SpvOpCompositeExtract:
            case SpvOpCompositeConstruct:
            case SpvOpVectorShuffle:
            case SpvOpConvertUToAccelerationStructureKHR:
                return true;
            default:
                return false;
        }
    }

    static void MarkInstructionOperandsUsed(SpvOp opcode, uint16_t wcount, const uint32_t* params, uint32_t idBound, std::vector<uint8_t>& usedIds)
    {
        const auto paramCount = (uint32_t)(wcount - 1u);
        auto operandEnd = paramCount;

        // Exclude trailing literal immediates so that small literal numbers don't collide with low <id>s
        switch (opcode)
        {
            case SpvOpCompositeExtract:  operandEnd = std::min(paramCount, 3u); break;
            case SpvOpCompositeInsert:
            case SpvOpVectorShuffle:     operandEnd = std::min(paramCount, 4u); break;
            case SpvOpLoad:              operandEnd = std::min(paramCount, 3u); break;
            case SpvOpStore:             operandEnd = std::min(paramCount, 2u); break;
            case SpvOpSelectionMerge:    operandEnd = std::min(paramCount, 1u); break;
            case SpvOpLoopMerge:         operandEnd = std::min(paramCount, 2u); break;
            case SpvOpBranchConditional: operandEnd = std::min(paramCount, 3u); break;
            default: break;
        }

        for (auto p = 0u; p < operandEnd; ++p)
        {
            if (opcode == SpvOpExtInst && p == 3u)
            {
                continue;
            }

            const auto id = params[p];

            if (id < idBound)
            {
                usedIds[id] = 1u;
            }
        }
    }


    bool ReflectResourceWrite(const uint32_t* code, const uint32_t wordCount, uint32_t variable, PKUniform type)
    {
        if (type != PKUniform::Image && type != PKUniform::StorageBuffer)
        {
            return false;
        }

        std::unordered_set<uint32_t> derivedIds;
        derivedIds.insert(variable);

        auto i = 5u;

        while (i < wordCount)
        {
            const auto opcode = (SpvOp)(code[i] & 0xFFFFu);
            const auto wcount = (uint16_t)((code[i] >> 16) & 0xFFFFu);
            const auto* params = code + i + 1u;

            if (wcount == 0u)
            {
                break;
            }

            // We have a valid access chain from resource to a load op. look for write op.
            // Buffer access always references resoruces through access chains so we can ignore the op load.
            switch (opcode)
            {
                case SpvOp::SpvOpAccessChain:
                case SpvOp::SpvOpInBoundsAccessChain:
                case SpvOp::SpvOpPtrAccessChain:
                case SpvOp::SpvOpInBoundsPtrAccessChain:
                case SpvOp::SpvOpBufferPointerEXT:
                case SpvOp::SpvOpCopyObject:
                case SpvOp::SpvOpCopyLogical:
                case SpvOp::SpvOpLoad:
                case SpvOp::SpvOpImageTexelPointer:
                {
                    if (wcount >= 4u && derivedIds.count(params[2]))
                    {
                        derivedIds.insert(params[1]);
                    }
                }
                break;

                case SpvOp::SpvOpUntypedAccessChainKHR:
                case SpvOp::SpvOpUntypedInBoundsAccessChainKHR:
                case SpvOp::SpvOpUntypedPtrAccessChainKHR:
                case SpvOp::SpvOpUntypedInBoundsPtrAccessChainKHR:
                {
                    if (wcount >= 5u && derivedIds.count(params[3]))
                    {
                        derivedIds.insert(params[1]);
                    }
                }
                break;

                case SpvOp::SpvOpStore:
                case SpvOp::SpvOpAtomicStore:
                case SpvOp::SpvOpImageWrite:
                {
                    if (wcount >= 3u && derivedIds.count(params[0]))
                    {
                        return true;
                    }
                }
                break;

                case SpvOp::SpvOpAtomicExchange:
                case SpvOp::SpvOpAtomicCompareExchange:
                case SpvOp::SpvOpAtomicCompareExchangeWeak:
                case SpvOp::SpvOpAtomicIIncrement:
                case SpvOp::SpvOpAtomicIDecrement:
                case SpvOp::SpvOpAtomicIAdd:
                case SpvOp::SpvOpAtomicISub:
                case SpvOp::SpvOpAtomicSMin:
                case SpvOp::SpvOpAtomicUMin:
                case SpvOp::SpvOpAtomicSMax:
                case SpvOp::SpvOpAtomicUMax:
                case SpvOp::SpvOpAtomicAnd:
                case SpvOp::SpvOpAtomicOr:
                case SpvOp::SpvOpAtomicXor:
                case SpvOp::SpvOpAtomicFAddEXT:
                {
                    if (wcount >= 4u && derivedIds.count(params[2]))
                    {
                        return true;
                    }
                }
                break;

                default: break;
            }

            i += wcount;
        }

        return false;
    }

    void ReflectPushConstantMemberAccess(
        const uint32_t* code,
        const uint32_t wordCount,
        uint32_t pushConstantVariable,
        uint32_t memberIndex,
        uint32_t bitOffset,
        uint32_t bitSize,
        PKUniform type,
        bool* outAccessed,
        bool* outWritten)
    {
        *outAccessed = false;
        *outWritten = false;

        const auto isHandle = PKUniformIsResourceHandle(type);
        const auto canWrite = type == PKUniform::Image || type == PKUniform::StorageBuffer;
        std::unordered_set<uint32_t> wordIds;
        std::unordered_set<uint32_t> indexIds;
        std::unordered_set<uint32_t> pointerIds;

        auto i = 5u;

        while (i < wordCount)
        {
            const auto opcode = (SpvOp)(code[i] & 0xFFFFu);
            const auto wcount = (uint16_t)((code[i] >> 16) & 0xFFFFu);
            const auto* params = code + i + 1u;

            if (wcount == 0u)
            {
                break;
            }

            switch (opcode)
            {
                case SpvOp::SpvOpAccessChain:
                case SpvOp::SpvOpInBoundsAccessChain:
                {
                    if (wcount >= 5u && params[2] == pushConstantVariable)
                    {
                        auto constVal = 0xFFFFFFFFu;
                        if (FindOpConstUint(code, wordCount, params[3], &constVal) && constVal == memberIndex)
                        {
                            if (bitSize == 0u)
                            {
                                if (!isHandle)
                                {
                                    *outAccessed = true;
                                    return;
                                }

                                indexIds.insert(params[1]);
                            }
                            else
                            {
                                wordIds.insert(params[1]);
                            }
                        }
                    }
                    else if (wcount >= 4u)
                    {
                        if (indexIds.count(params[2]))
                        {
                            indexIds.insert(params[1]);
                        }

                        if (pointerIds.count(params[2]) || (wcount >= 5u && indexIds.count(params[3])))
                        {
                            pointerIds.insert(params[1]);
                        }
                    }
                }
                break;

                case SpvOp::SpvOpBitFieldUExtract:
                case SpvOp::SpvOpBitFieldSExtract:
                {
                    if (bitSize > 0u && wcount >= 6u && wordIds.count(params[2]))
                    {
                        auto extractedOffset = 0xFFFFFFFFu;
                        auto extractedSize = 0xFFFFFFFFu;
                        if (FindOpConstUint(code, wordCount, params[3], &extractedOffset) &&
                            FindOpConstUint(code, wordCount, params[4], &extractedSize) &&
                            extractedOffset == bitOffset &&
                            extractedSize == bitSize)
                        {
                            if (!isHandle)
                            {
                                *outAccessed = true;
                                return;
                            }

                            indexIds.insert(params[1]);
                        }
                    }
                    else if (wcount >= 6u && indexIds.count(params[2]))
                    {
                        indexIds.insert(params[1]);
                    }
                }
                break;

                case SpvOp::SpvOpBufferPointerEXT:
                case SpvOp::SpvOpPtrAccessChain:
                case SpvOp::SpvOpInBoundsPtrAccessChain:
                {
                    if (wcount >= 4u && pointerIds.count(params[2]))
                    {
                        pointerIds.insert(params[1]);
                    }
                }
                break;

                case SpvOp::SpvOpUntypedAccessChainKHR:
                case SpvOp::SpvOpUntypedInBoundsAccessChainKHR:
                case SpvOp::SpvOpUntypedPtrAccessChainKHR:
                case SpvOp::SpvOpUntypedInBoundsPtrAccessChainKHR:
                {
                    if (wcount >= 5u && pointerIds.count(params[3]))
                    {
                        pointerIds.insert(params[1]);
                    }
                    else if (wcount >= 6u && indexIds.count(params[4]))
                    {
                        pointerIds.insert(params[1]);
                    }
                }
                break;

                case SpvOp::SpvOpConvertUToAccelerationStructureKHR:
                {
                    if (wcount >= 4u && indexIds.count(params[2]))
                    {
                        *outAccessed = true;
                        return;
                    }
                }
                break;

                case SpvOp::SpvOpLoad:
                {
                    if (wcount >= 4u)
                    {
                        if (wordIds.count(params[2]))
                        {
                            wordIds.insert(params[1]);
                        }

                        if (indexIds.count(params[2]))
                        {
                            indexIds.insert(params[1]);
                        }

                        if (pointerIds.count(params[2]))
                        {
                            *outAccessed = true;

                            if (!canWrite)
                            {
                                return;
                            }

                            pointerIds.insert(params[1]);
                        }
                    }
                }
                break;

                case SpvOp::SpvOpCopyObject:
                case SpvOp::SpvOpCopyLogical:
                case SpvOp::SpvOpUConvert:
                case SpvOp::SpvOpSConvert:
                case SpvOp::SpvOpBitcast:
                case SpvOp::SpvOpConvertUToPtr:
                case SpvOp::SpvOpConvertPtrToU:
                case SpvOp::SpvOpCompositeExtract:
                {
                    if (wcount >= 4u)
                    {
                        if (wordIds.count(params[2]))
                        {
                            wordIds.insert(params[1]);
                        }

                        if (indexIds.count(params[2]))
                        {
                            indexIds.insert(params[1]);
                        }

                        if (pointerIds.count(params[2]))
                        {
                            pointerIds.insert(params[1]);
                        }
                    }
                }
                break;

                case SpvOp::SpvOpCompositeConstruct:
                case SpvOp::SpvOpCompositeInsert:
                case SpvOp::SpvOpSelect:
                case SpvOp::SpvOpPhi:
                case SpvOp::SpvOpIMul:
                case SpvOp::SpvOpIAdd:
                case SpvOp::SpvOpShiftLeftLogical:
                case SpvOp::SpvOpShiftRightLogical:
                case SpvOp::SpvOpBitwiseAnd:
                case SpvOp::SpvOpBitwiseOr:
                {
                    if (wcount >= 4u)
                    {
                        for (uint16_t opIdx = 2u; opIdx + 1u < wcount; ++opIdx)
                        {
                            if (indexIds.count(params[opIdx]))
                            {
                                indexIds.insert(params[1]);
                            }

                            if (pointerIds.count(params[opIdx]))
                            {
                                pointerIds.insert(params[1]);
                            }
                        }
                    }
                }
                break;

                case SpvOp::SpvOpImageTexelPointer:
                {
                    if (wcount >= 4u && pointerIds.count(params[2]))
                    {
                        *outAccessed = true;
                        pointerIds.insert(params[1]);
                    }
                }
                break;

                case SpvOp::SpvOpArrayLength:
                {
                    if (wcount >= 5u && pointerIds.count(params[2]))
                    {
                        *outAccessed = true;

                        if (!canWrite)
                        {
                            return;
                        }
                    }
                }
                break;

                case SpvOp::SpvOpStore:
                case SpvOp::SpvOpAtomicStore:
                case SpvOp::SpvOpImageWrite:
                {
                    if (wcount >= 3u && pointerIds.count(params[0]))
                    {
                        *outAccessed = true;
                        *outWritten = true;
                        return;
                    }
                }
                break;

                case SpvOp::SpvOpAtomicExchange:
                case SpvOp::SpvOpAtomicCompareExchange:
                case SpvOp::SpvOpAtomicCompareExchangeWeak:
                case SpvOp::SpvOpAtomicIIncrement:
                case SpvOp::SpvOpAtomicIDecrement:
                case SpvOp::SpvOpAtomicIAdd:
                case SpvOp::SpvOpAtomicISub:
                case SpvOp::SpvOpAtomicSMin:
                case SpvOp::SpvOpAtomicUMin:
                case SpvOp::SpvOpAtomicSMax:
                case SpvOp::SpvOpAtomicUMax:
                case SpvOp::SpvOpAtomicAnd:
                case SpvOp::SpvOpAtomicOr:
                case SpvOp::SpvOpAtomicXor:
                case SpvOp::SpvOpAtomicFAddEXT:
                {
                    if (wcount >= 4u && pointerIds.count(params[2]))
                    {
                        *outAccessed = true;
                        *outWritten = true;
                        return;
                    }
                }
                break;

                default: break;
            }

            i += wcount;
        }
    }

    void PatchPushConstantMemberOffsets(uint32_t* code, const uint32_t wordCount, uint32_t structTypeId, const std::vector<uint32_t>& memberOffsets)
    {
        if (structTypeId == 0u || memberOffsets.empty())
        {
            return;
        }

        auto i = 5u;

        while (i < wordCount)
        {
            const auto opcode = (SpvOp)(code[i] & 0xFFFFu);
            const auto wcount = (uint16_t)((code[i] >> 16) & 0xFFFFu);
            auto* params = code + i + 1u;

            if (wcount == 0u)
            {
                break;
            }

            if (opcode == SpvOpMemberDecorate &&
                wcount >= 5u &&
                params[0] == structTypeId &&
                params[2] == SpvDecorationOffset &&
                params[1] < memberOffsets.size())
            {
                params[3] = memberOffsets[params[1]];
            }

            i += wcount;
        }
    }


    bool ReflectLocalSize(const uint32_t* code, const uint32_t wordCount, uint32_t* localSize)
    {
        auto i = 5u;

        while (i < wordCount)
        {
            const auto opcode = (SpvOp)(code[i] & 0xFFFFu);
            const auto wcount = (uint16_t)((code[i] >> 16) & 0xFFFFu);
            const auto* params = code + i + 1u;

            if (opcode == SpvOp::SpvOpExecutionModeId && params[1] == SpvExecutionMode::SpvExecutionModeLocalSizeId)
            {
                const auto foundX = FindOpConstUint(code, wordCount, params[2], &localSize[0]);
                const auto foundY = FindOpConstUint(code, wordCount, params[3], &localSize[1]);
                const auto foundZ = FindOpConstUint(code, wordCount, params[4], &localSize[2]);
                return foundX && foundY && foundZ;
            }

            i += wcount;
        }

        return false;
    }

    std::vector<uint32_t> CleanSPIRV(const uint32_t* code, uint32_t wordCount, const std::unordered_set<uint32_t>& deadVariableIds, bool stripDebugInfo)
    {
        if (wordCount < 5u)
        {
            return {};
        }

        const auto idBound = code[3];
        std::vector<uint32_t> instOffsets;
        instOffsets.reserve(wordCount / 4u);

        // Workaround for Slang omitting OpCapability SampleRateShading when emitting SpvDecorationSample
        auto hasSampleRateCapability = false;
        auto usesSampleDecoration = false;
        auto firstFunctionInstIdx = 0xFFFFFFFFu;

        for (auto i = 5u; i < wordCount;)
        {
            const auto opcode = (SpvOp)(code[i] & 0xFFFFu);
            const auto wcount = (uint16_t)((code[i] >> 16) & 0xFFFFu);
            const auto* params = code + i + 1u;

            if (wcount == 0u)
            {
                break;
            }

            if (opcode == SpvOpFunction && firstFunctionInstIdx == 0xFFFFFFFFu)
            {
                firstFunctionInstIdx = (uint32_t)instOffsets.size();
            }

            if (opcode == SpvOpCapability && wcount >= 2u && params[0] == SpvCapabilitySampleRateShading)
            {
                hasSampleRateCapability = true;
            }
            else if (
                (opcode == SpvOpDecorate && wcount >= 3u && params[1] == SpvDecorationSample) ||
                (opcode == SpvOpMemberDecorate && wcount >= 4u && params[2] == SpvDecorationSample))
            {
                usesSampleDecoration = true;
            }

            instOffsets.push_back(i);
            i += wcount;
        }

        std::vector<uint8_t> usedIds(idBound, 0u);
        std::vector<uint8_t> deadIds(idBound, 0u);
        std::vector<uint8_t> deadInsts(instOffsets.size(), 0u);

        for (const auto deadVarId : deadVariableIds)
        {
            if (deadVarId < idBound)
            {
                deadIds[deadVarId] = 1u;
            }
        }

        // Backwards SSA dead-instruction sweep across function bodies
        if (firstFunctionInstIdx != 0xFFFFFFFFu)
        {
            for (auto idx = (int32_t)instOffsets.size() - 1; idx >= (int32_t)firstFunctionInstIdx; --idx)
            {
                const auto offset = instOffsets[idx];
                const auto opcode = (SpvOp)(code[offset] & 0xFFFFu);
                const auto wcount = (uint16_t)((code[offset] >> 16) & 0xFFFFu);
                const auto* params = code + offset + 1u;

                if (IsRemovablePureInstruction(opcode, wcount, params))
                {
                    const auto resultId = params[1];

                    if (resultId < idBound && !usedIds[resultId])
                    {
                        deadInsts[idx] = 1u;
                        deadIds[resultId] = 1u;
                        continue;
                    }
                }

                MarkInstructionOperandsUsed(opcode, wcount, params, idBound, usedIds);
            }
        }

        std::vector<uint32_t> out;
        out.reserve(wordCount + 2u);

        // Copy 5-word SPIR-V header
        for (auto i = 0u; i < 5u; ++i)
        {
            out.push_back(code[i]);
        }

        if (usesSampleDecoration && !hasSampleRateCapability)
        {
            out.push_back((2u << 16u) | (uint32_t)SpvOpCapability);
            out.push_back((uint32_t)SpvCapabilitySampleRateShading);
        }

        for (auto idx = 0u; idx < instOffsets.size(); ++idx)
        {
            if (deadInsts[idx])
            {
                continue;
            }

            const auto i = instOffsets[idx];
            const auto opcode = (SpvOp)(code[i] & 0xFFFFu);
            const auto wcount = (uint16_t)((code[i] >> 16) & 0xFFFFu);
            const auto* params = code + i + 1u;

            if (stripDebugInfo)
            {
                if (opcode == SpvOpSourceContinued ||
                    opcode == SpvOpSource ||
                    opcode == SpvOpSourceExtension ||
                    opcode == SpvOpName ||
                    opcode == SpvOpMemberName ||
                    opcode == SpvOpLine ||
                    opcode == SpvOpNoLine ||
                    opcode == SpvOpModuleProcessed)
                {
                    continue;
                }
            }

            if ((opcode == SpvOpDecorate || opcode == SpvOpMemberDecorate || opcode == SpvOpName) &&
                wcount >= 2u &&
                params[0] < idBound &&
                deadIds[params[0]])
            {
                continue;
            }

            if (opcode == SpvOpVariable && wcount >= 4u && params[1] < idBound && deadIds[params[1]])
            {
                continue;
            }

            if (opcode == SpvOpEntryPoint && !deadVariableIds.empty())
            {
                const auto headerIdx = out.size();
                out.push_back(code[i]);
                out.push_back(params[0]);
                out.push_back(params[1]);

                uint16_t w = 3u;

                while (w < wcount)
                {
                    const auto word = code[i + w];
                    out.push_back(word);
                    w++;

                    if ((word & 0xFFu) == 0u ||
                        (word & 0xFF00u) == 0u ||
                        (word & 0xFF0000u) == 0u ||
                        (word & 0xFF000000u) == 0u)
                    {
                        break;
                    }
                }

                auto newWCount = w;

                for (; w < wcount; ++w)
                {
                    const auto interfaceId = code[i + w];

                    if (interfaceId >= idBound || !deadIds[interfaceId])
                    {
                        out.push_back(interfaceId);
                        newWCount++;
                    }
                }

                out[headerIdx] = ((uint32_t)newWCount << 16u) | (uint32_t)opcode;
                continue;
            }

            out.insert(out.end(), code + i, code + i + wcount);
        }

        return out;
    }
}
