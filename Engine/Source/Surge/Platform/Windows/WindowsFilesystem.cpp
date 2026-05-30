// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Utility/Filesystem.hpp"
#include <filesystem>
#include <fstream>

namespace Surge
{
    void Filesystem::CreateOrEnsureFile(const Path& path)
    {
        if (Filesystem::Exists(path))
            return;

        HANDLE hFile = ::CreateFile(path.Str().c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        SURGE_GET_WIN32_LAST_ERROR
        if (hFile != INVALID_HANDLE_VALUE)
        {
            ::WriteFile(hFile, nullptr, 0, nullptr, nullptr);
            SURGE_GET_WIN32_LAST_ERROR
            ::CloseHandle(hFile);
        }
    }

    template <>
    SURGE_API String Filesystem::ReadFile(const Path& path)
    {
        HANDLE hFile = ::CreateFile(path.Str().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        SURGE_GET_WIN32_LAST_ERROR
        String result;
        if (hFile != INVALID_HANDLE_VALUE)
        {
            DWORD size;
            size = ::GetFileSize(hFile, NULL);
            SURGE_GET_WIN32_LAST_ERROR
            result.resize(static_cast<size_t>(size));
            if (::ReadFile(hFile, result.data(), static_cast<DWORD>(result.size()), NULL, NULL) == FALSE)
                return String();
            SURGE_GET_WIN32_LAST_ERROR
            ::CloseHandle(hFile);
            return result;
        }

        return String();
    }

    template <>
    SURGE_API Vector<Uint> Filesystem::ReadFile(const Path& path)
    {
        Vector<Uint> result;
        FILE* f;
        errno_t err = fopen_s(&f, path.Str().c_str(), "rb");
        if (!err)
        {
            fseek(f, 0, SEEK_END);
            uint64_t size = ftell(f);
            fseek(f, 0, SEEK_SET);
            result = Vector<Uint>(size / sizeof(Uint));
            fread(result.data(), sizeof(Uint), result.size(), f);
            fclose(f);
        }
        else
            Log<Severity::Error>("[Filesystem::ReadFile] Cannot open path({0}) for reading!", path.Str());
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