#pragma once
namespace PK::Assets::Mesh
{
    constexpr static const char* PK_ASSET_MESH_SRC_EXTENSION = ".mdl";

    int WriteMesh(const char* pathSrc, const char* pathDst);
}