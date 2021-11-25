#include "PKShaderWriter.h"
#include "PKAssets/PKAsset.h"
#include <stdio.h>
#include <filesystem>

using namespace PK::Assets;

static std::string ProcessPath(const std::string& path)
{
    if (path.size() < 2)
    {
        return path;
    }

    auto outpath = path;

    if (path[0] == '\'' && path[path.size() - 1] == '\'')
    {
        outpath = path.substr(1, path.size() - 2);
    }

    if (outpath[outpath.size() - 1] != '/' && outpath[outpath.size() - 1] != '\\')
    {
        outpath += '\\';
    }

    return outpath;
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        printf("Invalid number of arguments. current: %i \n", argc);
        
        for (auto i = 0; i < argc; ++i)
        {
            printf("%s \n", argv[i]);
        }

        return 0;
    }

    auto srcdir = ProcessPath(argv[1]);
    auto dstdir = ProcessPath(argv[2]);

    printf("Processing assets from: %s \n", srcdir.c_str());
    printf("to: %s \n", dstdir.c_str());

    if (!std::filesystem::exists(srcdir))
    {
        printf("Source directory not found: %s \n", srcdir.c_str());
        return 0;
    }

    for (const auto& entry : std::filesystem::directory_iterator(srcdir))
    {
        auto& srcpath = entry.path();

        if (!srcpath.has_extension())
        {
            continue;
        }

        auto extension = srcpath.extension();
        auto dstpath = std::filesystem::path(dstdir + std::filesystem::relative(srcpath, srcdir).string());

        if (extension.compare(Shader::PK_ASSET_SHADER_SRC_EXTENSION) == 0)
        {
            auto dstpathstr = dstpath.replace_extension(PK_ASSET_EXTENSION_SHADER).string();
            auto srcpathstr = srcpath.string();
            
            if (Shader::WriteShader(srcpathstr.c_str(), dstpathstr.c_str()) != 0)
            {
                printf("Failed to write shader: %s \n", dstpathstr.c_str());
            }

            continue;
        }

        // Write other types
    }

    return 0;
}