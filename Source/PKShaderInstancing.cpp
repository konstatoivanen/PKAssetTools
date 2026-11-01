#include "PKShaderInstancing.h"
#include "PKStringUtilities.h"
#include "PKShaderUtilities.h"
#include "PKAssetWriter.h"

namespace PKAssets::Shader::Instancing
{
    void InsertMaterialAssembly(std::string& source, std::vector<PKMaterialProperty>& outProperties, bool* outEnabled, bool* outNoFragInstancing)
    {
        *outEnabled = false;
        outProperties.clear();

        const auto noFragment = !StringUtilities::ExtractToken(PK_SHADER_ATTRIB_INSTANCING_NOFRAG_PROP, source, true).empty();
        if (outNoFragInstancing)
        {
            *outNoFragInstancing = noFragment;
        }

        std::string output;
        auto pos = 0ull;

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

            auto type = StringToPKUniform(parts[0].c_str());

            if (type != PKUniform::Invalid)
            {
                PKMaterialProperty prop{};
                prop.type = type;
                WriteName(prop.name, parts[1].c_str());
                outProperties.push_back(prop);
            }
        }

        if (outProperties.empty())
        {
            if (!StringUtilities::ExtractToken(PK_SHADER_ATTRIB_INSTANCING_PROP, source, true).empty())
            {
                auto header = (noFragment ? Instancing_NoFrag_SLANG : Instancing_Default_SLANG) + std::string(Instancing_Common_Resources_SLANG) + "}\n";
                source.insert(0, header);
                *outEnabled = true;
            }
            else
            {
                source.insert(0, Instancing_Disabled_SLANG);
            }

            return;
        }

        std::string structFields;
        std::string propertyAccessors;

        for (const auto& prop : outProperties)
        {
            const std::string name(prop.name);
            const std::string fieldType = PKUniformToSlangString(prop.type);
            structFields += fieldType + " " + name + ";\n";

            switch (prop.type)
            {
                case PKUniform::Texture2D: propertyAccessors += "property Texture2D " + name + " {get{return pk_Heap_Textures2D[pk_Instancing_PropertyBlock." + name + "];}}\n"; break;
                case PKUniform::Texture3D: propertyAccessors += "property Texture3D " + name + " {get{return pk_Heap_Textures3D[pk_Instancing_PropertyBlock." + name + "];}}\n"; break;
                case PKUniform::TextureCube: propertyAccessors += "property TextureCube " + name + " {get{return pk_Heap_TexturesCube[pk_Instancing_PropertyBlock." + name + "];}}\n"; break;
                default: propertyAccessors += "property " + fieldType + " " + name + " {get{return pk_Instancing_PropertyBlock." + name + ";}}\n"; break;
            }
        }

        auto header = "struct PK_MaterialPropertyBlock\n{\n" + structFields + "};\n" +
            (noFragment ? Instancing_NoFrag_SLANG : Instancing_Default_SLANG) +
            Instancing_Material_Resources_SLANG +
            Instancing_Common_Resources_SLANG +
            Instancing_Material_Assign_SLANG +
            propertyAccessors;

        source.insert(0, header);
        *outEnabled = true;
    }
}
