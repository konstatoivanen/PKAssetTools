#include <SPIRV-Reflect/spirv_reflect.h>
#include "PKSPVUtilities.h"

namespace PKAssets::Shader
{
    // Debug types
    struct OpStore
    {
        uint32_t pointer;
        uint32_t object;
    };

    struct OpImageWrite
    {
        uint32_t image;
    };

    struct OpLoad
    {
        uint32_t resultType;
        uint32_t result;
        uint32_t pointer;
    };

    struct OpAccessChain
    {
        uint32_t resultType;
        uint32_t result;
        uint32_t base;
    };

    static bool FindOpConstUint(const uint32_t* code, const uint32_t wordCount, uint32_t constId, uint32_t* outValue)
    {
        uint32_t i = 5u; // Bytecode starts at word index 5, after the header.

        while (i < wordCount)
        {
            SpvOp opcode = (SpvOp)(code[i] & 0xFFFFu);
            uint16_t wcount = (code[i] >> 16) & 0xFFFFu;
            const uint32_t* params = code + i + 1;

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
        if (type != PKDescriptorType::Image && 
            type != PKDescriptorType::StorageBuffer && 
            type != PKDescriptorType::DynamicStorageBuffer)
        {
            return false;
        }

        uint32_t i = 5u; // Bytecode starts at word index 5, after the header.

        uint32_t accessChainId = ~0u;
        uint32_t opLoadId = ~0u;

        while (i < wordCount)
        {
            SpvOp opcode = (SpvOp)(code[i] & 0xFFFFu);
            uint16_t wcount = (code[i] >> 16) & 0xFFFFu;
            const uint32_t* params = code + i + 1u;

            // Reset access chain if the variable was reused.
            if (opcode == SpvOp::SpvOpAccessChain && params[1] == accessChainId)
            {
                accessChainId = ~0u;
            }

            // Reset opload if the variable was reused.
            if (opcode == SpvOp::SpvOpLoad && params[1] == opLoadId)
            {
                opLoadId = ~0u;
            }

            // found access chain. potentially aliased resource
            if (opcode == SpvOp::SpvOpAccessChain && params[2] == variable)
            {
                accessChainId = params[1];
            }

            if (opcode == SpvOp::SpvOpLoad && (params[2] == variable || params[2] == accessChainId))
            {
                opLoadId = params[1];
            }

            // We have a valid access chain from resource to a load op. look for write op.
            // Buffer access always references resoruces through access chains so we can ignore the op load.
            switch (opcode)
            {
                case SpvOp::SpvOpStore:
                case SpvOp::SpvOpAtomicStore:
                {
                    if (params[0] == accessChainId)
                    {
                        return true;
                    }
                }
                break;

                case SpvOp::SpvOpImageWrite:
                {
                    if (params[0] == opLoadId)
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
                    if (params[2] == accessChainId)
                    {
                        return true;
                    }
                }
                default: break;
            }

            i += wcount;
        }

        return false;
    }

    bool ReflectLocalSize(const uint32_t* code, const uint32_t wordCount, uint32_t* localSize)
    {
        uint32_t i = 5u; // Bytecode starts at word index 5, after the header.

        while (i < wordCount)
        {
            SpvOp opcode = (SpvOp)(code[i] & 0xFFFFu);
            uint16_t wcount = (code[i] >> 16) & 0xFFFFu;
            const uint32_t* params = code + i + 1;
            
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
}