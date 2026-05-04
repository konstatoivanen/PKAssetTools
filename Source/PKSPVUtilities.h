#pragma once
#include <PKAsset.h>

namespace PKAssets::Shader
{
    bool ReflectResourceWrite(const uint32_t* code, const uint32_t wordCount, uint32_t variable, PKDescriptorType type);
    bool ReflectLocalSize(const uint32_t* code, const uint32_t wordCount, uint32_t* outLocalSize);
}