#pragma once
#include <stdio.h>
#include <filesystem>
#include <vector>

namespace PKVersionUtilities
{
    void GetLastWriteTimeRecursive(const std::filesystem::path& dir, std::filesystem::file_time_type& lastTime);
    bool IsDirectoryOutOfDate(const std::filesystem::path& src, const std::filesystem::path& dst);
    bool IsFileOutOfDate(const std::filesystem::path& src, const std::filesystem::path& dst);
    bool IsFileAnyOutOfDate(const std::vector<std::string>& paths, const std::filesystem::file_time_type& lastTime);
    std::filesystem::file_time_type GetLastWriteTime(const std::filesystem::path& path);
}