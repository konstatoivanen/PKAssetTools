#pragma once
#include "PKAsset.h"

namespace PK::Assets
{
    PKAsset OpenAsset(const char* filepath);
    void CloseAsset(PKAsset* asset);

    Shader::PKShader* ReadAsShader(PKAsset* asset);
    Mesh::PKMesh* ReadAsMesh(PKAsset* asset);
}