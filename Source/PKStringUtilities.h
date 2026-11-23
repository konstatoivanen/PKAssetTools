#pragma once
#include <string>
#include <vector>

namespace PKAssets::StringUtilities
{
    std::string Trim(const std::string& value);
    std::vector<std::string> Split(const std::string& value, const char* symbols);
    std::vector<std::string> SplitNoWhiteSpace(const std::string& value, const char* symbols);
    std::string ReadFileName(const std::string& filepath);
    std::string ReadDirectory(const std::string& filepath);
    std::string ReadFileRecursiveInclude(const std::string& filepath, std::vector<std::string>& outIncludes);
    std::string ExtractToken(const char* token, std::string& source, bool includeToken, bool trim = false);
    std::string GetLineAtIndex(const std::string& string, size_t index);
    std::string_view ReadIdentifier(const std::string& source, size_t& pos);
    size_t ExtractToken(size_t offset, const char* token, std::string& source, std::string& output, bool includeToken, bool trim = false);
    size_t SkipWhitespaceAndComments(const std::string& source, size_t pos);
    size_t SkipStringLiteral(const std::string& source, size_t pos);
    void ConvertTabsToSpaces(std::string& value);
    bool IsAlphaNumeric(char c);
    bool ExtractDiagnosticLineIndex(const std::string& line, const std::string& moduleName, size_t* outLineIndex);
    bool TryParseInt16(const std::string& str, int16_t* outValue);

    template<size_t size>
    static uint32_t FindStringIndex(const char* const (&arr)[size], const char* str, uint32_t fallback)
    {
        auto length = str ? strnlen(str, 64) : 0u;

        for (auto i = 0u; length && i < size; ++i)
        {
            if (strlen(arr[i]) == length && strncmp(arr[i], str, length) == 0)
            {
                return i;
            }
        }

        return fallback;
    }
};