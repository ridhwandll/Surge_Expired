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
        return std::filesystem::create_directories(path.Str()) || std::filesystem::exists(path.Str());
    }

    String Filesystem::RemoveExtension(const Path& path)
    {
        size_t lastindex = path.Str().find_last_of(".");
        String rawName = path.Str().substr(0, lastindex);
        return rawName;
    }

    String Filesystem::GetNameWithExtension(const Path& assetFilepath) { return std::filesystem::path(assetFilepath.Str()).filename().string(); }

    String Filesystem::GetNameWithoutExtension(const Path& assetFilepath)
    {
        String name;
        auto lastSlash = assetFilepath.Str().find_last_of("/\\");
        lastSlash = lastSlash == String::npos ? 0 : lastSlash + 1;
        auto lastDot = assetFilepath.Str().rfind('.');
        auto count = lastDot == String::npos ? assetFilepath.Str().size() - lastSlash : lastDot - lastSlash;
        name = assetFilepath.Str().substr(lastSlash, count);
        return name;
    }

    Path Filesystem::GetParentPath(const Path& path)
    {
        std::filesystem::path p = path.Str();
        return p.parent_path().string();
    }

    bool Filesystem::Exists(const Path& path)
    {
        return std::filesystem::exists(path.Str());
    }

    void Filesystem::RemoveFile(const Path& path)
    {
        std::filesystem::remove(path.Str());
    }

    template <typename T>
    T Filesystem::ReadFile(const Path& path)
    {
        static_assert(false);
    }
} // namespace Surge

#endif