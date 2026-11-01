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

            if (opcode == SpvOp::SpvOpConstant && params[1] == constId)
            {
                *outValue = params[2];
                return true;
            }

            i += wcount;
        }

        return false;
    }

    bool ReflectResourceWrite(const uint32_t* code, const uint32_t wordCount, uint32_t variable, PKDescriptorType type)
    {
        if (type != PKDescriptorType::Image && type != PKDescriptorType::StorageBuffer)
        {
            return false;
        }

        std::vector<uint32_t> derivedIds;
        derivedIds.reserve(16u);
        derivedIds.push_back(variable);

        auto isDerived = [&](uint32_t id)
        {
            for (auto d : derivedIds)
            {
                if (d == id)
                {
                    return true;
                }
            }
            return false;
        };

        auto addDerived = [&](uint32_t id)
        {
            if (!isDerived(id))
            {
                derivedIds.push_back(id);
            }
        };

        auto i = 5u;

        while (i < wordCount)
        {
            const auto opcode = (SpvOp)(code[i] & 0xFFFFu);
            const auto wcount = (uint16_t)((code[i] >> 16) & 0xFFFFu);
            const auto params = code + i + 1u;

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
                case SpvOp::SpvOpCopyObject:
                case SpvOp::SpvOpLoad:
                case SpvOp::SpvOpImageTexelPointer:
                {
                    if (wcount >= 4u && isDerived(params[2]))
                    {
                        addDerived(params[1]);
                    }
                }
                break;

                case SpvOp::SpvOpStore:
                case SpvOp::SpvOpAtomicStore:
                case SpvOp::SpvOpImageWrite:
                {
                    if (wcount >= 3u && isDerived(params[0]))
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
                    if (wcount >= 4u && isDerived(params[2]))
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
                const bool foundX = FindOpConstUint(code, wordCount, params[2], &localSize[0]);
                const bool foundY = FindOpConstUint(code, wordCount, params[3], &localSize[1]);
                const bool foundZ = FindOpConstUint(code, wordCount, params[4], &localSize[2]);
                return foundX && foundY && foundZ;
            }

            i += wcount;
        }

        return false;
    }

    std::vector<uint32_t> CleanSPIRV(const uint32_t* code, uint32_t wordCount, const std::vector<uint32_t>& deadVariableIds, bool stripDebugInfo)
    {
        std::vector<uint32_t> out;
        out.reserve(wordCount + 2u);

        // Copy 5-word SPIR-V header
        for (auto i = 0u; i < 5u && i < wordCount; ++i)
        {
            out.push_back(code[i]);
        }

        // Workaround for Slang omitting OpCapability SampleRateShading when emitting SpvDecorationSample
        auto hasSampleRateCapability = false;
        auto usesSampleDecoration = false;

        for (auto i = 5u; i < wordCount;)
        {
            const auto opcode = (SpvOp)(code[i] & 0xFFFFu);
            const auto wcount = (uint16_t)((code[i] >> 16) & 0xFFFFu);
            const auto* params = code + i + 1u;
            
            if (wcount == 0u)
            {
                break;
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

            i += wcount;
        }

        if (usesSampleDecoration && !hasSampleRateCapability)
        {
            out.push_back((2u << 16u) | (uint32_t)SpvOpCapability);
            out.push_back((uint32_t)SpvCapabilitySampleRateShading);
        }

        auto isDeadId = [&](uint32_t id)
        {
            for (auto deadId : deadVariableIds)
            {
                if (deadId == id)
                {
                    return true;
                }
            }
            return false;
        };

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
                    i += wcount;
                    continue;
                }
            }

            if (!deadVariableIds.empty())
            {
                if ((opcode == SpvOpDecorate || opcode == SpvOpMemberDecorate || opcode == SpvOpName) && wcount >= 2u && isDeadId(params[0]))
                {
                    i += wcount;
                    continue;
                }

                if (opcode == SpvOpVariable && wcount >= 4u && isDeadId(params[1]))
                {
                    i += wcount;
                    continue;
                }

                if (opcode == SpvOpEntryPoint)
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

                        if (!isDeadId(interfaceId))
                        {
                            out.push_back(interfaceId);
                            newWCount++;
                        }
                    }

                    out[headerIdx] = ((uint32_t)newWCount << 16u) | (uint32_t)opcode;
                    i += wcount;
                    continue;
                }
            }

            out.insert(out.end(), code + i, code + i + wcount);
            i += wcount;
        }

        return out;
    }
}
