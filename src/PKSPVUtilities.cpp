#include "PKSPVUtilities.h"
#include <SPIRV-Reflect/spirv_reflect.h>

namespace PK::Assets::Shader
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

    static void GatherCommands(const uint32_t* code, const uint32_t wordCount)
    {
        uint32_t i = 5u; // Bytecode starts at word index 5, after the header.

        std::vector<OpStore> opStores;
        std::vector<OpImageWrite> opImageWrites;
        std::vector<OpLoad> opLoads;
        std::vector<OpAccessChain> opAccessChains;

        while(i < wordCount)
        {
            SpvOp opcode = (SpvOp)(code[i] & 0xFFFF);
            uint16_t wcount = (code[i] >> 16) & 0xFFFF;
            const uint32_t* params = code + i + 1;
        
            switch (opcode)
            {
                case SpvOp::SpvOpLoad:
                {
                    opLoads.push_back({ params[0], params[1], params[2] });
                }
                break;

                case SpvOp::SpvOpAccessChain:
                {
                    opAccessChains.push_back({ params[0], params[1], params[2] });
                }
                break;

                case SpvOp::SpvOpImageWrite:
                {
                    opImageWrites.push_back({ params[0] });
                }
                break;

                case SpvOp::SpvOpStore:
                {
                    opStores.push_back({ params[0], params[1] });
                }
                break;
            }

            i += wcount;
        }
    }

    static bool ReflectAccessChainStore(const uint32_t* code, const uint32_t wordCount, uint32_t accessChainId)
    {
        uint32_t i = 5u; // Bytecode starts at word index 5, after the header.

        while (i < wordCount)
        {
            SpvOp opcode = (SpvOp)(code[i] & 0xFFFFu);
            uint16_t wcount = (code[i] >> 16) & 0xFFFFu;
            const uint32_t* params = code + i + 1;

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
            }

            i += wcount;
        }

        return false;
    }

    static bool ReflectOpLoadImageWrite(const uint32_t* code, const uint32_t wordCount, uint32_t opLoadId)
    {
        uint32_t i = 5u; // Bytecode starts at word index 5, after the header.

        while (i < wordCount)
        {
            SpvOp opcode = (SpvOp)(code[i] & 0xFFFFu);
            uint16_t wcount = (code[i] >> 16) & 0xFFFFu;
            const uint32_t* params = code + i + 1;

            if (opcode == SpvOp::SpvOpImageWrite && 
                params[0] == opLoadId)
            {
                return true;
            }

            i += wcount;
        }

        return false;
    }

    bool ReflectBufferWrite(const uint32_t* code, const uint32_t wordCount, uint32_t variable)
    {
        uint32_t i = 5u; // Bytecode starts at word index 5, after the header.

        while (i < wordCount)
        {
            SpvOp opcode = (SpvOp)(code[i] & 0xFFFFu);
            uint16_t wcount = (code[i] >> 16) & 0xFFFFu;
            const uint32_t* params = code + i + 1;

            if (opcode == SpvOp::SpvOpAccessChain && 
                params[2] == variable && 
                ReflectAccessChainStore(code, wordCount, params[1]))
            {
                return true;
            }

            i += wcount;
        }

        return false;
    }

    bool ReflectImageWrite(const uint32_t* code, const uint32_t wordCount, uint32_t variable)
    {
        uint32_t i = 5u; // Bytecode starts at word index 5, after the header.

        while (i < wordCount)
        {
            SpvOp opcode = (SpvOp)(code[i] & 0xFFFFu);
            uint16_t wcount = (code[i] >> 16) & 0xFFFFu;
            const uint32_t* params = code + i + 1;

            if (opcode == SpvOp::SpvOpLoad &&
                params[2] == variable && 
                ReflectOpLoadImageWrite(code, wordCount, params[1]))
            {
                return true;
            }

            i += wcount;
        }

        return false;
    }

    bool ReflectResourceWrite(const uint32_t* code, const uint32_t wordCount, uint32_t variable, PKDescriptorType type)
    {
        switch (type)
        {
            case PKDescriptorType::Image: 
                return ReflectImageWrite(code, wordCount, variable);
            case PKDescriptorType::StorageBuffer:
            case PKDescriptorType::DynamicStorageBuffer: 
                return ReflectBufferWrite(code, wordCount, variable);
        }

        return false;
    }
}