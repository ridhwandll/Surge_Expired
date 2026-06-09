// Copyright (c) - SurgeTechnologies - All rights reserved
#ifdef SURGE_PLATFORM_ANDROID

#include "Surge/Utility/Filesystem.hpp"
#include <filesystem>
#include <fstream>


namespace Surge
{
    void Filesystem::CreateOrEnsureFile(const Path& path)
    {
    }

    template <>
    SURGE_API String Filesystem::ReadFile(const Path& path)
    {
        return String();
    }

    template <>
    SURGE_API Vector<Uint> Filesystem::ReadFile(const Path& path)
    {
        Vector<Uint> result;
        return result;
    }

    bool Filesystem::CreateOrEnsureDirectory(const Path& path)
    {
        return false;
    }

    String Filesystem::RemoveExtension(const Path& path)
    {
        size_t lastindex = path.string().find_last_of(".");
        String rawName = path.string().substr(0, lastindex);
        return rawName;
    }

    String Filesystem::GetNameWithExtension(const Path& assetFilepath) { return std::filesystem::path(assetFilepath.string()).filename().string(); }

    String Filesystem::GetNameWithoutExtension(const Path& assetFilepath)
    {
        String name;
        auto lastSlash = assetFilepath.string().find_last_of("/\\");
        lastSlash = lastSlash == String::npos ? 0 : lastSlash + 1;
        auto lastDot = assetFilepath.string().rfind('.');
        auto count = lastDot == String::npos ? assetFilepath.string().size() - lastSlash : lastDot - lastSlash;
        name = assetFilepath.string().substr(lastSlash, count);
        return name;
    }

    Path Filesystem::GetParentPath(const Path& path)
    {
        return path.parent_path().string();
    }

    bool Filesystem::Exists(const Path& path)
    {
        return std::filesystem::exists(path);
    }

    void Filesystem::RemoveFile(const Path& path)
    {
        std::filesystem::remove(path);
    }

    template <typename T>
    T Filesystem::ReadFile(const Path& path)
    {
        static_assert(false);
    }
} // namespace Surge

#endif