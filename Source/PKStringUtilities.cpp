#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include "PKStringUtilities.h"

namespace PKAssets::StringUtilities
{
    bool IsAlphaNumeric(char c)
    {
        return isalnum((unsigned char)c) || c == '_';
    }

    bool IsSwizzleChar(char c)
    {
        return c == 'x' || c == 'y' || c == 'z' || c == 'w' || c == 'r' || c == 'g' || c == 'b' || c == 'a';
    }

    std::string Trim(const std::string& value)
    {
        const auto first = value.find_first_not_of(" \n\r");
        return first == std::string::npos ? value : value.substr(first, (value.find_last_not_of(" \n\r") - first + 1));
    }

    std::vector<std::string> Split(const std::string& value, const char* symbols)
    {
        std::vector<std::string> output;
        auto beg = value.find_first_not_of(symbols, 0);

        while (beg != std::string::npos)
        {
            auto end = value.find_first_of(symbols, beg);

            if (end == std::string::npos)
            {
                output.push_back(value.substr(beg));
                break;
            }

            output.push_back(value.substr(beg, end - beg));
            beg = value.find_first_not_of(symbols, end);
        }

        return output;
    }

    std::vector<std::string> SplitNoWhiteSpace(const std::string& value, const char* symbols)
    {
        std::string packed;
        packed.reserve(value.size());

        for (char c : value)
        {
            if (!isspace((unsigned char)c))
            {
                packed.push_back(c);
            }
        }

        return Split(packed, symbols);
    }

    std::string ReadFileName(const std::string& filepath)
    {
        auto lastSlash = filepath.find_last_of("/\\");
        lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
        const auto lastDot = filepath.rfind('.');
        const auto count = lastDot == std::string::npos ? filepath.size() - lastSlash : lastDot - lastSlash;
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
        constexpr const char* includeOnceToken = "#pragma once";
        constexpr const char* includeToken = "#include ";
        const auto includeTokenLength = strlen(includeToken);
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
            if (!foundPragmaOnce && lineBuffer.find(includeOnceToken) != std::string::npos)
            {
                foundPragmaOnce = true;

                if (std::find(outIncludes.begin(), outIncludes.end(), filepath) != outIncludes.end())
                {
                    return "";
                }

                outIncludes.push_back(filepath);
                continue;
            }

            const auto includepos = lineBuffer.find(includeToken);

            if (includepos != std::string::npos)
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

        return result;
    }

    size_t ExtractToken(size_t offset, const char* token, std::string& source, std::string& output, bool includeToken, bool trim)
    {
        const auto pos = source.find(token, offset);

        if (pos == std::string::npos)
        {
            return std::string::npos;
        }

        const auto eol = source.find_first_of("\r\n", pos);
        const auto sol = source.find_first_not_of("\r\n", eol);

        if (eol == std::string::npos || sol == std::string::npos)
        {
            return std::string::npos;
        }

        const auto spos = includeToken ? pos : pos + strlen(token);
        const auto epos = includeToken ? sol : eol;
        output = source.substr(spos, epos - spos);
        source.erase(pos, sol - pos);

        if (trim)
        {
            output = Trim(output);
        }

        return pos;
    }

    std::string ExtractToken(const char* token, std::string& source, bool includeToken, bool trim)
    {
        std::string firstToken;
        std::string currentToken;
        auto pos = 0ull;

        while ((pos = ExtractToken(pos, token, source, currentToken, includeToken, trim)) != std::string::npos)
        {
            if (firstToken.empty())
            {
                firstToken = currentToken;
            }
        }

        return firstToken;
    }

    void ExtractTokens(const char* token, std::string& source, std::vector<std::string>& tokens, bool includeToken)
    {
        std::string current;
        auto pos = 0ull;

        while ((pos = ExtractToken(pos, token, source, current, includeToken, false)) != std::string::npos)
        {
            tokens.push_back(current);
        }
    }

    void FindTokens(const char* token, const std::string& source, std::vector<std::string>& tokens, bool includeToken)
    {
        auto pos = source.find(token, 0);

        while (pos != std::string::npos)
        {
            const auto eol = source.find_first_of("\r\n", pos);
            const auto sol = source.find_first_not_of("\r\n", eol);
            
            if (eol == std::string::npos || sol == std::string::npos)
            {
                return;
            }

            const auto spos = includeToken ? pos : pos + strlen(token);
            const auto epos = includeToken ? sol : eol;
            tokens.push_back(source.substr(spos, epos - spos));
            
            pos = source.find(token, sol);
        }
    }

    std::string GetLineAtIndex(const std::string& string, size_t index)
    {
        auto pos = 0ull;
        auto lineIndex = 0ull;

        while (pos < string.size())
        {
            auto eol = string.find_first_of("\r\n", pos);
            eol = eol == std::string::npos ? string.size() - 1ull : eol;

            if (lineIndex == index)
            {
                return string.substr(pos, (eol + 1ull) - pos);
            }

            pos = eol + 1ull;
            lineIndex++;
        }

        return {};
    }

    bool FindScope(const std::string& source, size_t offset, char scopeOpen, char scopeClose, size_t* outBeg, size_t* outEnd)
    {
        const char scopeTokens[3] = { scopeOpen, scopeClose, '\0' };
        auto scopepos = source.find(scopeOpen, offset);

        if (scopepos == std::string::npos)
        {
            return false;
        }

        if (outBeg)
        {
            *outBeg = scopepos;
        }

        auto indent = 1;
        
        while ((scopepos = source.find_first_of(scopeTokens, scopepos + 1u)) != std::string::npos)
        {
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

        return false;
    }

    bool FindScope(const std::string& source, size_t offset, const std::string&& scopeOpen, const std::string& scopeClose, size_t* outBeg, size_t* outEnd)
    {
        auto scopepos = source.find(scopeOpen, offset);

        if (scopepos == std::string::npos)
        {
            return false;
        }

        if (outBeg)
        {
            *outBeg = scopepos;
        }

        const auto offsetopen = scopeOpen.size() + 1u;
        const auto offsetclose = scopeClose.size() + 1u;
        auto indent = 1;

        while (true)
        {
            const auto posopen = source.find(scopeOpen, scopepos + offsetopen);
            const auto posclose = source.find(scopeClose, scopepos + offsetclose);
            scopepos = std::min(posopen, posclose);

            if (scopepos == std::string::npos)
            {
                return false;
            }

            indent += (posopen < posclose) ? 1 : -1;
            
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
        const auto ptr = strchr(str, c);
        return ptr == nullptr ? std::string::npos : (ptr - str);
    }

    size_t LastIndexOf(const char* str, char c)
    {
        const auto ptr = strrchr(str, c);
        return ptr == nullptr ? std::string::npos : (ptr - str);
    }

    void ConvertTabsToSpaces(std::string& value)
    {
        for (char& c : value)
        {
            if (c == '\f' || c == '\t' || c == '\v')
            {
                c = ' ';
            }
        }
    }

    void ReplaceAll(std::string& str, const std::string& surroundMask, const std::string& from, const std::string& to)
    {
        if (from.empty())
        {
            return;
        }

        auto pos = 0ull;

        while ((pos = str.find(from, pos)) != std::string::npos)
        {
            if ((pos > 0 && surroundMask.find(str[pos - 1]) != std::string::npos) ||
                (pos + from.length() < str.length() && surroundMask.find(str[pos + from.length()]) != std::string::npos))
            {
                pos += from.length();
                continue;
            }

            str.replace(pos, from.length(), to);
            pos += to.length();
        }
    }

    void ClearTextRange(std::string& str, size_t beg, size_t end)
    {
        for (auto i = beg; i < end && i < str.size(); ++i)
        {
            if (str[i] != '\n' && str[i] != '\r')
            {
                str[i] = ' ';
            }
        }
    }


    void RemoveCommentsAndPadScalarSwizzles(std::string& source)
    {
        auto i = 0ull;

        while (i + 1ull < source.size())
        {
            if (source[i] == '/' && source[i + 1ull] == '/')
            {
                const auto eol = source.find('\n', i + 2ull);

                if (eol == std::string::npos)
                {
                    source.erase(i);
                    break;
                }

                source.erase(i, eol - i);
                continue;
            }

            if (source[i] == '/' && source[i + 1ull] == '*')
            {
                const auto blockEnd = source.find("*/", i + 2ull);
                if (blockEnd == std::string::npos)
                {
                    source.erase(i);
                    break;
                }

                ClearTextRange(source, i, blockEnd + 2ull);
                i = blockEnd + 2ull;
                continue;
            }

            if (isdigit((uint8_t)source[i]) && (i == 0ull || !IsAlphaNumeric(source[i - 1ull])))
            {
                auto k = i;

                while (k < source.size() && isdigit(source[k]))
                {
                    k++;
                }
                
                if (k < source.size() && source[k] == '.')
                {
                    k++;
                    
                    while (k < source.size() && isdigit((uint8_t)source[k]))
                    {
                        k++;
                    }

                    if (k < source.size() && (source[k] == 'e' || source[k] == 'E'))
                    {
                        k++;
                        
                        if (k < source.size() && (source[k] == '+' || source[k] == '-')) 
                        {
                            k++;
                        }

                        while (k < source.size() && isdigit((uint8_t)source[k])) 
                        { 
                            k++; 
                        }
                    }

                    if (k < source.size() && (source[k] == 'f' || source[k] == 'F' || source[k] == 'h' || source[k] == 'H'))
                    {
                        k++;
                    }

                    if (k + 1ull < source.size() && source[k] == '.' && IsSwizzleChar(source[k + 1ull]))
                    {
                        source.insert(k, " ");
                        i = k + 2ull;
                        continue;
                    }
                }
            }

            ++i;
        }
    }

    bool InjectFunctionAttribute(std::string& source, const std::string& functionName, const std::string& attribute)
    {
        auto pos = 0ull;

        while ((pos = source.find(functionName, pos)) != std::string::npos)
        {
            const auto nameEnd = pos + functionName.size();

            if ((pos && IsAlphaNumeric(source[pos - 1ull])) || (nameEnd < source.size() && IsAlphaNumeric(source[nameEnd])))
            {
                pos = nameEnd;
                continue;
            }

            const auto openArgs = source.find_first_not_of(" \t\r\n", nameEnd);
            
            if (openArgs == std::string::npos || source[openArgs] != '(')
            {
                pos = nameEnd;
                continue;
            }

            auto closeArgs = openArgs + 1ull;
            auto depth = 1;

            while (closeArgs < source.size() && depth > 0)
            {
                if (source[closeArgs] == '(')
                {
                    depth++;
                }
                else if (source[closeArgs] == ')')
                { 
                    depth--; 
                }

                closeArgs++;
            }

            if (depth)
            {
                return false;
            }

            auto bodyPos = source.find_first_not_of(" \t\r\n", closeArgs);

            if (bodyPos != std::string::npos && source[bodyPos] == ':')
            {
                bodyPos = source.find_first_not_of(" \t\r\n", bodyPos + 1ull);

                while (bodyPos < source.size() && IsAlphaNumeric(source[bodyPos]))
                {
                    bodyPos++;
                }

                bodyPos = source.find_first_not_of(" \t\r\n", bodyPos);
            }

            if (bodyPos == std::string::npos || source[bodyPos] != '{')
            {
                pos = closeArgs;
                continue;
            }

            if (!pos)
            {
                return false;
            }

            auto returnEnd = pos;
            
            while (returnEnd && isspace((uint8_t)source[returnEnd - 1ull]))
            {
                returnEnd--;
            }

            auto returnBeg = returnEnd;

            while (returnBeg && (IsAlphaNumeric(source[returnBeg - 1ull]) || source[returnBeg - 1ull] == '<' || source[returnBeg - 1ull] == '>'))
            {
                returnBeg--;
            }

            if (returnBeg == returnEnd)
            {
                pos = closeArgs;
                continue;
            }

            source.insert(returnBeg, attribute);
            return true;
        }

        return false;
    }

    bool ParseUniform(const std::string& source, size_t uniformBodyOffset, std::string& outType, std::string& outName, uint32_t* outCount, size_t* outEnd)
    {
        const auto typeBeg = source.find_first_not_of(" \t", uniformBodyOffset);

        if (typeBeg == std::string::npos)
        {
            return false;
        }

        auto pos = typeBeg;
        auto depth = 0;

        while (pos < source.size())
        {
            const auto c = source[pos];

            if (c == '<')
            {
                depth++;
            }
            else if (c == '>')
            {
                depth = std::max(0, depth - 1);
            }
            else if (((c == ' ' || c == '\t') && !depth) || c == ';' || c == '\n' || c == '\r')
            {
                break;
            }

            pos++;
        }

        if (pos == typeBeg)
        {
            return false;
        }

        const auto nameBeg = source.find_first_not_of(" \t", pos);
        auto nameEnd = nameBeg;

        while (nameEnd < source.size() && IsAlphaNumeric(source[nameEnd]))
        {
            nameEnd++;
        }

        if (nameEnd == nameBeg || nameEnd >= source.size())
        {
            return false;
        }

        auto arraySize = 1u;
        auto semicolon = source.find_first_not_of(" \t", nameEnd);

        if (semicolon == std::string::npos)
        {
            return false;
        }

        if (source[semicolon] == '[')
        {
            const auto arrayClose = source.find(']', semicolon + 1ull);

            if (arrayClose == std::string::npos)
            {
                return false;
            }

            const auto count = Trim(source.substr(semicolon + 1ull, arrayClose - (semicolon + 1ull)));

            if (count.empty() || !isdigit((uint8_t)count[0]))
            {
                return false;
            }

            arraySize = (uint32_t)std::stoul(count);
            semicolon = source.find_first_not_of(" \t", arrayClose + 1ull);
        }

        if (semicolon == std::string::npos || source[semicolon] != ';')
        {
            return false;
        }

        outType = source.substr(typeBeg, pos - typeBeg);
        outName = source.substr(nameBeg, nameEnd - nameBeg);

        if (outCount)
        {
            *outCount = arraySize;
        }

        if (outEnd)
        {
            *outEnd = semicolon + 1ull;
        }

        return true;
    }

    bool ExtractDiagnosticLineIndex(const std::string& line, const std::string& moduleName, size_t* outLineIndex)
    {
        for (char sep : { '(', ':' })
        {
            const auto token = moduleName + sep;
            const auto pos = line.find(token);

            if (pos != std::string::npos)
            {
                const auto indexBeg = pos + token.size();

                if (indexBeg < line.size() && isdigit((uint8_t)line[indexBeg]))
                {
                    *outLineIndex = (size_t)std::stoull(line.c_str() + indexBeg);
                    return true;
                }
            }
        }

        return false;
    }
}
