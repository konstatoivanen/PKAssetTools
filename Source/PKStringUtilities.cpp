#include <fstream>
#include "PKStringUtilities.h"

namespace PKAssets::StringUtilities
{
    std::string Trim(const std::string& value)
    {
        auto first = value.find_first_not_of(" \n\r");

        if (first == std::string::npos)
        {
            return value;
        }

        return value.substr(first, (value.find_last_not_of(" \n\r") - first + 1));
    }

    std::vector<std::string> Split(const std::string& value, const char* symbols)
    {
        std::vector<std::string> output;

        auto start = value.find_first_not_of(symbols, 0);

        while (start != std::string::npos)
        {
            auto end = value.find_first_of(symbols, start);

            if (end == std::string::npos)
            {
                output.push_back(value.substr(start));
                break;
            }

            output.push_back(value.substr(start, end - start));
            start = value.find_first_not_of(symbols, end);
        }

        return output;
    }

    std::vector<std::string> SplitNoWhiteSpace(const std::string& value, const char* symbols)
    {
        std::vector<std::string> output;

        std::string packed;
        packed.reserve(value.size());

        for (auto i = 0u; i < value.size(); ++i)
        {
            if (!isspace(value[i]))
            {
                packed.push_back(value[i]);
            }
        }

        auto start = packed.find_first_not_of(symbols, 0);

        while (start != std::string::npos)
        {
            auto end = packed.find_first_of(symbols, start);

            if (end == std::string::npos)
            {
                output.push_back(packed.substr(start));
                break;
            }

            output.push_back(packed.substr(start, end - start));
            start = packed.find_first_not_of(symbols, end);
        }

        return output;
    }

    std::string ReadFileName(const std::string& filepath)
    {
        auto lastSlash = filepath.find_last_of("/\\");
        lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
        auto lastDot = filepath.rfind('.');
        auto count = lastDot == std::string::npos ? filepath.size() - lastSlash : lastDot - lastSlash;
        return filepath.substr(lastSlash, count);
    }

    std::string ReadDirectory(const std::string& filepath)
    {
        auto lastSlash = filepath.find_last_of("/\\");
        lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
        return filepath.substr(0, lastSlash);
    }

    std::string ReadFileRecursiveInclude(const std::string& filepath, std::vector<std::string>& outIncludes)
    {
        auto includeOnceToken = "#pragma once";
        auto includeToken = "#include ";
        auto includeTokenLength = strlen(includeToken);
        auto foundPragmaOnce = false;

        std::ifstream file(filepath, std::ios::in);

        if (!file)
        {
            return "FAILED TO OPEN: " + filepath;
        }

        std::string result;
        std::string lineBuffer;

        while (std::getline(file, lineBuffer))
        {
            if (!foundPragmaOnce && lineBuffer.find(includeOnceToken) != lineBuffer.npos)
            {
                foundPragmaOnce = true;

                if (std::find(outIncludes.begin(), outIncludes.end(), filepath) != outIncludes.end())
                {
                    file.close();
                    return "";
                }

                outIncludes.push_back(filepath);
                continue;
            }

            auto includepos = lineBuffer.find(includeToken);

            if (includepos != lineBuffer.npos)
            {
                lineBuffer.erase(0, includepos + includeTokenLength);

                auto pos0 = lineBuffer.find_first_of('\"', 0u);
                auto pos1 = pos0 != std::string::npos ? lineBuffer.find_first_of('\"', pos0 + 1u) : std::string::npos;

                if (pos0 == std::string::npos || pos1 == std::string::npos)
                {
                    return "Invalid path specified at include directive: " + lineBuffer;
                }

                lineBuffer = lineBuffer.substr(pos0 + 1u, pos1 - pos0 - 1u);
                lineBuffer.insert(0, filepath.substr(0, filepath.find_last_of("/\\") + 1));
                result += ReadFileRecursiveInclude(lineBuffer, outIncludes);
                continue;
            }

            result += lineBuffer + '\n';
        }

        file.close();

        return result;
    }

    std::string ExtractToken(const char* token, std::string& source, bool includeToken, bool trim)
    {
        std::string firstToken;

        auto pos = source.find(token, 0);

        while (pos != std::string::npos)
        {
            auto eol = source.find_first_of("\r\n", pos);
            auto sol = source.find_first_not_of("\r\n", eol);

            if (eol == std::string::npos ||
                sol == std::string::npos)
            {
                return firstToken;
            }

            if (firstToken.empty())
            {
                if (includeToken)
                {
                    firstToken = source.substr(pos, sol - pos);
                }
                else
                {
                    auto spos = pos + strlen(token);
                    firstToken = source.substr(spos, eol - spos);
                }
            }

            source.erase(pos, sol - pos);
            pos = source.find(token);
        }

        if (trim)
        {
            firstToken = Trim(firstToken);
        }

        return firstToken;
    }

    size_t ExtractToken(size_t offset, const char* token, std::string& source, std::string& output, bool includeToken, bool trim)
    {
        auto pos = source.find(token, offset);

        if (pos == std::string::npos)
        {
            return std::string::npos;
        }

        auto eol = source.find_first_of("\r\n", pos);
        auto sol = source.find_first_not_of("\r\n", eol);

        if (eol == std::string::npos ||
            sol == std::string::npos)
        {
            return std::string::npos;
        }

        if (includeToken)
        {
            output = source.substr(pos, sol - pos);
        }
        else
        {
            auto spos = pos + strlen(token);
            output = source.substr(spos, eol - spos);
        }

        source.erase(pos, sol - pos);

        if (trim)
        {
            output = Trim(output);
        }

        return pos;
    }

    void ExtractTokens(const char* token, std::string& source, std::vector<std::string>& tokens, bool includeToken)
    {
        auto pos = source.find(token, 0);

        while (pos != std::string::npos)
        {
            auto eol = source.find_first_of("\r\n", pos);
            auto sol = source.find_first_not_of("\r\n", eol);

            if (eol == std::string::npos ||
                sol == std::string::npos)
            {
                return;
            }

            if (includeToken)
            {
                tokens.push_back(source.substr(pos, sol - pos));
            }
            else
            {
                auto spos = pos + strlen(token);
                tokens.push_back(source.substr(spos, eol - spos));
            }

            source.erase(pos, sol - pos);
            pos = source.find(token);
        }
    }

    void FindTokens(const char* token, const std::string& source, std::vector<std::string>& tokens, bool includeToken)
    {
        auto pos = source.find(token, 0);

        while (pos != std::string::npos)
        {
            auto eol = source.find_first_of("\r\n", pos);
            auto sol = source.find_first_not_of("\r\n", eol);

            if (eol == std::string::npos ||
                sol == std::string::npos)
            {
                return;
            }

            if (includeToken)
            {
                tokens.push_back(source.substr(pos, sol - pos));
            }
            else
            {
                auto spos = pos + strlen(token);
                tokens.push_back(source.substr(spos, eol - spos));
            }

            pos = source.find(token, sol);
        }
    }

    bool FindScope(const std::string& source, size_t offset, char scopeOpen, char scopeClose, size_t* outStart, size_t* outEnd)
    {
        auto scopeTokens = std::string(1u, scopeOpen) + std::string(1u, scopeClose);
        auto scopepos = source.find(scopeOpen, offset);
        auto indent = 1;

        if (scopepos == std::string::npos)
        {
            return false;
        }

        if (outStart)
        {
            *outStart = scopepos;
        }

        while (true)
        {
            scopepos = source.find_first_of(scopeTokens.c_str(), scopepos + 1u);

            if (scopepos == std::string::npos)
            {
                return false;
            }

            indent += source[scopepos] == scopeOpen ? 1 : -1;

            if (indent == 0)
            {
                if (outEnd)
                {
                    *outEnd = scopepos;
                }

                return true;
            }
        }
    }

    bool FindScope(const std::string& source, size_t offset, const std::string&& scopeOpen, const std::string& scopeClose, size_t* outStart, size_t* outEnd)
    {
        auto scopepos = source.find(scopeOpen, offset);
        auto offsetopen = scopeOpen.size() + 1u;
        auto offsetclose = scopeClose.size() + 1u;
        auto indent = 1;

        if (scopepos == std::string::npos)
        {
            return false;
        }

        if (outStart)
        {
            *outStart = scopepos;
        }

        while (true)
        {
            auto posopen = source.find(scopeOpen, scopepos + offsetopen);
            auto posclose = source.find(scopeClose, scopepos + offsetclose);
            auto isopen = posopen < posclose;
            scopepos = posopen < posclose ? posopen : posclose;

            if (scopepos == std::string::npos)
            {
                return false;
            }

            indent += isopen ? 1 : -1;

            if (indent == 0)
            {
                if (outEnd)
                {
                    *outEnd = scopepos;
                }

                return true;
            }
        }
    }

    size_t FirstIndexOf(const char* str, char c)
    {
        auto ptr = strchr(str, c);
        return ptr == nullptr ? std::string::npos : (ptr - str);
    }

    size_t LastIndexOf(const char* str, char c)
    {
        auto ptr = strrchr(str, c);
        return ptr == nullptr ? std::string::npos : (ptr - str);
    }

    void ReplaceAll(std::string& str, const std::string& surroundMask, const std::string& from, const std::string& to)
    {
        if (from.empty())
        {
            return;
        }

        auto tl = to.length();
        auto fl = from.length();
        size_t pos = 0ull;

        while ((pos = str.find(from, pos)) != std::string::npos)
        {
            auto sl = str.length();

            if ((pos > 0 && surroundMask.find(str[pos - 1]) != std::string::npos) ||
                (pos + fl < sl && surroundMask.find(str[pos + fl]) != std::string::npos))
            {
                pos += fl;
                continue;
            }

            str.replace(pos, fl, to);
            pos += tl;
        }
    }
}