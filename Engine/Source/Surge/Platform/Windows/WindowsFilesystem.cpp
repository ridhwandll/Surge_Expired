// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Utility/Filesystem.hpp"
#include <filesystem>
#include <fstream>
#include <Windows.h>

namespace Surge
{
    void Filesystem::CreateOrEnsureFile(const Path& path)
    {
        if (Filesystem::Exists(path))
            return;

        HANDLE hFile = ::CreateFile(path.string().c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        SURGE_GET_WIN32_LAST_ERROR
        if (hFile != INVALID_HANDLE_VALUE)
        {
            ::WriteFile(hFile, nullptr, 0, nullptr, nullptr);
            SURGE_GET_WIN32_LAST_ERROR
            ::CloseHandle(hFile);
        }
    }

    template <>
    String Filesystem::ReadFile(const Path& path)
    {
        HANDLE hFile = ::CreateFile(path.string().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
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
    Vector<Uint> Filesystem::ReadFile(const Path& path)
    {
        Vector<Uint> result;
        FILE* f;
        errno_t err = fopen_s(&f, path.string().c_str(), "rb");
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
            Log<Severity::Error>("[Filesystem::ReadFile] Cannot open path({0}) for reading!", path.string());
        return result;
    }

    bool Filesystem::CreateOrEnsureDirectory(const Path& path)
    {
        return std::filesystem::create_directories(path.string()) || std::filesystem::exists(path.string());
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
        std::filesystem::path p = path.string();
        return p.parent_path().string();
    }

    bool Filesystem::Exists(const Path& path)
    {
        return std::filesystem::exists(path.string());
    }

    void Filesystem::RemoveFile(const Path& path)
    {
        std::filesystem::remove(path.string());
    }

    template <typename T>
    T Filesystem::ReadFile(const Path& path)
    {
        static_assert(false);
    }
} // namespace Surge