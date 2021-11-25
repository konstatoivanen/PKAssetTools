#include "PKShaderWriter.h"
#include "PKAssets/PKAsset.h"
#include <stdio.h>
#include <filesystem>

using namespace PK::Assets;

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        printf("Invalid number of arguments \n");
        return 0;
    }

    auto srcdir = std::string(argv[1]);
    auto dstdir = std::string(argv[2]);

    if (!std::filesystem::exists(srcdir))
    {
        printf("Source directory not found: %s \n", srcdir.c_str());
        return 0;
    }

    if (!std::filesystem::exists(dstdir))
    {
        printf("Destination directory not found: %s \n", dstdir.c_str());
        return 0;
    }

    if (dstdir[dstdir.size() - 1] != '/')
    {
        dstdir += '/';
    }

    for (const auto& entry : std::filesystem::directory_iterator(srcdir))
    {
        auto& path = entry.path();

        if (!path.has_extension())
        {
            continue;
        }

        auto extension = path.extension();
    
        if (extension.compare(Shader::PK_ASSET_SHADER_SRC_EXTENSION) == 0)
        {
            auto filename = path.filename().replace_extension(PK_ASSET_EXTENSION_SHADER);
            auto dstpath = dstdir + filename.string();
            auto srcpath = path.string();

            printf("Writing shader: %s \n", dstpath.c_str());

            if (Shader::WriteShader(srcpath.c_str(), dstpath.c_str()) != 0)
            {
                printf("Failed to write shader: %s \n", dstpath.c_str());
            }

            continue;
        }

        // Write other types
    }

    return 0;
}