#include <stdio.h>
#include <filesystem>
#include <PKAsset.h>
#include "PKShaderWriter.h"
#include "PKMeshWriter.h"
#include "PKFontWriter.h"
#include "PKTextureWriter.h"
#include "PKFileVersionUtilities.h"

using namespace PKAssets;

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

void ProcessFilesRecursive(const std::string& basedir, const std::filesystem::path& subdir, const std::string& dstdir)
{
    for (const auto& entry : std::filesystem::directory_iterator(subdir))
    {
        auto& entryPath = entry.path();

        if (!entryPath.has_extension())
        {
            ProcessFilesRecursive(basedir, entryPath, dstdir);
            continue;
        }

        auto extension = entryPath.extension();
        auto dstpath = std::filesystem::path(dstdir + std::filesystem::relative(entryPath, basedir).string());

        if (extension.compare(Shader::PK_ASSET_SHADER_SRC_EXTENSION) == 0)
        {
            auto dstpathstr = dstpath.replace_extension(PK_ASSET_EXTENSION_SHADER).string();
            auto srcpathstr = entryPath.string();
            auto writeStatus = Shader::WriteShader(srcpathstr.c_str(), dstpathstr.c_str());

            switch (writeStatus)
            {
                case -1: printf("Failed to asset: %s \n", dstpathstr.c_str()); break;
                case 1: printf("Asset was up to date: %s \n", dstpathstr.c_str()); break;
            }

            continue;
        }

        if (extension.compare(Mesh::PK_ASSET_MESH_SRC_EXTENSION) == 0)
        {
            auto dstpathstr = dstpath.replace_extension(PK_ASSET_EXTENSION_MESH).string();
            auto srcpathstr = entryPath.string();
            auto writeStatus = Mesh::WriteMesh(srcpathstr.c_str(), dstpathstr.c_str());

            switch (writeStatus)
            {
                case -1: printf("Failed to asset: %s \n", dstpathstr.c_str()); break;
                case 1: printf("Asset was up to date: %s \n", dstpathstr.c_str()); break;
            }

            continue;
        }

        if (extension.compare(Font::PK_ASSET_FONT_SRC_EXTENSION) == 0)
        {
            auto dstpathstr = dstpath.replace_extension(PK_ASSET_EXTENSION_FONT).string();
            auto srcpathstr = entryPath.string();
            auto writeStatus = Font::WriteFont(srcpathstr.c_str(), dstpathstr.c_str());

            switch (writeStatus)
            {
                case -1: printf("Failed to asset: %s \n", dstpathstr.c_str()); break;
                case 1: printf("Asset was up to date: %s \n", dstpathstr.c_str()); break;
            }

            continue;
        }

        if (extension.compare(Texture::PK_ASSET_TEXTURE_SRC_EXTENSION) == 0)
        {
            auto dstpathstr = dstpath.replace_extension(PK_ASSET_EXTENSION_TEXTURE).string();
            auto srcpathstr = entryPath.string();
            auto writeStatus = Texture::WriteTexture(srcpathstr.c_str(), dstpathstr.c_str());

            switch (writeStatus)
            {
                case -1: printf("Failed to asset: %s \n", dstpathstr.c_str()); break;
                case 1: printf("Asset was up to date: %s \n", dstpathstr.c_str()); break;
            }

            continue;
        }
    }
}

int main(int argc, char** argv)
{
    if (argc < 2 || argc > 3)
    {
        printf("Invalid number of arguments. current: %i \n", argc);

        for (auto i = 0; i < argc; ++i)
        {
            printf("%s \n", argv[i]);
        }

        return 0;
    }

    // Three arguments usually means that the working directory is included as the first argument.
    auto offs = argc == 3 ? 1 : 0;
    auto srcdir = std::filesystem::absolute(ProcessPath(argv[offs + 0])).string();
    auto dstdir = std::filesystem::absolute(ProcessPath(argv[offs + 1])).string();

    printf("Processing assets from: %s \n", srcdir.c_str());
    printf("to: %s \n", dstdir.c_str());

    if (!std::filesystem::exists(srcdir))
    {
        printf("Source directory not found: %s \n", srcdir.c_str());
        return 0;
    }

    if (!PKVersionUtilities::IsDirectoryOutOfDate(srcdir, dstdir))
    {
        printf("Assets are up to date.");
        return 0;
    }

    ProcessFilesRecursive(srcdir, srcdir, dstdir);
    return 0;
}