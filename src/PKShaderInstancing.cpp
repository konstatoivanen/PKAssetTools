#include "PKShaderInstancing.h"
#include "PKStringUtilities.h"
#include "PKShaderUtilities.h"
#include "PKAssetWriter.h"

namespace PK::Assets::Shader::Instancing
{
    using namespace PK::Assets::StringUtilities;

    void InsertEntryPoint(std::string& source, PKShaderStage stage, bool enableInstancing)
    {
        if (!enableInstancing)
        {
            return;
        }

        switch (stage)
        {
            //@TODO Mesh shading support
            case PKShaderStage::Vertex: source.insert(0, Instancing_Vertex_GLSL); break;
            case PKShaderStage::MeshAssembly: source.insert(0, Instancing_MeshAssembly_GLSL); break;
            case PKShaderStage::Fragment: source.insert(0, Instancing_Fragment_GLSL); break;
            default: return;
        }

        auto pos = source.find("main()");

        // Source might contain multiple mains
        while (pos != std::string::npos)
        {
            pos = source.find('{', pos);
            auto eol = source.find_first_of("\r\n", pos);
            auto nextLinePos = source.find_first_not_of("\r\n", eol);
            source.insert(nextLinePos, Instancing_Stage_GLSL);
            pos = source.find("main()", nextLinePos);
        }
    }
    
    void InsertMaterialAssembly(std::string& source, std::vector<PKMaterialProperty>& materialProperties, bool* enableInstancing)
    {
        *enableInstancing = false;
        std::string output;
        size_t pos = 0;

        materialProperties.clear();

        while (true)
        {
            pos = StringUtilities::ExtractToken(pos, PK_SHADER_ATTRIB_MATERIAL_PROP, source, output, false);

            if (pos == std::string::npos)
            {
                break;
            }

            auto parts = StringUtilities::Split(output, " ");

            if (parts.size() != 2)
            {
                continue;
            }

            auto type = PK::Assets::GetElementType(parts.at(0).c_str());

            if (type == PKElementType::Invalid)
            {
                continue;
            }

            PKMaterialProperty prop;
            prop.type = type;
            WriteName(prop.name, parts.at(1).c_str());
            materialProperties.push_back(prop);
        }

        if (materialProperties.size() == 0)
        {
            auto standaloneToken = StringUtilities::ExtractToken(PK_SHADER_ATTRIB_INSTANCING_PROP, source, true);

            if (!standaloneToken.empty())
            {
                source.insert(0, Instancing_Standalone_GLSL);
                *enableInstancing = true;
            }

            return;
        }

        std::string block = "struct PK_MaterialPropertyBlock\n{\n";

        for (auto& prop : materialProperties)
        {
            block += "    " + GetGLSLType(prop.type) + " " + std::string(prop.name) + ";\n";
        }

        block += "};\n";

        for (auto& prop : materialProperties)
        {
            auto name = std::string(prop.name);

            switch (prop.type)
            {
                case PKElementType::Texture2DHandle: block += "uint " + name + "_Handle;\n"; break;
                case PKElementType::Texture3DHandle: block += "uint " + name + "_Handle;\n"; break;
                case PKElementType::TextureCubeHandle: block += "uint " + name + "_Handle;\n"; break;
                default: block += GetGLSLType(prop.type) + " " + name + ";\n"; break;
            }
        }

        block += Instancing_Base_GLSL;

        for (auto& prop : materialProperties)
        {
            auto name = std::string(prop.name);

            switch (prop.type)
            {
                case PKElementType::Texture2DHandle:
                case PKElementType::Texture3DHandle:
                case PKElementType::TextureCubeHandle:
                    block += "    " + name + "_Handle = prop." + name + ";\n";
                    break;

                default:
                    block += "    " + name + " = prop." + name + ";\n";
                    break;
            }
        }

        block += "}\n";

        for (auto& prop : materialProperties)
        {
            auto name = std::string(prop.name);

            switch (prop.type)
            {
                case PKElementType::Texture2DHandle: block += "#define " + name + " pk_Instancing_Textures2D[" + name + "_Handle]\n"; break;
                case PKElementType::Texture3DHandle: block += "#define " + name + " pk_Instancing_Textures3D[" + name + "_Handle]\n"; break;
                case PKElementType::TextureCubeHandle: block += "#define " + name + " pk_Instancing_TexturesCube[" + name + "_Handle]\n"; break;
                default: break;
            }
        }

        source.insert(0, block);
        *enableInstancing = true;
    }
}