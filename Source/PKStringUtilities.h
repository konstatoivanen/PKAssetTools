#pragma once
#include <string>
#include <vector>

namespace PKAssets::StringUtilities
{
    bool IsAlphaNumeric(char c);
    bool IsSwizzleChar(char c);
    std::string Trim(const std::string& value);
    std::vector<std::string> Split(const std::string& value, const char* symbols);
    std::vector<std::string> SplitNoWhiteSpace(const std::string& value, const char* symbols);
    std::string ReadFileName(const std::string& filepath);
    std::string ReadDirectory(const std::string& filepath);
    std::string ReadFileRecursiveInclude(const std::string& filepath, std::vector<std::string>& outIncludes);
    std::string ExtractToken(const char* token, std::string& source, bool includeToken, bool trim = false);
    std::string GetLineAtIndex(const std::string& string, size_t index);
    size_t ExtractToken(size_t offset, const char* token, std::string& source, std::string& output, bool includeToken, bool trim = false);
    void ExtractTokens(const char* token, std::string& source, std::vector<std::string>& tokens, bool includeToken);
    void FindTokens(const char* token, const std::string& source, std::vector<std::string>& tokens, bool includeToken);
    bool FindScope(const std::string& source, size_t offset, char scopeOpen, char scopeClose, size_t* outBeg = nullptr, size_t* outEnd = nullptr);
    bool FindScope(const std::string& source, size_t offset, const std::string&& scopeOpen, const std::string& scopeClose, size_t* outBeg = nullptr, size_t* outEnd = nullptr);
    size_t FirstIndexOf(const char* str, char c);
    size_t LastIndexOf(const char* str, char c);
    void ConvertTabsToSpaces(std::string& value);
    void ReplaceAll(std::string& str, const std::string& surroundMask, const std::string& from, const std::string& to);
    void ClearTextRange(std::string& str, size_t beg, size_t end);

    void RemoveCommentsAndPadScalarSwizzles(std::string& source);
    bool InjectFunctionAttribute(std::string& source, const std::string& functionName, const std::string& attribute);
    bool ParseUniform(const std::string& source, size_t uniformBodyOffset, std::string& outType, std::string& outName, uint32_t* outCount, size_t* outEnd);
    bool ExtractDiagnosticLineIndex(const std::string& line, const std::string& moduleName, size_t* outLineIndex);
};