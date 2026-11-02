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
    size_t ExtractToken(size_t offset, const char* token, std::string& source, std::string& output, bool includeToken, bool trim = false);
    void ConvertTabsToSpaces(std::string& value);
    bool ExtractDiagnosticLineIndex(const std::string& line, const std::string& moduleName, size_t* outLineIndex);
};