#pragma once
#include "PKAssets/PKAsset.h"

namespace PK::Assets::Shader
{
    bool ReflectBufferWrite(const uint32_t* code, const uint32_t wordCount, uint32_t variable);
    bool ReflectImageWrite(const uint32_t* code, const uint32_t wordCount, uint32_t variable);
    bool ReflectResourceWrite(const uint32_t* code, const uint32_t wordCount, uint32_t variable, PKDescriptorType type);
}