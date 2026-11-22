#include <algorithm>
#include <sstream>
#include "PKStringUtilities.h"
#include "PKAssetWriter.h"
#include "PKShaderUtilities.h"

namespace PKAssets::Shader
{
    enum class ShaderPassType : uint8_t
    {
        Graphics,
        Compute,
        RayTracing
    };

    struct PragmaDirective
    {
        std::string pass;
        std::vector<std::string> args;
    };

    struct ParsedDirective
    {
        std::string pass;
        std::string name;
        std::vector<int16_t> values;
    };

    struct ParsedUniformDecl
    {
        std::string name;
        std::string fieldType;
        std::string targetType;
        PKUniform type = PKUniform::Invalid;
        uint16_t count = 1u;
        simplecpp::Token* endToken = nullptr;
    };

    struct TokenStatement
    {
        simplecpp::Token* beg = nullptr;
        simplecpp::Token* end = nullptr;
        simplecpp::Token* openBrace = nullptr;
        simplecpp::Token* closeBrace = nullptr;
        simplecpp::Token* functionName = nullptr;
        bool isFunction = false;
    };

    struct UniformMeta
    {
        const char* name = nullptr;
        const char* fieldType = nullptr;
        const char* defaultTemplate = nullptr;
        PKUniform type = PKUniform::Invalid;
        bool requiresTemplate = false;
    };

    constexpr const static UniformMeta PK_UNIFORM_RESOURCE_META[] =
    {
        { "SamplerState",                    "byte",   nullptr,    PKUniform::Sampler,               false },
        { "SamplerComparisonState",          "byte",   nullptr,    PKUniform::Sampler,               false },
        { "Texture2D",                       "ushort", "<float4>", PKUniform::Texture,               false },
        { "Texture3D",                       "ushort", "<float4>", PKUniform::Texture,               false },
        { "TextureCube",                     "ushort", "<float4>", PKUniform::Texture,               false },
        { "Texture2DArray",                  "ushort", "<float4>", PKUniform::Texture,               false },
        { "TextureCubeArray",                "ushort", "<float4>", PKUniform::Texture,               false },
        { "Texture2DMS",                     "ushort", "<float4>", PKUniform::Texture,               false },
        { "RWTexture2D",                     "ushort", "<float4>", PKUniform::Image,                 false },
        { "RWTexture2DArray",                "ushort", "<float4>", PKUniform::Image,                 false },
        { "RWTexture3D",                     "ushort", "<float4>", PKUniform::Image,                 false },
        { "StructuredBuffer",                "ushort", nullptr,    PKUniform::StorageBuffer,         true  },
        { "RWStructuredBuffer",              "ushort", nullptr,    PKUniform::StorageBuffer,         true  },
        { "ByteAddressBuffer",               "ushort", nullptr,    PKUniform::StorageBuffer,         false },
        { "RWByteAddressBuffer",             "ushort", nullptr,    PKUniform::StorageBuffer,         false },
        { "ConstantBuffer",                  "ushort", nullptr,    PKUniform::ConstantBuffer,        true  },
        { "RaytracingAccelerationStructure", "ulong",  nullptr,    PKUniform::AccelerationStructure, false },
    };

    const static char* PKComparison_NAMES[] =
    {
        "Off",
        "Never",
        "Less",
        "Equal",
        "LessEqual",
        "Greater",
        "NotEqual",
        "GreaterEqual",
        "Always",
    };

    const static char* PKCullMode_NAMES[] =
    {
        "Off",
        "Front",
        "Back"
    };

    const static char* PKBlendFactor_NAMES[] =
    {
        "None",
        "One",
        "Zero",
        "SrcColor",
        "SrcAlpha",
        "DstColor",
        "DstAlpha",
        "OneMinusSrcColor",
        "OneMinusSrcAlpha",
        "OneMinusDstColor",
        "OneMinusDstAlpha",
        "ConstColor",
        "OneMinusConstColor",
        "ConstAlpha",
        "OneMinusConstAlpha",
    };

    const static char* PKBlendOp_NAMES[] =
    {
        "None",
        "Add",
        "Subtract",
        "ReverseSubtract",
        "Min",
        "Max",
    };

    const static char* PKRasterMode_NAMES[] =
    {
        "Default",
        "OverEstimate",
        "UnderEstimate",
    };


    static ShaderPassType GetShaderPassType(PKShaderStage stage)
    {
        switch (stage)
        {
            case PKShaderStage::Compute: return ShaderPassType::Compute;
            case PKShaderStage::RayGeneration:
            case PKShaderStage::RayMiss:
            case PKShaderStage::RayClosestHit:
            case PKShaderStage::RayAnyHit:
            case PKShaderStage::RayIntersection: return ShaderPassType::RayTracing;
            default: return ShaderPassType::Graphics;
        }
    }

    static const ShaderPass* FindPass(const ShaderPasses& passes, const std::string& name)
    {
        for (const auto& pass : passes)
        {
            if (pass.name == name)
            {
                return &pass;
            }
        }

        return nullptr;
    }

    static ShaderPass& FindOrAddPass(ShaderPasses& passes, const std::string& name)
    {
        for (auto& pass : passes)
        {
            if (pass.name == name)
            {
                return pass;
            }
        }

        passes.push_back(ShaderPass{ name, 0u });

        return passes.back();
    }

    static const EntryPointInfo* FindActiveEntryPoint(const std::string& name, const std::vector<EntryPointInfo>& activeEntries)
    {
        for (const auto& entry : activeEntries)
        {
            if (entry.name == name)
            {
                return &entry;
            }
        }

        return nullptr;
    }


    static int ExtractPragmaDirectives(const char* token, std::string& source, const ShaderPasses& passes, std::vector<PragmaDirective>& outDirectives, bool requirePasses)
    {
        outDirectives.clear();

        std::string output;
        auto pos = 0ull;

        while (true)
        {
            pos = StringUtilities::ExtractToken(pos, token, source, output, false);

            if (pos == std::string::npos)
            {
                break;
            }

            auto parts = StringUtilities::Split(output, " \n\r");

            if (parts.empty())
            {
                printf("Error: Pragma '%s' is missing pass and arguments!\n", token);
                return -1;
            }

            if (requirePasses && parts[0] != PK_SHADER_ATTRIB_PASS_ANY && !FindPass(passes, parts[0]))
            {
                printf("Error: Pragma '%s' references undeclared pass '%s'!\n", token, parts[0].c_str());
                return -1;
            }

            PragmaDirective directive{};
            directive.pass = parts[0];
            directive.args.assign(parts.begin() + 1, parts.end());
            outDirectives.push_back(directive);
        }

        std::stable_sort(outDirectives.begin(), outDirectives.end(), [](const PragmaDirective& a, const PragmaDirective& b)
        {
            const auto isAnyA = a.pass == PK_SHADER_ATTRIB_PASS_ANY;
            const auto isAnyB = b.pass == PK_SHADER_ATTRIB_PASS_ANY;
            return isAnyA != isAnyB ? isAnyA : false;
        });

        return 0;
    }

    template<typename TFunc>
    static bool ExtractStateAttribute(const char* token, uint32_t minArgs, uint32_t maxArgs, std::string& source, ShaderPasses& passes, TFunc&& apply)
    {
        std::vector<PragmaDirective> directives;

        if (ExtractPragmaDirectives(token, source, passes, directives, true) != 0)
        {
            return false;
        }

        for (const auto& directive : directives)
        {
            if (directive.args.size() < minArgs || directive.args.size() > maxArgs)
            {
                printf("Error: Invalid argument count for '%s' in pass '%s'!\n", token, directive.pass.c_str());
                return false;
            }

            for (auto& pass : passes)
            {
                if (directive.pass == PK_SHADER_ATTRIB_PASS_ANY || directive.pass == pass.name)
                {
                    apply(pass.attributes, directive.args);
                }
            }
        }

        return true;
    }

    static bool FindStructDefinition(const std::string& source, const std::string& structName, size_t* outOpen, size_t* outClose)
    {
        auto pos = 0ull;

        while (pos < source.size())
        {
            pos = StringUtilities::SkipWhitespaceAndComments(source, pos);

            if (pos >= source.size())
            {
                return false;
            }

            const auto nextPos = StringUtilities::SkipStringLiteral(source, pos);

            if (nextPos != pos)
            {
                pos = nextPos;
                continue;
            }

            if (StringUtilities::IsAlphaNumeric(source[pos]))
            {
                const auto wordBeg = pos;

                while (pos < source.size() && StringUtilities::IsAlphaNumeric(source[pos]))
                {
                    pos++;
                }

                if (pos - wordBeg == 6ull && source.compare(wordBeg, 6ull, "struct") == 0)
                {
                    const auto nameBeg = StringUtilities::SkipWhitespaceAndComments(source, pos);
                    auto nameEnd = nameBeg;

                    while (nameEnd < source.size() && StringUtilities::IsAlphaNumeric(source[nameEnd]))
                    {
                        nameEnd++;
                    }

                    if (nameEnd - nameBeg == structName.size() && source.compare(nameBeg, structName.size(), structName) == 0)
                    {
                        auto bracePos = StringUtilities::SkipWhitespaceAndComments(source, nameEnd);

                        if (bracePos < source.size() && source[bracePos] == '{')
                        {
                            const auto openBrace = bracePos;
                            auto depth = 0;

                            while (bracePos < source.size())
                            {
                                bracePos = StringUtilities::SkipWhitespaceAndComments(source, bracePos);

                                if (bracePos >= source.size())
                                {
                                    return false;
                                }

                                if (source[bracePos] == '{')
                                {
                                    depth++;
                                }
                                else if (source[bracePos] == '}')
                                {
                                    if (--depth == 0)
                                    {
                                        *outOpen = openBrace;
                                        *outClose = bracePos;
                                        return true;
                                    }
                                }

                                bracePos++;
                            }

                            return false;
                        }
                    }
                }

                continue;
            }

            pos++;
        }

        return false;
    }


    static void EraseTokenRange(simplecpp::TokenList& tokens, simplecpp::Token* first, simplecpp::Token* last)
    {
        auto* token = first;

        while (token)
        {
            auto* next = token == last ? nullptr : token->next;
            tokens.deleteToken(token);
            token = next;
        }
    }

    static simplecpp::Token* FindTokenScope(simplecpp::Token* start, simplecpp::Token* end, char openOp, char closeOp)
    {
        if (!start || start->op != openOp)
        {
            return nullptr;
        }

        auto depth = 0;

        for (auto* token = start; token && token != end; token = token->next)
        {
            if (token->op == openOp)
            {
                depth++;
            }
            else if (token->op == closeOp)
            {
                if (--depth == 0)
                {
                    return token;
                }
            }
        }

        return nullptr;
    }

    static simplecpp::Token* SkipLeadingAttributes(simplecpp::Token* start, simplecpp::Token* end)
    {
        auto* token = start;

        while (token && token != end && token->op == '[')
        {
            auto* closeBracket = FindTokenScope(token, end, '[', ']');

            if (!closeBracket)
            {
                break;
            }

            token = closeBracket->next;
        }

        return token;
    }

    static simplecpp::Token* FindTemplateArgumentScope(simplecpp::Token* start)
    {
        if (!start || start->op != '<')
        {
            return nullptr;
        }

        auto angleDepth = 0;
        auto parenDepth = 0;

        for (auto* token = start; token; token = token->next)
        {
            if (token->op == ';' || token->op == '{' || token->op == '}' || token->op == '?')
            {
                return nullptr;
            }

            if ((token->op == '&' || token->op == '|') && token->next && token->next->op == token->op)
            {
                return nullptr;
            }

            if (token->op == '(' || token->op == '[')
            {
                parenDepth++;
            }
            else if (token->op == ')' || token->op == ']')
            {
                if (--parenDepth < 0)
                {
                    return nullptr;
                }
            }
            else if (parenDepth == 0)
            {
                if (token->op == '<')
                {
                    angleDepth++;
                }
                else if (token->op == '>')
                {
                    if (--angleDepth == 0)
                    {
                        return token;
                    }
                }
            }
        }

        return nullptr;
    }
    
    static const char* ParseUniformDeclaration(simplecpp::Token* typeBeg, simplecpp::Token* limit, ParsedUniformDecl& outDecl)
    {
        if (!typeBeg || !typeBeg->name)
        {
            return "Unexpected token";
        }

        auto* typeEnd = typeBeg;

        if (typeEnd->next && typeEnd->next->op == '<')
        {
            typeEnd = FindTokenScope(typeEnd->next, limit, '<', '>');

            if (!typeEnd)
            {
                return "Unclosed template argument";
            }
        }

        auto* nameToken = typeEnd->next;

        if (!nameToken || !nameToken->name)
        {
            return "Missing field name";
        }

        uint16_t count = 1u;
        simplecpp::Token* endToken = nullptr;

        if (nameToken->next && nameToken->next->op == ';')
        {
            endToken = nameToken->next;
        }
        else if (nameToken->next && nameToken->next->op == '[')
        {
            auto* closeBracket = FindTokenScope(nameToken->next, limit, '[', ']');
            auto* countToken = nameToken->next->next;

            if (!closeBracket || !closeBracket->next || closeBracket->next->op != ';' || !countToken || !countToken->number || countToken->next != closeBracket)
            {
                return "Invalid array declaration";
            }

            count = (uint16_t)std::stoul(countToken->str());
            endToken = closeBracket->next;
        }
        else
        {
            return "Expected ';'";
        }

        std::string typeText;

        for (auto* token = typeBeg; token != nameToken; token = token->next)
        {
            typeText += token->str();
        }

        outDecl.type = ReflectUniformType(typeText, &outDecl.fieldType, &outDecl.targetType);

        if (outDecl.type == PKUniform::Invalid)
        {
            return "Unsupported field type";
        }

        outDecl.name = nameToken->str();
        outDecl.count = count;
        outDecl.endToken = endToken;
        return nullptr;
    }

    static const simplecpp::Token* FindFunctionOpenParen(const simplecpp::Token* token)
    {
        if (!token || !token->next)
        {
            return nullptr;
        }

        if (token->next->op == '(')
        {
            return token->next;
        }

        if (token->next->op == '<')
        {
            const auto* close = FindTemplateArgumentScope(token->next);

            if (close && close->next && close->next->op == '(')
            {
                return close->next;
            }
        }

        return nullptr;
    }

    static uint32_t GetFunctionArityRange(const simplecpp::Token* openParen, uint32_t* outMinArgs = nullptr)
    {
        if (outMinArgs)
        {
            *outMinArgs = 0u;
        }

        if (!openParen || openParen->op != '(')
        {
            return 0u;
        }

        const auto* closeParen = FindTokenScope(const_cast<simplecpp::Token*>(openParen), nullptr, '(', ')');

        if (!closeParen || openParen->next == closeParen)
        {
            return 0u;
        }

        auto maxArgs = 1u;
        auto minArgs = 0u;
        auto hasDefault = false;
        auto depth = 0;

        for (const auto* token = openParen->next; token && token != closeParen; token = token->next)
        {
            if (depth == 0 && token->op == '<')
            {
                if (const auto* closeAngle = FindTemplateArgumentScope(const_cast<simplecpp::Token*>(token)))
                {
                    const auto isCtorOrScope = closeAngle->next && (closeAngle->next->op == '(' || closeAngle->next->op == '.' || closeAngle->next->op == ':');

                    if ((outMinArgs && !hasDefault) || isCtorOrScope)
                    {
                        token = closeAngle;
                        continue;
                    }
                }
            }

            if (token->op == '(' || token->op == '[' || token->op == '{')
            {
                depth++;
            }
            else if (token->op == ')' || token->op == ']' || token->op == '}')
            {
                depth = std::max(0, depth - 1);
            }
            else if (outMinArgs && depth == 0 && token->op == '=')
            {
                hasDefault = true;
            }
            else if (depth == 0 && token->op == ',')
            {
                if (!hasDefault)
                {
                    minArgs++;
                }

                hasDefault = false;
                maxArgs++;
            }
        }

        if (!hasDefault)
        {
            minArgs++;
        }

        if (outMinArgs)
        {
            *outMinArgs = minArgs;
        }

        return maxArgs;
    }

    static std::string GetTokenUsageKey(const simplecpp::Token* token)
    {
        if (const auto* openParen = FindFunctionOpenParen(token))
        {
            const auto maxArgs = GetFunctionArityRange(openParen);
            return token->str() + "#" + std::to_string(maxArgs);
        }

        return token->str();
    }

    static uint32_t CountIdentifierInRange(const simplecpp::Token* first, const simplecpp::Token* last, const std::string& key)
    {
        auto count = 0u;

        for (const auto* token = first; token; token = token->next)
        {
            if (token->name && (!token->previous || token->previous->op != '.') && GetTokenUsageKey(token) == key)
            {
                count++;
            }

            if (token == last)
            {
                break;
            }
        }

        return count;
    }

    static uint32_t GetIdentifierUsageCount(const std::unordered_map<std::string, uint32_t>& usageMap, const std::string& name)
    {
        const auto iterator = usageMap.find(name);
        return iterator != usageMap.end() ? iterator->second : 0u;
    }

    static std::vector<TokenStatement> ScanTopLevelStatements(simplecpp::TokenList& tokens)
    {
        std::vector<TokenStatement> statements;
        simplecpp::Token* statementBeg = nullptr;
        simplecpp::Token* openBrace = nullptr;
        simplecpp::Token* closeBrace = nullptr;
        simplecpp::Token* functionName = nullptr;
        auto hasEquals = false;

        for (auto* token = tokens.front(); token; token = token->next)
        {
            if (!statementBeg)
            {
                statementBeg = token;
            }

            if (token->op == '[')
            {
                auto* closeBracket = FindTokenScope(token, nullptr, '[', ']');

                if (!closeBracket)
                {
                    break;
                }

                token = closeBracket;
            }
            else if (token->op == '<' && !openBrace && !hasEquals)
            {
                auto* closeAngle = FindTemplateArgumentScope(token);

                if (closeAngle && closeAngle->next && closeAngle->next->op == '(' && token->previous && token->previous->name)
                {
                    functionName = token->previous;
                    token = closeAngle;
                }
            }
            else if (token->op == '(')
            {
                if (!openBrace && !hasEquals && token->previous && token->previous->name)
                {
                    functionName = token->previous;
                }

                auto* closeParen = FindTokenScope(token, nullptr, '(', ')');

                if (!closeParen)
                {
                    break;
                }

                token = closeParen;
            }
            else if (token->op == '=')
            {
                hasEquals = true;
                functionName = nullptr;
            }
            else if (token->op == '{')
            {
                openBrace = token;
                closeBrace = FindTokenScope(token, nullptr, '{', '}');

                if (!closeBrace)
                {
                    break;
                }

                token = closeBrace;
                const auto isFunction = functionName != nullptr;
                const auto isProperty = statementBeg->name && statementBeg->str() == "property";

                if (isFunction || isProperty)
                {
                    if (token->next && token->next->op == ';')
                    {
                        token = token->next;
                    }

                    statements.push_back({ statementBeg, token, openBrace, closeBrace, isFunction ? functionName : nullptr, isFunction });
                    statementBeg = nullptr;
                    openBrace = nullptr;
                    closeBrace = nullptr;
                    functionName = nullptr;
                    hasEquals = false;
                }
            }
            else if (token->op == ';')
            {
                const auto isPrototype = !openBrace && !hasEquals && functionName;
                statements.push_back({ statementBeg, token, openBrace, closeBrace, isPrototype ? functionName : nullptr, isPrototype });

                statementBeg = nullptr;
                openBrace = nullptr;
                closeBrace = nullptr;
                functionName = nullptr;
                hasEquals = false;
            }
        }

        return statements;
    }

    static simplecpp::Token* FindPropertyDeclarationName(const TokenStatement& statement)
    {
        if (statement.beg &&
            statement.openBrace &&
            statement.beg->name &&
            statement.beg->str() == "property" &&
            statement.openBrace->previous &&
            statement.openBrace->previous->name)
        {
            return statement.openBrace->previous;
        }

        return nullptr;
    }

    static simplecpp::Token* FindVariableDeclarationName(const TokenStatement& statement)
    {
        if (statement.isFunction || statement.openBrace || !statement.end)
        {
            return nullptr;
        }

        auto* firstToken = SkipLeadingAttributes(statement.beg, statement.end);

        if (!firstToken || !firstToken->name)
        {
            return nullptr;
        }

        const auto& qualifier = firstToken->str();

        if (qualifier != "uniform" && qualifier != "groupshared" && qualifier != "static")
        {
            return nullptr;
        }

        for (auto* token = firstToken->next; token && token != statement.end->next; token = token->next)
        {
            if (token->op == '<')
            {
                token = FindTokenScope(token, statement.end->next, '<', '>');

                if (!token)
                {
                    return nullptr;
                }

                continue;
            }

            if (token->op == '(' || token->op == '=')
            {
                return nullptr;
            }

            if (token->op == '[' || token->op == ';')
            {
                return token->previous && token->previous->name ? token->previous : nullptr;
            }
        }

        return nullptr;
    }

    static std::unordered_map<std::string, uint32_t> BuildIdentifierUsageMap(const simplecpp::TokenList& tokens, const std::vector<TokenStatement>& statements)
    {
        std::unordered_map<std::string, uint32_t> counts;

        for (const auto* token = tokens.cfront(); token; token = token->next)
        {
            if (token->name && (!token->previous || token->previous->op != '.'))
            {
                counts[GetTokenUsageKey(token)]++;
            }
        }

        // Subtract declaration tokens so the map only stores actual reference counts
        for (const auto& statement : statements)
        {
            const simplecpp::Token* declToken = nullptr;

            if (statement.isFunction)
            {
                declToken = statement.functionName;
            }
            else if (auto* propToken = FindPropertyDeclarationName(statement))
            {
                declToken = propToken;
            }
            else if (auto* varToken = FindVariableDeclarationName(statement))
            {
                declToken = varToken;
            }

            if (declToken)
            {
                auto iterator = counts.find(GetTokenUsageKey(declToken));
                
                if (iterator != counts.end() && iterator->second > 0u)
                {
                    iterator->second--;
                }
            }
        }

        return counts;
    }


    #define DECLARE_STRING_TO_ENUM(TType, TFallback) TType StringTo##TType(const char* str) { return (TType)StringUtilities::FindStringIndex(TType##_NAMES, str, (uint32_t)TFallback); }
    DECLARE_STRING_TO_ENUM(PKUniform, PKUniform::Invalid)
    DECLARE_STRING_TO_ENUM(PKShaderStage, PKShaderStage::EnumCount)
    DECLARE_STRING_TO_ENUM(PKComparison, PKComparison::Off)
    DECLARE_STRING_TO_ENUM(PKCullMode, PKCullMode::Off)
    DECLARE_STRING_TO_ENUM(PKBlendFactor, PKBlendFactor::None)
    DECLARE_STRING_TO_ENUM(PKBlendOp, PKBlendOp::None)
    DECLARE_STRING_TO_ENUM(PKRasterMode, PKRasterMode::Default)
    #undef DECLARE_STRING_TO_ENUM

    PKColorMask StringToPKColorMask(const char* str)
    {
        auto length = str ? strnlen(str, 4) : 0;

        if (length == 0)
        {
            return PKColorMask::RGBA;
        }

        auto mask = (uint8_t)PKColorMask::NONE;

        for (auto i = 0u; i < length; ++i)
        {
            switch (str[i])
            {
                case 'X':
                case 'R': mask = mask | (uint8_t)PKColorMask::R; break;
                case 'Y':
                case 'G': mask = mask | (uint8_t)PKColorMask::G; break;
                case 'Z':
                case 'B': mask = mask | (uint8_t)PKColorMask::B; break;
                case 'W':
                case 'A': mask = mask | (uint8_t)PKColorMask::A; break;
                default: break;
            }
        }

        return (PKColorMask)mask;
    }

    PKUniform SpvFormatToPKUniform(SpvReflectFormat format)
    {
        switch (format)
        {
            case SPV_REFLECT_FORMAT_UNDEFINED:           return PKUniform::Invalid;
            case SPV_REFLECT_FORMAT_R16_SINT:            return PKUniform::Short;
            case SPV_REFLECT_FORMAT_R16G16_SINT:         return PKUniform::Short2;
            case SPV_REFLECT_FORMAT_R16G16B16_SINT:      return PKUniform::Short3;
            case SPV_REFLECT_FORMAT_R16G16B16A16_SINT:   return PKUniform::Short4;
            case SPV_REFLECT_FORMAT_R16_UINT:            return PKUniform::Ushort;
            case SPV_REFLECT_FORMAT_R16G16_UINT:         return PKUniform::Ushort2;
            case SPV_REFLECT_FORMAT_R16G16B16_UINT:      return PKUniform::Ushort3;
            case SPV_REFLECT_FORMAT_R16G16B16A16_UINT:   return PKUniform::Ushort4;
            case SPV_REFLECT_FORMAT_R16_SFLOAT:          return PKUniform::Half;
            case SPV_REFLECT_FORMAT_R16G16_SFLOAT:       return PKUniform::Half2;
            case SPV_REFLECT_FORMAT_R16G16B16_SFLOAT:    return PKUniform::Half3;
            case SPV_REFLECT_FORMAT_R16G16B16A16_SFLOAT: return PKUniform::Half4;
            case SPV_REFLECT_FORMAT_R32_SFLOAT:          return PKUniform::Float;
            case SPV_REFLECT_FORMAT_R32G32_SFLOAT:       return PKUniform::Float2;
            case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:    return PKUniform::Float3;
            case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT: return PKUniform::Float4;
            case SPV_REFLECT_FORMAT_R32_SINT:            return PKUniform::Int;
            case SPV_REFLECT_FORMAT_R32G32_SINT:         return PKUniform::Int2;
            case SPV_REFLECT_FORMAT_R32G32B32_SINT:      return PKUniform::Int3;
            case SPV_REFLECT_FORMAT_R32G32B32A32_SINT:   return PKUniform::Int4;
            case SPV_REFLECT_FORMAT_R32_UINT:            return PKUniform::Uint;
            case SPV_REFLECT_FORMAT_R32G32_UINT:         return PKUniform::Uint2;
            case SPV_REFLECT_FORMAT_R32G32B32_UINT:      return PKUniform::Uint3;
            case SPV_REFLECT_FORMAT_R32G32B32A32_UINT:   return PKUniform::Uint4;
            case SPV_REFLECT_FORMAT_R64_SINT:            return PKUniform::Long;
            case SPV_REFLECT_FORMAT_R64G64_SINT:         return PKUniform::Long2;
            case SPV_REFLECT_FORMAT_R64G64B64_SINT:      return PKUniform::Long3;
            case SPV_REFLECT_FORMAT_R64G64B64A64_SINT:   return PKUniform::Long4;
            case SPV_REFLECT_FORMAT_R64_UINT:            return PKUniform::Ulong;
            case SPV_REFLECT_FORMAT_R64G64_UINT:         return PKUniform::Ulong2;
            case SPV_REFLECT_FORMAT_R64G64B64_UINT:      return PKUniform::Ulong3;
            case SPV_REFLECT_FORMAT_R64G64B64A64_UINT:   return PKUniform::Ulong4;
            case SPV_REFLECT_FORMAT_R64_SFLOAT:          return PKUniform::Double;
            case SPV_REFLECT_FORMAT_R64G64_SFLOAT:       return PKUniform::Double2;
            case SPV_REFLECT_FORMAT_R64G64B64_SFLOAT:    return PKUniform::Double3;
            case SPV_REFLECT_FORMAT_R64G64B64A64_SFLOAT: return PKUniform::Double4;
        }

        return PKUniform::Invalid;
    }

    PKUniform ReflectUniformType(std::string_view typeText, std::string* outFieldType, std::string* outTargetType)
    {
        auto baseName = typeText;
        std::string_view templateArg{};

        const auto templateOpen = typeText.find('<');

        if (templateOpen != std::string_view::npos)
        {
            const auto templateClose = typeText.rfind('>');

            if (templateClose == std::string_view::npos || templateClose < templateOpen)
            {
                return PKUniform::Invalid;
            }

            baseName = typeText.substr(0ull, templateOpen);
            templateArg = typeText.substr(templateOpen, (templateClose + 1ull) - templateOpen);
        }

        if (templateArg.empty())
        {
            for (auto i = (uint32_t)PKUniform::Bool; i <= (uint32_t)PKUniform::Half4x4; ++i)
            {
                if (baseName == PKUniform_NAMES[i])
                {
                    if (outFieldType)
                    {
                        *outFieldType = std::string(baseName);
                    }

                    if (outTargetType)
                    {
                        *outTargetType = std::string(baseName);
                    }

                    return (PKUniform)i;
                }
            }
        }

        for (const auto& meta : PK_UNIFORM_RESOURCE_META)
        {
            if (baseName == meta.name)
            {
                if (meta.requiresTemplate && templateArg.empty())
                {
                    return PKUniform::Invalid;
                }

                if (outFieldType)
                {
                    *outFieldType = meta.fieldType;
                }

                if (outTargetType)
                {
                    *outTargetType = std::string(meta.name);

                    if (!templateArg.empty())
                    {
                        *outTargetType += std::string(templateArg);
                    }
                    else if (meta.defaultTemplate)
                    {
                        *outTargetType += meta.defaultTemplate;
                    }
                }

                return meta.type;
            }
        }

        return PKUniform::Invalid;
    }

    std::string CompileUniformProperty(PKUniform type, const std::string& targetType, const std::string& name, const std::string& valueExpr, bool isBitpacked)
    {
        std::string expr = valueExpr;

        if (type == PKUniform::AccelerationStructure)
        {
            expr = targetType + "(" + valueExpr + ")";
        }
        else if (PKUniformIsResourceHandle(type))
        {
            expr = "DescriptorHandle<" + targetType + ">(uint2(" + valueExpr + ",0u))";
        }
        else if (isBitpacked && type == PKUniform::Half)
        {
            expr = "half(f16tof32(" + valueExpr + "))";
        }
        else if (isBitpacked)
        {
            expr = targetType + "(" + valueExpr + ")";
        }

        return "property " + targetType + " " + name + "{get{return " + expr + ";}}\n";
    }

    SlangStage PKShaderStageToSlangStage(PKShaderStage stage)
    {
        switch (stage)
        {
            case PKShaderStage::Vertex:                return SLANG_STAGE_VERTEX;
            case PKShaderStage::TesselationControl:    return SLANG_STAGE_HULL;
            case PKShaderStage::TesselationEvaluation: return SLANG_STAGE_DOMAIN;
            case PKShaderStage::Geometry:              return SLANG_STAGE_GEOMETRY;
            case PKShaderStage::MeshTask:              return SLANG_STAGE_AMPLIFICATION;
            case PKShaderStage::MeshAssembly:          return SLANG_STAGE_MESH;
            case PKShaderStage::Fragment:              return SLANG_STAGE_FRAGMENT;
            case PKShaderStage::Compute:               return SLANG_STAGE_COMPUTE;
            case PKShaderStage::RayGeneration:         return SLANG_STAGE_RAY_GENERATION;
            case PKShaderStage::RayMiss:               return SLANG_STAGE_MISS;
            case PKShaderStage::RayClosestHit:         return SLANG_STAGE_CLOSEST_HIT;
            case PKShaderStage::RayAnyHit:             return SLANG_STAGE_ANY_HIT;
            case PKShaderStage::RayIntersection:       return SLANG_STAGE_INTERSECTION;
            default:                                   return SLANG_STAGE_NONE;
        }
    }

    const char* GetSlangStageAttribute(PKShaderStage stage)
    {
        switch (stage)
        {
            case PKShaderStage::Vertex:                return "[shader(\"vertex\")] ";
            case PKShaderStage::TesselationControl:    return "[shader(\"hull\")] ";
            case PKShaderStage::TesselationEvaluation: return "[shader(\"domain\")] ";
            case PKShaderStage::Geometry:              return "[shader(\"geometry\")] ";
            case PKShaderStage::MeshTask:              return "[shader(\"amplification\")] ";
            case PKShaderStage::MeshAssembly:          return "[shader(\"mesh\")] ";
            case PKShaderStage::Fragment:              return "[shader(\"fragment\")] ";
            case PKShaderStage::Compute:               return "[shader(\"compute\")] ";
            case PKShaderStage::RayGeneration:         return "[shader(\"raygeneration\")] ";
            case PKShaderStage::RayMiss:               return "[shader(\"miss\")] ";
            case PKShaderStage::RayClosestHit:         return "[shader(\"closesthit\")] ";
            case PKShaderStage::RayAnyHit:             return "[shader(\"anyhit\")] ";
            case PKShaderStage::RayIntersection:       return "[shader(\"intersection\")] ";
            default:                                   return "";
        }
    }

    std::string FormatErrorMessage(const std::string& name, const std::string& source, const std::string& error)
    {
        std::string output;

        for (const auto& line : StringUtilities::Split(error, "\r\n"))
        {
            output.append(line + "\n");
            auto lineIndex = 0ull;

            if (StringUtilities::ExtractDiagnosticLineIndex(line, name, &lineIndex) && lineIndex)
            {
                output.append(StringUtilities::GetLineAtIndex(source, lineIndex - 1ull));
            }
        }

        return output;
    }

    bool ExtractFlag(const char* token, std::string& source)
    {
        return !StringUtilities::ExtractToken(token, source, true).empty();
    }

    int ExtractEntryPoints(std::string& source, ShaderPasses& passes)
    {
        passes.clear();

        std::vector<PragmaDirective> directives;

        if (ExtractPragmaDirectives(PK_SHADER_ATTRIB_PROGRAM, source, passes, directives, false) != 0)
        {
            return -1;
        }

        auto sharedStageMask = 0u;
        std::vector<EntryPointInfo> sharedEntryPoints;

        for (const auto& directive : directives)
        {
            if (directive.args.size() != 2)
            {
                printf("Error: Entry point declaration for pass '%s' contains an invalid number of arguments (should be pass, stage, name)\n", directive.pass.c_str());
                return -1;
            }

            const auto stage = StringToPKShaderStage(directive.args[0].c_str());

            if (stage == PKShaderStage::EnumCount)
            {
                printf("Error: Unsupported shader stage '%s' specified!\n", directive.args[0].c_str());
                return -1;
            }

            EntryPointInfo info{};
            info.name = directive.args[1];
            info.stage = stage;

            if (source.find(info.name, 0u) == std::string::npos)
            {
                printf("Error: Definition for declared entry point '%s' was not found!\n", info.name.c_str());
                return -1;
            }

            if (directive.pass == PK_SHADER_ATTRIB_PASS_ANY)
            {
                if (sharedStageMask & (1u << (uint32_t)info.stage))
                {
                    printf("Error: Pass '%s' already declares an entry point for '%s'!\n", PK_SHADER_ATTRIB_PASS_ANY, PKShaderStage_NAMES[(uint32_t)info.stage]);
                    return -1;
                }

                sharedStageMask |= 1u << (uint32_t)info.stage;
                sharedEntryPoints.push_back(info);
            }
            else
            {
                auto& pass = FindOrAddPass(passes, directive.pass);

                if (!pass.entryPoints.empty() && GetShaderPassType(pass.entryPoints[0].stage) != GetShaderPassType(info.stage))
                {
                    printf("Error: Pass '%s' mixes incompatible shader stage types!\n", directive.pass.c_str());
                    return -1;
                }

                if (pass.stageMask & (1u << (uint32_t)info.stage))
                {
                    printf("Error: Pass '%s' already declares an entry point for '%s'!\n", directive.pass.c_str(), PKShaderStage_NAMES[(uint32_t)info.stage]);
                    return -1;
                }

                pass.stageMask |= 1u << (uint32_t)info.stage;
                pass.entryPoints.push_back(info);
            }
        }

        if (passes.empty())
        {
            if (sharedEntryPoints.empty())
            {
                printf("Error: Shader does not declare any entry points!\n");
                return -1;
            }

            const auto passType = GetShaderPassType(sharedEntryPoints[0].stage);

            for (auto i = 1ull; i < sharedEntryPoints.size(); ++i)
            {
                if (GetShaderPassType(sharedEntryPoints[i].stage) != passType)
                {
                    printf("Error: Pass '%s' mixes incompatible shader stage types!\n", PK_SHADER_ATTRIB_PASS_ANY);
                    return -1;
                }
            }

            auto& pass = FindOrAddPass(passes, PK_SHADER_ATTRIB_PASS_ANY);
            pass.stageMask = sharedStageMask;
            pass.entryPoints = std::move(sharedEntryPoints);
        }
        else if (!sharedEntryPoints.empty())
        {
            for (auto& pass : passes)
            {
                const auto passType = GetShaderPassType(pass.entryPoints[0].stage);
                
                for (const auto& info : sharedEntryPoints)
                {
                    const auto stageFlag = 1u << (uint32_t)info.stage;

                    if (GetShaderPassType(info.stage) == passType && (pass.stageMask & stageFlag) == 0u)
                    {
                        pass.entryPoints.push_back(info);
                        pass.stageMask |= stageFlag;
                    }
                }
            }
        }

        if (passes.size() > PK_ASSET_MAX_SHADER_PASSES)
        {
            printf("Error: Maximum number of shader passes exceeded (%llu / %u)!\n", passes.size(), PK_ASSET_MAX_SHADER_PASSES);
            return -1;
        }

        if (passes.empty())
        {
            printf("Error: Shader didn't declare any passes!\n");
            return -1;
        }

        return 0;
    }

    int ExtractMulticompiles(std::string& source, ShaderPasses& passes)
    {
        std::vector<PragmaDirective> pragmaDirectives;

        if (ExtractPragmaDirectives(PK_SHADER_ATTRIB_MULTI_COMPILE, source, passes, pragmaDirectives, true) != 0)
        {
            return -1;
        }

        std::vector<ParsedDirective> parsedDirectives;
        parsedDirectives.reserve(pragmaDirectives.size());

        for (const auto& pragma : pragmaDirectives)
        {
            if (pragma.args.size() < 3u)
            {
                printf("Error: Multicompile pragma in pass '%s' requires a directive name and at least two integer values!\n", pragma.pass.c_str());
                return -1;
            }

            const auto valueCount = pragma.args.size() - 1ull;

            if (valueCount > PK_ASSET_MAX_SHADER_DIRECTIVE_SIZE)
            {
                printf("Error: Multicompile directive '%s' in pass '%s' exceeds maximum value count (%llu / %u)!\n", pragma.args[0].c_str(), pragma.pass.c_str(), valueCount, PK_ASSET_MAX_SHADER_DIRECTIVE_SIZE);
                return -1;
            }

            ParsedDirective parsed{};
            parsed.pass = pragma.pass;
            parsed.name = pragma.args[0];
            parsed.values.reserve(valueCount);

            for (auto i = 1ull; i < pragma.args.size(); ++i)
            {
                int16_t value = 0;

                if (!StringUtilities::TryParseInt16(pragma.args[i], &value))
                {
                    printf("Error: Multicompile directive '%s' in pass '%s' has non-integer value '%s'!\n", parsed.name.c_str(), parsed.pass.c_str(), pragma.args[i].c_str());
                    return -1;
                }

                parsed.values.push_back(value);
            }

            parsedDirectives.push_back(std::move(parsed));
        }

        for (auto& pass : passes)
        {
            pass.directives.clear();
            pass.variantDefines.clear();

            for (const auto& parsed : parsedDirectives)
            {
                if (parsed.pass != PK_SHADER_ATTRIB_PASS_ANY && parsed.pass != pass.name)
                {
                    continue;
                }

                auto overridden = false;

                for (auto& existing : pass.directives)
                {
                    if (existing.name == parsed.name)
                    {
                        existing.values = parsed.values;
                        overridden = true;
                        break;
                    }
                }

                if (!overridden)
                {
                    if (pass.directives.size() >= PK_ASSET_MAX_SHADER_DIRECTIVES)
                    {
                        printf("Error: Maximum number of multicompile directives exceeded in pass '%s' (%u)!\n", pass.name.c_str(), PK_ASSET_MAX_SHADER_DIRECTIVES);
                        return -1;
                    }

                    ShaderDirective dir{};
                    dir.name = parsed.name;
                    dir.values = parsed.values;
                    pass.directives.push_back(std::move(dir));
                }
            }

            auto variantCount = 1ull;

            for (auto& dir : pass.directives)
            {
                if (variantCount * dir.values.size() > PK_ASSET_MAX_SHADER_VARIANTS)
                {
                    printf("Error: Maximum number of shader variants exceeded in pass '%s' (%u)!\n", pass.name.c_str(), PK_ASSET_MAX_SHADER_VARIANTS);
                    return -1;
                }

                dir.stride = (uint16_t)variantCount;
                variantCount *= dir.values.size();
            }

            pass.variantDefines.reserve(variantCount);
            const auto passDefine = pass.name != PK_SHADER_ATTRIB_PASS_ANY ? ("#define " + pass.name + "\n") : "";

            for (auto i = 0u; i < variantCount; ++i)
            {
                auto defines = passDefine;
                auto declareIndex = i;

                for (const auto& dir : pass.directives)
                {
                    const auto value = dir.values[declareIndex % dir.values.size()];
                    defines.append("#define ");
                    defines.append(dir.name);
                    defines.append(" ");
                    defines.append(std::to_string(value));
                    defines.append("\n");
                    declareIndex /= (uint32_t)dir.values.size();
                }

                pass.variantDefines.push_back(std::move(defines));
            }
        }

        return 0;
    }

    int ExtractStateAttributes(std::string& source, ShaderPasses& passes)
    {
        auto isValid = true;

        isValid &= ExtractStateAttribute(PK_SHADER_ATTRIB_ZWRITE, 1u, 1u, source, passes, [](auto& a, const auto& args)
        {
            a.zwrite = args[0] == "True" ? 1u : 0u;
        });

        isValid &= ExtractStateAttribute(PK_SHADER_ATTRIB_ZTEST, 1u, 1u, source, passes, [](auto& a, const auto& args)
        {
            a.ztest = StringToPKComparison(args[0].c_str());
        });

        isValid &= ExtractStateAttribute(PK_SHADER_ATTRIB_BLENDCOLOR, 3u, 3u, source, passes, [](auto& a, const auto& args)
        {
            a.blendOpColor = StringToPKBlendOp(args[0].c_str());
            a.blendSrcFactorColor = StringToPKBlendFactor(args[1].c_str());
            a.blendDstFactorColor = StringToPKBlendFactor(args[2].c_str());
        });

        isValid &= ExtractStateAttribute(PK_SHADER_ATTRIB_BLENDALPHA, 3u, 3u, source, passes, [](auto& a, const auto& args)
        {
            a.blendOpAlpha = StringToPKBlendOp(args[0].c_str());
            a.blendSrcFactorAlpha = StringToPKBlendFactor(args[1].c_str());
            a.blendDstFactorAlpha = StringToPKBlendFactor(args[2].c_str());
        });

        isValid &= ExtractStateAttribute(PK_SHADER_ATTRIB_OFFSET, 3u, 3u, source, passes, [](auto& a, const auto& args)
        {
            a.zoffsets[0] = std::stof(args[0]);
            a.zoffsets[1] = std::stof(args[1]);
            a.zoffsets[2] = std::stof(args[2]);
        });

        isValid &= ExtractStateAttribute(PK_SHADER_ATTRIB_RASTERMODE, 1u, 2u, source, passes, [](auto& a, const auto& args)
        {
            a.rasterMode = StringToPKRasterMode(args[0].c_str());
            a.overEstimation = args.size() > 1u ? (uint8_t)std::stoi(args[1]) : 0u;
        });

        isValid &= ExtractStateAttribute(PK_SHADER_ATTRIB_COLORMASK, 1u, 1u, source, passes, [](auto& a, const auto& args)
        {
            a.colorMask = StringToPKColorMask(args[0].c_str());
        });

        isValid &= ExtractStateAttribute(PK_SHADER_ATTRIB_CULL, 1u, 1u, source, passes, [](auto& a, const auto& args)
        {
            a.cull = StringToPKCullMode(args[0].c_str());
        });

        return isValid ? 0 : -1;
    }

    int ExtractExportedStructs(std::string& source, ShaderStructs& outStructs)
    {
        std::vector<std::string> structNames;
        std::string output;
        auto pos = 0ull;

        outStructs.clear();

        while (true)
        {
            pos = StringUtilities::ExtractToken(pos, PK_SHADER_ATTRIB_EXPORT_STRUCT, source, output, false);

            if (pos == std::string::npos)
            {
                break;
            }

            auto parts = StringUtilities::Split(output, " \n\r");

            if (parts.size() != 1u)
            {
                printf("Error: '%s' expects a single struct name argument!\n", PK_SHADER_ATTRIB_EXPORT_STRUCT);
                return -1;
            }

            if (std::find(structNames.begin(), structNames.end(), parts[0]) == structNames.end())
            {
                structNames.push_back(parts[0]);
            }
        }

        for (const auto& structName : structNames)
        {
            auto openBrace = 0ull;
            auto closeBrace = 0ull;

            if (!FindStructDefinition(source, structName, &openBrace, &closeBrace))
            {
                printf("Error: Definition for exported struct '%s' was not found!\n", structName.c_str());
                return -1;
            }

            const auto bodySource = source.substr(openBrace + 1ull, closeBrace - openBrace - 1ull);

            std::istringstream stream(bodySource);
            std::vector<std::string> filenames;
            simplecpp::OutputList outputList;
            simplecpp::TokenList bodyTokens(stream, filenames, structName, {}, &outputList);
            bodyTokens.removeComments();

            std::vector<ParsedUniformDecl> fields;

            for (auto* tokenBeg = bodyTokens.front(); tokenBeg;)
            {
                ParsedUniformDecl decl{};
                const auto* error = ParseUniformDeclaration(tokenBeg, nullptr, decl);

                if (error)
                {
                    printf("Error: %s near '%s' in exported struct '%s'!\n", error, tokenBeg->str().c_str(), structName.c_str());
                    return -1;
                }

                if (PKUniformIsResourceHandle(decl.type) && decl.count > 1u)
                {
                    printf("Error: Resource handle arrays are not supported in exported struct '%s' ('%s')!\n", structName.c_str(), decl.name.c_str());
                    return -1;
                }

                tokenBeg = decl.endToken->next;
                fields.push_back(std::move(decl));
            }

            if (!fields.size())
            {
                printf("Error: Exported struct '%s' has no fields!\n", structName.c_str());
                return -1;
            }

            std::stable_sort(fields.begin(), fields.end(), [](const ParsedUniformDecl& a, const ParsedUniformDecl& b)
            {
                const auto alignA = PKUniformToScalarSize(a.type);
                const auto alignB = PKUniformToScalarSize(b.type);

                if (alignA != alignB)
                {
                    return alignA > alignB;
                }

                return PKUniformToSize(a.type) * a.count > PKUniformToSize(b.type) * b.count;
            });

            auto stride = 0u;
            auto maxAlign = 4u;
            std::string structFields;
            std::string propertyAccessors;

            outStructs.push_back({structName});
            auto& outStruct = outStructs.back();
            outStruct.fields.reserve(fields.size());

            for (auto& field : fields)
            {
                const auto align = PKUniformToScalarSize(field.type);
                maxAlign = std::max(maxAlign, align);
                stride = (stride + align - 1u) & ~(align - 1u);

                PKShaderStructField parsed{};
                WriteName(parsed.name, field.name.c_str());
                parsed.type = field.type;
                parsed.count = (uint8_t)field.count;
                parsed.offset = (uint16_t)stride;
                outStruct.fields.push_back(parsed);

                stride += PKUniformToSize(field.type) * field.count;

                if (PKUniformIsResourceHandle(field.type))
                {
                    const auto handleName = field.name + "_Handle";
                    structFields += field.fieldType + " " + handleName + ";\n";
                    propertyAccessors += CompileUniformProperty(field.type, field.targetType, field.name, handleName, false);
                }
                else
                {
                    const auto arraySuffix = field.count > 1u ? ("[" + std::to_string(field.count) + "]") : "";
                    structFields += field.fieldType + " " + field.name + arraySuffix + ";\n";
                }
            }

            if (stride % maxAlign)
            {
                const auto padding = maxAlign - (stride % maxAlign);
                structFields += "uint8_t _pk_pad[" + std::to_string(padding) + "];\n";
                stride += padding;
            }

            outStruct.size = (uint16_t)stride;

            const auto compiledBody = "{\n" + structFields + propertyAccessors + "}";
            
            source.replace(openBrace, closeBrace - openBrace + 1ull, compiledBody);
        }

        return 0;
    }

    int PreprocessSource(const std::string& name, const std::string& source, std::vector<std::string>& filenames, simplecpp::TokenList& outTokens)
    {
        std::istringstream stream(source);
        simplecpp::OutputList outputList;

        simplecpp::TokenList rawTokens(stream, filenames, name, {}, &outputList);
        rawTokens.removeComments();

        simplecpp::FileDataCache fileData;
        simplecpp::preprocess(outTokens, rawTokens, filenames, fileData, {}, &outputList);

        if (!outputList.empty())
        {
            printf("\n ----------BEGIN PREPROCESS ERROR---------- \n");
            for (const auto& output : outputList)
            {
                auto filename = output.location.fileIndex < filenames.size() ? filenames[output.location.fileIndex] : name;
                printf("%s:%u: %s\n", filename.c_str(), output.location.line, output.msg.c_str());
            }
            printf("\n ----------END PREPROCESS ERROR---------- \n\n");
            return -1;
        }

        return 0;
    }

    void SetActiveEntryPoints(simplecpp::TokenList& tokens, const std::vector<EntryPointInfo>& activeEntries)
    {
        const auto statements = ScanTopLevelStatements(tokens);

        for (const auto& statement : statements)
        {
            if (statement.isFunction && statement.functionName && statement.openBrace)
            {
                const auto* entry = FindActiveEntryPoint(statement.functionName->str(), activeEntries);

                if (entry)
                {
                    statement.beg->setstr(std::string(GetSlangStageAttribute(entry->stage)) + statement.beg->str());
                }
            }
        }
    }

    void RemoveUnusedResourcesAndFunctions(simplecpp::TokenList& tokens, const std::vector<EntryPointInfo>& activeEntries)
    {
        auto changed = true;

        while (changed)
        {
            changed = false;
            const auto statements = ScanTopLevelStatements(tokens);
            const auto usageMap = BuildIdentifierUsageMap(tokens, statements);

            for (auto index = (int32_t)statements.size() - 1; index >= 0; --index)
            {
                const auto& statement = statements[index];

                if (statement.isFunction && statement.functionName)
                {
                    const auto& functionName = statement.functionName->str();

                    auto minArgs = 0u;
                    auto maxArgs = GetFunctionArityRange(FindFunctionOpenParen(statement.functionName), &minArgs);

                    auto totalRefs = 0u;
                    auto selfRefs = 0u;

                    for (auto arity = minArgs; arity <= maxArgs; ++arity)
                    {
                        const auto functionKey = functionName + "#" + std::to_string(arity);
                        totalRefs += GetIdentifierUsageCount(usageMap, functionKey);
                        selfRefs += CountIdentifierInRange(statement.openBrace, statement.end, functionKey);
                    }

                    if (!FindActiveEntryPoint(functionName, activeEntries) && totalRefs <= selfRefs)
                    {
                        EraseTokenRange(tokens, statement.beg, statement.end);
                        changed = true;
                    }

                    continue;
                }

                if (auto* propertyNameToken = FindPropertyDeclarationName(statement))
                {
                    if (GetIdentifierUsageCount(usageMap, propertyNameToken->str()) == 0u)
                    {
                        EraseTokenRange(tokens, statement.beg, statement.end);
                        changed = true;
                    }

                    continue;
                }

                if (auto* variableNameToken = FindVariableDeclarationName(statement))
                {
                    if (GetIdentifierUsageCount(usageMap, variableNameToken->str()) == 0u)
                    {
                        EraseTokenRange(tokens, statement.beg, statement.end);
                        changed = true;
                    }
                }
            }
        }
    }

    void ExtractPushConstants(simplecpp::TokenList& tokens, SourcePushConstants& outConstants)
    {
        const auto statements = ScanTopLevelStatements(tokens);

        for (const auto& statement : statements)
        {
            if (statement.isFunction || statement.openBrace || !statement.end)
            {
                continue;
            }

            auto* token = SkipLeadingAttributes(statement.beg, statement.end);

            if (!token || !token->name || token->str() != "uniform")
            {
                continue;
            }

            ParsedUniformDecl decl{};

            if (ParseUniformDeclaration(token->next, statement.end, decl) != nullptr || decl.endToken != statement.end)
            {
                continue;
            }

            auto found = false;

            for (auto& member : outConstants)
            {
                if (member.name == decl.name)
                {
                    if (member.targetType != decl.targetType || member.count != decl.count)
                    {
                        printf("Warning constant parameter '%s' was declared again with a different format '%s'\n", member.name.c_str(), decl.targetType.c_str());
                    }

                    found = true;
                    break;
                }
            }

            if (!found)
            {
                SourcePushConstant constant{};
                constant.name = std::move(decl.name);
                constant.fieldType = std::move(decl.fieldType);
                constant.targetType = std::move(decl.targetType);
                constant.type = decl.type;
                constant.count = decl.count;
                outConstants.push_back(std::move(constant));
            }

            EraseTokenRange(tokens, statement.beg, statement.end);
        }
    }

    void CompilePushConstantBlock(std::string& source, SourcePushConstants& constants)
    {
        if (constants.empty())
        {
            return;
        }

        std::stable_sort(constants.begin(), constants.end(), [](const SourcePushConstant& a, const SourcePushConstant& b)
        {
            const auto sizeA = PKUniformToSize(a.type) * a.count;
            const auto sizeB = PKUniformToSize(b.type) * b.count;
            const auto isPackedA = sizeA < 4u;
            const auto isPackedB = sizeB < 4u;

            if (isPackedA != isPackedB)
            {
                return !isPackedA;
            }

            const auto alignA = PKUniformToScalarSize(a.type);
            const auto alignB = PKUniformToScalarSize(b.type);
            return alignA != alignB ? alignA > alignB : sizeA > sizeB;
        });

        auto memberIndex = 0u;
        auto packedByteOffset = 0u;
        std::string blockFields;
        std::string resourceAccessors;

        for (auto& member : constants)
        {
            const auto elementSize = PKUniformToSize(member.type);
            const auto totalSize = elementSize * member.count;
            const auto isHandle = PKUniformIsResourceHandle(member.type);

            if (totalSize < 4u)
            {
                packedByteOffset = (packedByteOffset + elementSize - 1u) & ~(elementSize - 1u);
                const auto wordIndex = packedByteOffset / 4u;

                member.memberIndex = (uint16_t)(memberIndex + wordIndex);
                member.memberSize = 4u;
                member.memberAlignment = 4u;
                member.bitOffset = (uint8_t)((packedByteOffset % 4u) * 8u);
                member.bitSize = (uint8_t)(totalSize * 8u);
                packedByteOffset += totalSize;

                const auto extractExpr = "bitfieldExtract(pk_packed_pc_" + std::to_string(wordIndex) + "," + std::to_string(member.bitOffset) + "," + std::to_string(member.bitSize) + ")";
                resourceAccessors += CompileUniformProperty(member.type, member.targetType, member.name, extractExpr, true);
                continue;
            }

            member.memberIndex = (uint16_t)(memberIndex++);
            member.bitOffset = 0u;
            member.bitSize = 0u;

            // Single resource access was handled above. no need to check count here.
            if (isHandle && member.type != PKUniform::AccelerationStructure)
            {
                const auto wordCount = (member.count + 1u) / 2u;
                member.memberSize = (uint16_t)(wordCount * 4u);
                member.memberAlignment = 4u;
                blockFields += "uint " + member.name + "_Handle[" + std::to_string(wordCount) + "];\n";
                resourceAccessors += "PK_PC_HANDLE_ARRAY(" + member.targetType + "," + member.name + ")\n";
                continue;
            }

            member.memberSize = (uint16_t)totalSize;
            member.memberAlignment = (uint8_t)PKUniformToScalarSize(member.type);

            if (isHandle)
            {
                const auto fieldName = member.name + "_Handle";
                blockFields += member.fieldType + " " + fieldName + ";\n";
                resourceAccessors += CompileUniformProperty(member.type, member.targetType, member.name, fieldName, false);
                continue;
            }

            const auto arraySuffix = member.count > 1u ? "[" + std::to_string(member.count) + "]" : "";
            blockFields += member.fieldType + " " + member.name + arraySuffix + ";\n";
        }

        const auto packedWordCount = (packedByteOffset + 3u) / 4u;

        for (auto wordIndex = 0u; wordIndex < packedWordCount; ++wordIndex)
        {
            blockFields += "uint pk_packed_pc_" + std::to_string(wordIndex) + ";\n";
        }

        source.insert(0ull, "[[vk::push_constant]] cbuffer pk_global_push_constant_block\n{\n" + blockFields + "};\n" + resourceAccessors);
    }

    uint64_t FNV1aHash(const void* data, uint64_t size, uint64_t hash)
    {
        const auto* bytes = static_cast<const uint8_t*>(data);

        for (size_t i = 0ull; i < size; ++i)
        {
            hash ^= bytes[i];
            hash *= 1099511628211ull;
        }

        return hash;
    }
}
