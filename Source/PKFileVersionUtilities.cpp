#include "PKFileVersionUtilities.h"

namespace PKVersionUtilities
{
    void GetLastWriteTimeRecursive(const std::filesystem::path& dir, std::filesystem::file_time_type& lastTime)
    {
        for (const auto& entry : std::filesystem::directory_iterator(dir))
        {
            auto& entryPath = entry.path();

            if (!entryPath.has_extension())
            {
                GetLastWriteTimeRecursive(entryPath, lastTime);
                continue;
            }

            auto time = std::filesystem::last_write_time(entryPath);

            if (time > lastTime)
            {
                lastTime = time;
            }
        }
    }

    bool IsDirectoryOutOfDate(const std::filesystem::path& src, const::std::filesystem::path& dst)
    {
        if (!std::filesystem::exists(dst))
        {
            return true;
        }

        auto srctime = std::filesystem::file_time_type::min();
        auto dsttime = std::filesystem::file_time_type::min();
        GetLastWriteTimeRecursive(src, srctime);
        GetLastWriteTimeRecursive(dst, dsttime);
        return srctime > dsttime;
    }

    bool IsFileOutOfDate(const std::filesystem::path& src, const::std::filesystem::path& dst)
    {
        if (!std::filesystem::exists(dst))
        {
            return true;
        }

        auto srctime = std::filesystem::last_write_time(src);
        auto dsttime = std::filesystem::last_write_time(dst);
        return srctime > dsttime;
    }

    bool IsFileAnyOutOfDate(const std::vector<std::string>& paths, const std::filesystem::file_time_type& lastTime)
    {
        for (const auto& path : paths)
        {
            if (std::filesystem::exists(path))
            {
                if (std::filesystem::last_write_time(path) > lastTime)
                {
                    return true;
                }
            }
        }

        return false;
    }

    std::filesystem::file_time_type GetLastWriteTime(const std::filesystem::path& path)
    {
        if (!std::filesystem::exists(path))
        {
            return std::filesystem::file_time_type();
        }

        return std::filesystem::last_write_time(path);
    }
}
