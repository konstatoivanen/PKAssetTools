#pragma once
#include <vector>
#include <string>
#include <PKAsset.h>

namespace PKAssets::Shader
{
    struct SpvPushConstantMember
    {
        std::string name;
        uint32_t wordOffset = 0u;
        uint16_t offset = 0u;
        bool isAccessed = false;
        bool isWritten = false;
    };

    struct SpvStageReflection
    {
        std::vector<uint32_t> spirv;
        std::vector<SpvPushConstantMember> pushConstantMembers;
        std::vector<PKVertexInputAttribute> vertexAttributes;
        uint16_t groupSize[3]{};
    };

    bool ReflectSPIRVStage(
        const uint32_t* code,
        size_t wordCount,
        PKShaderStage stage,
        SpvStageReflection* outReflection);
}