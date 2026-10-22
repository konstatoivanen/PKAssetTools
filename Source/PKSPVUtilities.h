#pragma once
#include <PKAsset.h>

namespace PKAssets::Shader
{
    bool ReflectResourceWrite(const uint32_t* code, const uint32_t wordCount, uint32_t variable, PKDescriptorType type);
    bool ReflectLocalSize(const uint32_t* code, const uint32_t wordCount, uint32_t* outLocalSize);
    std::vector<uint32_t> CleanSPIRV(const uint32_t* code, uint32_t wordCount, const std::vector<uint32_t>& deadVariableIds, bool stripDebugInfo);
}