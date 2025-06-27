#pragma once
#include <string>
#include <vector>

namespace PKAssets::StringUtilities
{
    std::string Trim(const std::string& value);
    std::vector<std::string> Split(const std::string& value, const char* symbols);
    std::string ReadFileName(const std::string& filepath);
    std::string ReadDirectory(const std::string& filepath);
    std::string ReadFileRecursiveInclude(const std::string& filepath, std::vector<std::string>& outIncludes);
    std::string ExtractToken(const char* token, std::string& source, bool includeToken, bool trim = false);
    size_t ExtractToken(size_t offset, const char* token, std::string& source, std::string& output, bool includeToken, bool trim = false);
    void ExtractTokens(const char* token, std::string& source, std::vector<std::string>& tokens, bool includeToken);
    void FindTokens(const char* token, const std::string& source, std::vector<std::string>& tokens, bool includeToken);
    bool FindScope(const std::string& source, size_t offset, char scopeOpen, char scopeClose, size_t* outStart = nullptr, size_t* outEnd = nullptr);
    bool FindScope(const std::string& source, size_t offset, const std::string&& scopeOpen, const std::string& scopeClose, size_t* outStart = nullptr, size_t* outEnd = nullptr);
    size_t FirstIndexOf(const char* str, char c);
    size_t LastIndexOf(const char* str, char c);
    void ReplaceAll(std::string& str, const std::string& surroundMask, const std::string& from, const std::string& to);
};