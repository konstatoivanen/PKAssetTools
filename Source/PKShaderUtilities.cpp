#include <algorithm>
#include <sstream>
#include <format>
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

    struct ParsedUniform
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


    static uint32_t AlignUp(uint32_t value, uint32_t alignment)
    {
        return (value + alignment - 1u) & ~(alignment - 1u);
    }

    template<typename... TArgs>
    static int Fail(std::format_string<TArgs...> fmt, TArgs&&... args)
    {
        printf("Error: %s\n", std::format(fmt, std::forward<TArgs>(args)...).c_str());
        return -1;
    }

    #define RETURN_ERROR_IF(condition, ...) if (condition) return Fail(__VA_ARGS__)

    template<typename TContainer>
    static auto* FindByName(TContainer& items, std::string_view name)
    {
        for (auto& item : items)
        {
            if (item.name == name)
            {
                return &item;
            }
        }

        return decltype(items.data()){};
    }

    static ShaderPass& FindOrAddPass(ShaderPasses& passes, const std::string& name)
    {
        if (auto* pass = FindByName(passes, name))
        {
            return *pass;
        }

        return passes.emplace_back(ShaderPass{ name, 0u });
    }

    template<typename TFunc>
    static void ForEachMatchingPass(ShaderPasses& passes, std::string_view passName, TFunc&& apply)
    {
        for (auto& pass : passes)
        {
            if (passName == PK_SHADER_ATTRIB_PASS_ANY || passName == pass.name)
            {
                apply(pass);
            }
        }
    }

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

            const auto parts = StringUtilities::Split(output, " \n\r");

            RETURN_ERROR_IF(parts.empty(), "Pragma '{}' is missing pass and arguments!", token);
            RETURN_ERROR_IF(requirePasses && parts[0] != PK_SHADER_ATTRIB_PASS_ANY && !FindByName(passes, parts[0]), "Pragma '{}' references undeclared pass '{}'!", token, parts[0]);

            PragmaDirective directive{};
            directive.pass = parts[0];
            directive.args.assign(parts.begin() + 1, parts.end());
            outDirectives.push_back(std::move(directive));
        }

        std::stable_sort(outDirectives.begin(), outDirectives.end(), [](const PragmaDirective& a, const PragmaDirective& b)
        {
            return a.pass == PK_SHADER_ATTRIB_PASS_ANY && b.pass != PK_SHADER_ATTRIB_PASS_ANY;
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
                Fail("Invalid argument count for '{}' in pass '{}'!", token, directive.pass);
                return false;
            }

            ForEachMatchingPass(passes, directive.pass, [&](ShaderPass& pass)
            {
                apply(pass.attributes, directive.args);
            });
        }

        return true;
    }

    static int AddPassEntryPoint(ShaderPass& pass, const EntryPointInfo& info, bool checkPassType)
    {
        const auto stageFlag = 1u << (uint32_t)info.stage;
        const auto entryType = GetShaderPassType(info.stage);
        
        RETURN_ERROR_IF(checkPassType && !pass.entryPoints.empty() && GetShaderPassType(pass.entryPoints[0].stage) != entryType, "Pass '{}' mixes incompatible shader stage types!", pass.name);
        RETURN_ERROR_IF(pass.stageMask & stageFlag, "Pass '{}' already declares an entry point for '{}'!", pass.name, PKShaderStage_NAMES[(uint32_t)info.stage]);

        pass.stageMask |= stageFlag;
        pass.entryPoints.push_back(info);
        return 0;
    }

    static int ParseMulticompileDirective(const PragmaDirective& pragma, ShaderDirective& outDirective)
    {
        RETURN_ERROR_IF(pragma.args.size() < 3u, "Multicompile directive in pass '{}' requires a directive name, pass name and at least two integer values!", pragma.pass);

        outDirective.name = pragma.args[0];
        outDirective.values.clear();
        outDirective.values.reserve(pragma.args.size() - 1ull);

        for (auto i = 1ull; i < pragma.args.size(); ++i)
        {
            auto value = (int16_t)0;

            RETURN_ERROR_IF(!StringUtilities::TryParseInt16(pragma.args[i], &value), "Multicompile directive '{}' in pass '{}' has non-integer value '{}'!", outDirective.name, pragma.pass, pragma.args[i]);

            if (std::find(outDirective.values.begin(), outDirective.values.end(), value) == outDirective.values.end())
            {
                outDirective.values.push_back(value);
            }
        }

        RETURN_ERROR_IF(outDirective.values.size() < 2u, "Multicompile directive '{}' in pass '{}' requires at least two unique integer values!", outDirective.name, pragma.pass);
        RETURN_ERROR_IF(outDirective.values.size() > PK_ASSET_MAX_SHADER_DIRECTIVE_SIZE, "Multicompile directive '{}' exceeds maximum value count limit '{}' in pass '{}'!", outDirective.name, PK_ASSET_MAX_SHADER_DIRECTIVE_SIZE, pragma.pass);
        return 0;
    }

    static bool FindStructDefinition(const std::string& source, const std::string& structName, size_t* outOpen, size_t* outClose)
    {
        constexpr auto whitespace = " \t\r\n";
        auto pos = 0ull;

        while ((pos = source.find(structName, pos)) != std::string::npos)
        {
            const auto nameBeg = pos;
            const auto nameEnd = pos + structName.size();
            pos = nameEnd;

            if (nameBeg < 7ull || StringUtilities::IsAlphaNumeric(source[nameBeg - 1ull]))
            {
                continue;
            }

            if (nameEnd < source.size() && StringUtilities::IsAlphaNumeric(source[nameEnd]))
            {
                continue;
            }

            const auto keywordEnd = source.find_last_not_of(whitespace, nameBeg - 1ull);

            if (keywordEnd == std::string::npos || keywordEnd < 5ull || source.compare(keywordEnd - 5ull, 6ull, "struct") != 0)
            {
                continue;
            }

            if (keywordEnd > 5ull && StringUtilities::IsAlphaNumeric(source[keywordEnd - 6ull]))
            {
                continue;
            }

            const auto openBrace = source.find_first_not_of(whitespace, nameEnd);

            if (openBrace == std::string::npos || source[openBrace] != '{')
            {
                continue;
            }

            const auto closeBrace = source.find('}', openBrace + 1ull);

            if (closeBrace == std::string::npos)
            {
                return false;
            }

            *outOpen = openBrace;
            *outClose = closeBrace;
            return true;
        }

        return false;
    }

    static PKUniform ReflectUniformType(std::string_view typeText, std::string* outFieldType, std::string* outTargetType)
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

    static bool IsUnqualifiedIdentifier(const simplecpp::Token* token)
    {
        return token && token->name && (!token->previous || token->previous->op != '.');
    }

    template<typename TToken>
    static TToken* FindTokenScope(TToken* start, const simplecpp::Token* end, char openOp, char closeOp)
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
            else if (token->op == closeOp && --depth == 0)
            {
                return token;
            }
        }

        return nullptr;
    }

    static simplecpp::Token* SkipLeadingAttributes(simplecpp::Token* start, const simplecpp::Token* end)
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

    template<typename TToken>
    static TToken* FindTemplateArgumentScope(TToken* start)
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
                if ((token->op == '&' || token->op == '|') && token->next && token->next->op == token->op)
                {
                    return nullptr;
                }

                if ((token->op == '<' || token->op == '>' || token->op == '=' || token->op == '!') && token->next && token->next->op == '=')
                {
                    return nullptr;
                }

                if (token->op == '<')
                {
                    angleDepth++;
                }
                else if (token->op == '>' && --angleDepth == 0)
                {
                    return token;
                }
            }
        }

        return nullptr;
    }

    static const char* ParseUniform(simplecpp::Token* typeBeg, simplecpp::Token* limit, ParsedUniform& outUniform)
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

        outUniform.type = ReflectUniformType(typeText, &outUniform.fieldType, &outUniform.targetType);

        if (outUniform.type == PKUniform::Invalid)
        {
            return "Unsupported field type";
        }

        outUniform.name = nameToken->str();
        outUniform.count = count;
        outUniform.endToken = endToken;
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

        const auto* closeParen = FindTokenScope(openParen, nullptr, '(', ')');

        if (!closeParen || openParen->next == closeParen || (openParen->next->next == closeParen && openParen->next->str() == "void"))
        {
            return 0u;
        }

        auto maxArgs = 1u;
        auto minArgs = 0u;
        auto hasDefault = false;
        auto depth = 0;

        for (const auto* token = openParen->next; token && token != closeParen; token = token->next)
        {
            if (depth == 0 && token->op == '<' && token->previous && token->previous->name)
            {
                if (const auto* closeAngle = FindTemplateArgumentScope(token))
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
            return std::format("{}#{}", token->str(), GetFunctionArityRange(openParen));
        }

        return token->str();
    }

    static uint32_t CountIdentifierInRange(const simplecpp::Token* first, const simplecpp::Token* last, const std::string& key)
    {
        auto count = 0u;

        for (const auto* token = first; token; token = token->next)
        {
            if (IsUnqualifiedIdentifier(token) && GetTokenUsageKey(token) == key)
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
        TokenStatement current{};
        auto hasEquals = false;

        auto commitStatement = [&](simplecpp::Token* endToken, bool isFunction)
        {
            current.end = endToken;
            current.isFunction = isFunction;
            current.functionName = isFunction ? current.functionName : nullptr;
            statements.push_back(current);
            current = {};
            hasEquals = false;
        };

        for (auto* token = tokens.front(); token; token = token->next)
        {
            if (!current.beg)
            {
                current.beg = token;
            }

            if (token->op == '[')
            {
                token = FindTokenScope(token, nullptr, '[', ']');

                if (!token)
                {
                    break;
                }
            }
            else if (token->op == '<' && !current.openBrace && !hasEquals)
            {
                auto* closeAngle = FindTemplateArgumentScope(token);

                if (closeAngle && closeAngle->next && closeAngle->next->op == '(' && token->previous && token->previous->name)
                {
                    current.functionName = token->previous;
                    token = closeAngle;
                }
            }
            else if (token->op == '(')
            {
                if (!current.openBrace && !hasEquals && token->previous && token->previous->name)
                {
                    current.functionName = token->previous;
                }

                token = FindTokenScope(token, nullptr, '(', ')');

                if (!token)
                {
                    break;
                }
            }
            else if (token->op == '=')
            {
                hasEquals = true;
                current.functionName = nullptr;
            }
            else if (token->op == '{')
            {
                current.openBrace = token;
                current.closeBrace = FindTokenScope(token, nullptr, '{', '}');

                if (!current.closeBrace)
                {
                    break;
                }

                token = current.closeBrace;
                const auto isFunction = current.functionName != nullptr;
                const auto* firstToken = SkipLeadingAttributes(current.beg, current.openBrace);
                const auto isBlock = firstToken && firstToken->name && (firstToken->str() == "property" || firstToken->str() == "namespace" || firstToken->str() == "extension");

                if (isFunction || isBlock)
                {
                    if (token->next && token->next->op == ';')
                    {
                        token = token->next;
                    }

                    commitStatement(token, isFunction);
                }
            }
            else if (token->op == ';')
            {
                const auto isPrototype = !current.openBrace && !hasEquals && current.functionName != nullptr;
                commitStatement(token, isPrototype);
            }
        }

        return statements;
    }

    static simplecpp::Token* FindDeclarationName(const TokenStatement& statement)
    {
        if (statement.isFunction)
        {
            return statement.functionName;
        }

        if (statement.openBrace)
        {
            const auto* firstToken = SkipLeadingAttributes(statement.beg, statement.openBrace);

            if (firstToken && firstToken->name && firstToken->str() == "property" && statement.openBrace->previous && statement.openBrace->previous->name)
            {
                return statement.openBrace->previous;
            }

            return nullptr;
        }

        if (!statement.end)
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
            if (IsUnqualifiedIdentifier(token))
            {
                counts[GetTokenUsageKey(token)]++;
            }
        }

        // Subtract declaration tokens so the map only stores actual reference counts.
        for (const auto& statement : statements)
        {
            if (const auto* declToken = FindDeclarationName(statement))
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
            output.append(line);
            output.push_back('\n');

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

        ShaderPass sharedPass{ PK_SHADER_ATTRIB_PASS_ANY, 0u };

        for (const auto& directive : directives)
        {
            RETURN_ERROR_IF(directive.args.size() != 2u, "Entry point declaration for pass '{}' contains an invalid number of arguments (should be pass, stage, name)", directive.pass);

            const auto stage = StringToPKShaderStage(directive.args[0].c_str());

            RETURN_ERROR_IF(stage == PKShaderStage::EnumCount, "Unsupported shader stage '{}' specified!", directive.args[0]);
            RETURN_ERROR_IF(source.find(directive.args[1], 0ull) == std::string::npos, "Definition for declared entry point '{}' was not found!", directive.args[1]);

            EntryPointInfo info{};
            info.name = directive.args[1];
            info.stage = stage;

            const auto isAny = directive.pass == PK_SHADER_ATTRIB_PASS_ANY;
            auto& targetPass = isAny ? sharedPass : FindOrAddPass(passes, directive.pass);

            if (AddPassEntryPoint(targetPass, info, !isAny) != 0)
            {
                return -1;
            }
        }

        if (passes.empty())
        {
            RETURN_ERROR_IF(sharedPass.entryPoints.empty(), "Shader does not declare any entry points!");

            const auto passType = GetShaderPassType(sharedPass.entryPoints[0].stage);

            for (auto i = 1ull; i < sharedPass.entryPoints.size(); ++i)
            {
                RETURN_ERROR_IF(GetShaderPassType(sharedPass.entryPoints[i].stage) != passType, "Pass '{}' mixes incompatible shader stage types!", PK_SHADER_ATTRIB_PASS_ANY);
            }

            passes.push_back(std::move(sharedPass));
        }
        else if (!sharedPass.entryPoints.empty())
        {
            for (auto& pass : passes)
            {
                const auto passType = GetShaderPassType(pass.entryPoints[0].stage);

                for (const auto& info : sharedPass.entryPoints)
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

        RETURN_ERROR_IF(passes.size() > PK_ASSET_MAX_SHADER_PASSES, "Maximum number of shader passes exceeded ({} / {})!", passes.size(), PK_ASSET_MAX_SHADER_PASSES);
        
        return 0;
    }

    int ExtractMulticompiles(std::string& source, ShaderPasses& passes)
    {
        constexpr auto maxDirectivesPerPass = PK_ASSET_MAX_SHADER_DIRECTIVES;
        constexpr auto maxVariantsPerPass = PK_ASSET_MAX_SHADER_VARIANTS;
        constexpr auto maxTotalDirectives = 0xFFu;
        constexpr auto maxTotalDirectiveValues = 0xFFFFu;
        constexpr auto maxTotalVariants = 0x1000u;

        std::vector<PragmaDirective> pragmaDirectives;

        if (ExtractPragmaDirectives(PK_SHADER_ATTRIB_MULTI_COMPILE, source, passes, pragmaDirectives, true) != 0)
        {
            return -1;
        }

        for (auto& pass : passes)
        {
            pass.directives.clear();
            pass.variantDefines.clear();
        }

        for (const auto& pragma : pragmaDirectives)
        {
            ShaderDirective parsed{};

            if (ParseMulticompileDirective(pragma, parsed) != 0)
            {
                return -1;
            }

            ForEachMatchingPass(passes, pragma.pass, [&](ShaderPass& pass)
            {
                if (auto* existing = FindByName(pass.directives, parsed.name))
                {
                    existing->values = parsed.values;
                }
                else
                {
                    pass.directives.push_back(parsed);
                }
            });
        }

        auto totalDirectives = 0ull;
        auto totalDirectiveValues = 0ull;
        auto totalVariants = 0ull;

        for (auto& pass : passes)
        {
            RETURN_ERROR_IF(pass.directives.size() > maxDirectivesPerPass, "Pass multicompile directive limit '{}' exceeded in pass '{}'!", maxDirectivesPerPass, pass.name);

            totalDirectives += pass.directives.size();

            RETURN_ERROR_IF(totalDirectives > maxTotalDirectives, "Total multicompile directive limit '{}' exceeded!", maxTotalDirectives);

            auto variantCount = 1ull;

            for (auto& dir : pass.directives)
            {
                totalDirectiveValues += dir.values.size();

                RETURN_ERROR_IF(totalDirectiveValues > maxTotalDirectiveValues, "Total shader directive value count limit '{}' exceeded!", maxTotalDirectiveValues);
                RETURN_ERROR_IF(variantCount * dir.values.size() > maxVariantsPerPass, "Pass variant count limit '{}' exceeded in pass '{}'!", maxVariantsPerPass, pass.name);

                dir.stride = (uint8_t)variantCount;
                variantCount *= dir.values.size();
            }

            totalVariants += variantCount;

            RETURN_ERROR_IF(totalVariants > maxTotalVariants, "Total shader variant count limit '{}' exceeded!", maxTotalVariants);

            pass.variantDefines.reserve(variantCount);
            const auto passDefine = pass.name != PK_SHADER_ATTRIB_PASS_ANY ? std::format("#define {}\n", pass.name) : std::string();

            for (auto i = 0u; i < variantCount; ++i)
            {
                auto defines = passDefine;
                auto variantIndex = i;

                for (const auto& dir : pass.directives)
                {
                    const auto valueCount = (uint32_t)dir.values.size();
                    defines += std::format("#define {} {}\n", dir.name, dir.values[variantIndex % valueCount]);
                    variantIndex /= valueCount;
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
        outStructs.clear();

        std::vector<std::string> structNames;
        std::string output;
        auto pos = 0ull;

        while (true)
        {
            pos = StringUtilities::ExtractToken(pos, PK_SHADER_ATTRIB_EXPORT_STRUCT, source, output, false);

            if (pos == std::string::npos)
            {
                break;
            }

            const auto parts = StringUtilities::Split(output, " \n\r");

            RETURN_ERROR_IF(parts.size() != 1u, "'{}' expects a single struct name argument!", PK_SHADER_ATTRIB_EXPORT_STRUCT);

            if (std::find(structNames.begin(), structNames.end(), parts[0]) == structNames.end())
            {
                structNames.push_back(parts[0]);
            }
        }

        for (const auto& structName : structNames)
        {
            auto openBrace = 0ull;
            auto closeBrace = 0ull;

            RETURN_ERROR_IF(!FindStructDefinition(source, structName, &openBrace, &closeBrace), "Definition for exported struct '{}' was not found!", structName);

            const auto bodySource = source.substr(openBrace + 1ull, closeBrace - openBrace - 1ull);
            std::istringstream stream(bodySource);
            std::vector<std::string> filenames;
            simplecpp::OutputList outputList;
            simplecpp::TokenList bodyTokens(stream, filenames, structName, {}, &outputList);
            bodyTokens.removeComments();

            std::vector<ParsedUniform> fields;

            for (auto* tokenBeg = bodyTokens.front(); tokenBeg;)
            {
                auto& uniform = fields.emplace_back();
                const auto* error = ParseUniform(tokenBeg, nullptr, uniform);

                RETURN_ERROR_IF(error, "{} near '{}' in exported struct '{}'!", error, tokenBeg->str(), structName);
                RETURN_ERROR_IF(PKUniformIsResourceHandle(uniform.type) && uniform.count > 1u, "Resource handle arrays are not supported in exported struct '{}' ('{}')!", structName, uniform.name);

                tokenBeg = uniform.endToken->next;
            }

            RETURN_ERROR_IF(fields.empty(), "Exported struct '{}' has no fields!", structName);

            std::stable_sort(fields.begin(), fields.end(), [](const ParsedUniform& a, const ParsedUniform& b)
            {
                const auto alignA = PKUniformToScalarSize(a.type);
                const auto alignB = PKUniformToScalarSize(b.type);

                if (alignA != alignB)
                {
                    return alignA > alignB;
                }

                return PKUniformToSize(a.type) * a.count > PKUniformToSize(b.type) * b.count;
            });

            auto& outStruct = outStructs.emplace_back();
            outStruct.name = structName;
            outStruct.fields.reserve(fields.size());

            auto stride = 0u;
            auto maxAlign = 4u;

            for (const auto& field : fields)
            {
                const auto align = PKUniformToScalarSize(field.type);
                maxAlign = std::max(maxAlign, align);
                stride = AlignUp(stride, align);

                auto& layoutField = outStruct.fields.emplace_back();
                WriteName(layoutField.name, field.name.c_str());
                layoutField.type = field.type;
                layoutField.count = field.count;
                layoutField.offset = stride;

                stride += PKUniformToSize(field.type) * field.count;
            }

            const auto alignedStride = AlignUp(stride, maxAlign);
            const auto padding = alignedStride - stride;
            outStruct.size = alignedStride;

            std::string structFields;
            std::string accessors;

            for (const auto& field : fields)
            {
                if (PKUniformIsResourceHandle(field.type))
                {
                    const auto* macro = field.type == PKUniform::AccelerationStructure ? "PK_STRUCT_TLAS" : "PK_STRUCT_HANDLE";
                    structFields += std::format("{} {}_Handle;\n", field.fieldType, field.name);
                    accessors += std::format("{}({}, {})\n", macro, field.targetType, field.name);
                    continue;
                }

                const auto arraySuffix = field.count > 1u ? std::format("[{}]", field.count) : "";
                structFields += std::format("{} {}{};\n", field.fieldType, field.name, arraySuffix);
            }

            if (padding > 0u)
            {
                structFields += std::format("uint8_t _pk_pad[{}];\n", padding);
            }

            source.replace(openBrace, closeBrace - openBrace + 1ull, std::format("{{\n{}{}}}", structFields, accessors));
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
                const auto& filename = output.location.fileIndex < filenames.size() ? filenames[output.location.fileIndex] : name;
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
                if (const auto* entry = FindByName(activeEntries, statement.functionName->str()))
                {
                    statement.beg->setstr(std::format("{}{}", GetSlangStageAttribute(entry->stage), statement.beg->str()));
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
                    const auto maxArgs = GetFunctionArityRange(FindFunctionOpenParen(statement.functionName), &minArgs);

                    auto totalRefs = 0u;
                    auto selfRefs = 0u;

                    for (auto arity = minArgs; arity <= maxArgs; ++arity)
                    {
                        const auto functionKey = std::format("{}#{}", functionName, arity);
                        totalRefs += GetIdentifierUsageCount(usageMap, functionKey);
                        selfRefs += CountIdentifierInRange(statement.openBrace, statement.end, functionKey);
                    }

                    if (!FindByName(activeEntries, functionName) && totalRefs <= selfRefs)
                    {
                        EraseTokenRange(tokens, statement.beg, statement.end);
                        changed = true;
                    }

                    continue;
                }

                if (const auto* declToken = FindDeclarationName(statement))
                {
                    if (GetIdentifierUsageCount(usageMap, declToken->str()) == 0u)
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

            ParsedUniform uniform{};

            if (ParseUniform(token->next, statement.end, uniform) != nullptr || uniform.endToken != statement.end)
            {
                continue;
            }

            if (const auto* existing = FindByName(outConstants, uniform.name))
            {
                if (existing->targetType != uniform.targetType || existing->count != uniform.count)
                {
                    printf("Warning constant parameter '%s' was declared again with a different format '%s'\n", existing->name.c_str(), uniform.targetType.c_str());
                }
            }
            else
            {
                SourcePushConstant constant{};
                constant.name = std::move(uniform.name);
                constant.fieldType = std::move(uniform.fieldType);
                constant.targetType = std::move(uniform.targetType);
                constant.type = uniform.type;
                constant.count = uniform.count;
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

        for (auto& member : constants)
        {
            const auto elementSize = PKUniformToSize(member.type);
            const auto totalSize = elementSize * member.count;
            const auto isPackedHandle = PKUniformIsResourceHandle(member.type) && elementSize < 4u;

            member.memberIndex = 0u;
            member.bitOffset = 0u;
            member.bitSize = 0u;

            if (isPackedHandle)
            {
                member.memberAlignment = 4u;
                member.memberSize = (uint16_t)AlignUp(totalSize, 4u);

                if (member.count == 1u)
                {
                    member.bitSize = (uint8_t)(elementSize * 8u);
                }
            }
            else
            {
                member.memberAlignment = (uint8_t)PKUniformToScalarSize(member.type);
                member.memberSize = (uint16_t)totalSize;
            }
        }

        std::stable_sort(constants.begin(), constants.end(), [](const SourcePushConstant& a, const SourcePushConstant& b)
        {
            const auto isSharedA = a.bitSize > 0u;
            const auto isSharedB = b.bitSize > 0u;

            if (isSharedA != isSharedB)
            {
                return !isSharedA;
            }

            if (isSharedA)
            {
                return a.bitSize > b.bitSize;
            }

            if (a.memberAlignment != b.memberAlignment)
            {
                return a.memberAlignment > b.memberAlignment;
            }

            return a.memberSize > b.memberSize;
        });

        auto dedicatedMemberCount = 0u;
        auto packedByteOffset = 0u;

        for (auto& member : constants)
        {
            if (member.bitSize > 0u)
            {
                const auto elementSize = member.bitSize / 8u;
                packedByteOffset = AlignUp(packedByteOffset, elementSize);

                member.memberIndex = (uint16_t)(dedicatedMemberCount + packedByteOffset / 4u);
                member.bitOffset = (uint8_t)((packedByteOffset % 4u) * 8u);
                packedByteOffset += elementSize;
            }
            else
            {
                member.memberIndex = (uint16_t)(dedicatedMemberCount++);
            }
        }

        const auto packedWordCount = (packedByteOffset + 3u) / 4u;

        std::string blockFields;
        std::string accessors;

        for (const auto& member : constants)
        {
            if (member.bitSize > 0u)
            {
                const auto wordIndex = member.memberIndex - dedicatedMemberCount;
                accessors += std::format("PK_PC_HANDLE({}, {}, {}, {}, {})\n", member.targetType, member.name, wordIndex, member.bitOffset, member.bitSize);
                continue;
            }

            const auto arraySuffix = member.count > 1u ? std::format("[{}]", member.count) : "";

            if (!PKUniformIsResourceHandle(member.type))
            {
                blockFields += std::format("{} {}{};\n", member.fieldType, member.name, arraySuffix);
                continue;
            }

            if (member.type == PKUniform::AccelerationStructure)
            {
                const auto* macro = member.count > 1u ? "PK_PC_TLAS_ARRAY" : "PK_PC_TLAS";
                blockFields += std::format("{} {}_Handle{};\n", member.fieldType, member.name, arraySuffix);
                accessors += std::format("{}({}, {})\n", macro, member.targetType, member.name);
                continue;
            }

            const auto wordCount = member.memberSize / 4u;
            const auto elementBits = PKUniformToSize(member.type) * 8u;
            blockFields += std::format("uint {}_Handle[{}];\n", member.name, wordCount);
            accessors += std::format("PK_PC_HANDLE_ARRAY({}, {}, {}u)\n", member.targetType, member.name, elementBits);
        }

        for (auto wordIndex = 0u; wordIndex < packedWordCount; ++wordIndex)
        {
            blockFields += std::format("uint pk_packed_pc_{};\n", wordIndex);
        }

        source.insert(0ull, std::format("[[vk::push_constant]] cbuffer pk_global_push_constant_block\n{{\n{}}};\n{}", blockFields, accessors));
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
