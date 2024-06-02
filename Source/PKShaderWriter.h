#pragma once
namespace PKAssets::Shader
{
    constexpr static const char* PK_ASSET_SHADER_SRC_EXTENSION = ".shader";

    int WriteShader(const char* pathSrc, const char* pathDst);
}