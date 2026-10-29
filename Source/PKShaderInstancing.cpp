#include <algorithm>
#include <unordered_set>
#include "PKShaderInstancing.h"
#include "PKStringUtilities.h"
#include "PKShaderUtilities.h"
#include "PKAssetWriter.h"

namespace PKAssets::Shader::Instancing
{
    void InsertMaterialAssembly(std::string& source, std::string& outheader, std::vector<PKMaterialProperty>& outProperties, bool* outEnabled)
    {
        struct ParsedMaterialProperty
        {
            PKMaterialProperty prop{};
            std::string fieldType;
            std::string targetType;
        };

        *outEnabled = false;
        outheader = Instancing_Disabled_SLANG;
        outProperties.clear();
        
        auto noFragment = !StringUtilities::ExtractToken(PK_SHADER_ATTRIB_INSTANCING_NOFRAG_PROP, source, true).empty();

        std::vector<ParsedMaterialProperty> properties;
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

            if (parts.size() == 2)
            {
                std::string targetType;
                const auto type = ReflectElementType(parts[0], nullptr, &targetType);

                if (type != PKElementType::Invalid)
                {
                    ParsedMaterialProperty prop{};
                    prop.prop.type = type;
                    WriteName(prop.prop.name, parts[1].c_str());
                    prop.fieldType = parts[0];
                    prop.targetType = targetType;
                    properties.push_back(prop);
                }
            }
        }

        if (properties.empty())
        {
            if (!StringUtilities::ExtractToken(PK_SHADER_ATTRIB_INSTANCING_PROP, source, true).empty())
            {
                outheader =  (noFragment ? Instancing_NoFrag_SLANG : Instancing_Default_SLANG) + std::string(Instancing_Common_Resources_SLANG) + "}\n";
                *outEnabled = true;
            }

            return;
        }

        std::stable_sort(properties.begin(), properties.end(), [](const ParsedMaterialProperty& a, const ParsedMaterialProperty& b)
        {
            const auto alignA = PKElementTypeToScalarSize(a.prop.type);
            const auto alignB = PKElementTypeToScalarSize(b.prop.type);

            if (alignA != alignB)
            {
                return alignA > alignB;
            }

            return PKElementTypeToSize(a.prop.type) > PKElementTypeToSize(b.prop.type);
        });

        auto stride = 0u;
        auto maxAlign = 1u;
        std::string structFields;
        std::string propertyAccessors;
        outProperties.reserve(properties.size());


        for (const auto& prop : properties)
        {
            outProperties.push_back(prop.prop);
            const auto align = PKElementTypeToScalarSize(prop.prop.type);
            maxAlign = std::max(maxAlign, align);
            stride = (stride + align - 1u) & ~(align - 1u);
            stride += PKElementTypeToSize(prop.prop.type);

            const std::string name(prop.prop.name);
            const std::string getter = PKElementTypeIsResourceHandle(prop.prop.type) ? ".Get()" : "";
            structFields += prop.fieldType + " " + name + ";\n";
            propertyAccessors += "property " + prop.targetType + " " + name + " { get { return pk_Instancing_PropertyBlock." + name + getter + "; } }\n";
        }

        if ((stride % maxAlign) != 0u)
        {
            structFields += "uint8_t _pk_pad[" + std::to_string(maxAlign - (stride % maxAlign)) + "];\n";
        }

        outheader = "struct PK_MaterialPropertyBlock\n{\n" + structFields + "};\n" + 
                        (noFragment ? Instancing_NoFrag_SLANG : Instancing_Default_SLANG) +
                        Instancing_Material_Resources_SLANG + 
                        Instancing_Common_Resources_SLANG + 
                        Instancing_Material_Assign_SLANG + 
                        propertyAccessors;

        *outEnabled = true;
    }
}