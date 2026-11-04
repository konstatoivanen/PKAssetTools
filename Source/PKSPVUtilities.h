#pragma once
#include <vector>
#include <unordered_set>
#include <PKAsset.h>

namespace PKAssets::Shader
{
    bool ReflectResourceWrite(const uint32_t* code, const uint32_t wordCount, uint32_t variable, PKUniform type);
    void ReflectPushConstantMemberAccess(
        const uint32_t* code,
        const uint32_t wordCount,
        uint32_t pushConstantVariable,
        uint32_t memberIndex,
        uint32_t bitOffset,
        uint32_t bitSize,
        PKUniform type,
        bool* outAccessed,
        bool* outWritten);
    void PatchPushConstantMemberOffsets(uint32_t* code, const uint32_t wordCount, uint32_t structTypeId, const std::vector<uint32_t>& memberOffsets);
    bool ReflectLocalSize(const uint32_t* code, const uint32_t wordCount, uint32_t* outLocalSize);
    std::vector<uint32_t> CleanSPIRV(const uint32_t* code, uint32_t wordCount, const std::unordered_set<uint32_t>& deadVariableIds, bool stripDebugInfo);
}