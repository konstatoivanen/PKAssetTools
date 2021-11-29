#pragma once
#include <string>
#include <vector>

namespace PK::Assets::StringUtilities
{
    std::string Trim(const std::string& value);
    std::vector<std::string> Split(const std::string& value, const char* symbols);
    std::string ReadFileName(const std::string& filepath);
    std::string ReadDirectory(const std::string& filepath);
    std::string ReadFileRecursiveInclude(const std::string& filepath);
    std::string ExtractToken(const char* token, std::string& source, bool includeToken);
    size_t ExtractToken(size_t offset, const char* token, std::string& source, std::string& output, bool includeToken);
    void ExtractTokens(const char* token, std::string& source, std::vector<std::string>& tokens, bool includeToken);
    void FindTokens(const char* token, const std::string& source, std::vector<std::string>& tokens, bool includeToken);
    size_t FirstIndexOf(const char* str, char c);
    size_t LastIndexOf(const char* str, char c);
};