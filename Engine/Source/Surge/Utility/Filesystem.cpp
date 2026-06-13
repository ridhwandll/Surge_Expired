// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Filesystem.hpp"
#include "Surge/Core/Logger/Logger.hpp"

#include <algorithm>

#ifdef SURGE_PLATFORM_ANDROID
#include "Surge/Platform/Android/AndroidApp.hpp"
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <android/asset_manager.h>
#elif defined(SURGE_PLATFORM_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace Surge::Filesystem
{
#ifdef SURGE_PLATFORM_ANDROID
    static String SanitizeAndroidPath(const Path& path)
    {
        String p = path.generic_string();
        std::replace(p.begin(), p.end(), '\\', '/');
        if(!p.empty() && p[0] == '/')
            p = p.substr(1);

        return p;
    }

    bool Exists(const Path& path)
    {
        AAssetManager* mgr = Android::GAndroidApp->activity->assetManager;
        AAsset* asset = AAssetManager_open(mgr, SanitizeAndroidPath(path).c_str(), AASSET_MODE_UNKNOWN);
        if(asset)
        {
            AAsset_close(asset);
            return true;
        }
        return false;
    }

    Vector<Path> GetFilesInDirectory(const Path& directory, const String& extension)
    {
        Vector<Path> result;
        AAssetManager* mgr = Android::GAndroidApp->activity->assetManager;
        AAssetDir* assetDir = AAssetManager_openDir(mgr, SanitizeAndroidPath(directory).c_str());

        if(assetDir)
        {
            const char* filename = nullptr;
            while((filename = AAssetDir_getNextFileName(assetDir)) != nullptr)
            {
                String fileStr(filename);
                if(extension.empty() || fileStr.ends_with(extension))
                {
                    result.push_back(directory / fileStr);
                }
            }
            AAssetDir_close(assetDir);
        }
        return result;
    }

    bool ReadBinaryFile(const Path& path, Vector<uint8_t>& outData)
    {
        AAssetManager* mgr = Android::GAndroidApp->activity->assetManager;
        AAsset* asset = AAssetManager_open(mgr, SanitizeAndroidPath(path).c_str(), AASSET_MODE_BUFFER);
        if(!asset)
            return false;

        size_t size = AAsset_getLength(asset);
        outData.resize(size);
        AAsset_read(asset, outData.data(), size);
        AAsset_close(asset);
        return true;
    }

    bool ReadBinaryFilePartial(const Path& path, Vector<Byte>& outBuffer, size_t maxBytes)
    {
        AAssetManager* am = Android::GAndroidApp->activity->assetManager;
        AAsset* asset = AAssetManager_open(am, SanitizeAndroidPath(path).c_str(), AASSET_MODE_STREAMING);
        if(!asset)
            return false;

        const size_t fileSize = static_cast<size_t>(AAsset_getLength(asset));
        const size_t readSize = std::min(maxBytes, fileSize);
        outBuffer.resize(readSize);
        const bool ok = AAsset_read(asset, outBuffer.data(), readSize) == static_cast<int>(readSize);
        AAsset_close(asset);
        return ok;
    }

    bool ReadTextFile(const Path& path, String& outData)
    {
        AAssetManager* mgr = Android::GAndroidApp->activity->assetManager;
        AAsset* asset = AAssetManager_open(mgr, SanitizeAndroidPath(path).c_str(), AASSET_MODE_BUFFER);
        if(!asset)
            return false;

        size_t size = AAsset_getLength(asset);
        outData.resize(size, '\0');
        AAsset_read(asset, outData.data(), size);
        AAsset_close(asset);

        // Remove all Windows Carriage Returns (\r)
        outData.erase(std::remove(outData.begin(), outData.end(), '\r'), outData.end());

        return true;
    }

    bool WriteBinaryFile(const Path& path, const void* data, size_t size)
    {
        Log<Severity::Error>("[Filesystem] Cannot write to {}. APK assets are readonly!", path.string());
        return false;
    }

    bool WriteTextFile(const Path& path, const String& data)
    {
        Log<Severity::Error>("[Filesystem] Cannot write to {}. APK assets are readonly!", path.string());
        return false;
    }

    bool CreateOrEnsureDirectories(const Path& path)
    {
        Log<Severity::Error>("[Filesystem] Cannot create {}. APK assets are readonly!", path.string());
        return false;
    }

    void RemoveFile(const Path& path)
    {
        Log<Severity::Error>("[Filesystem] Cannot remove {}. APK assets are readonly!", path.string());
    }

#elif defined(SURGE_PLATFORM_WINDOWS)

    bool Exists(const Path& path)
    {
        DWORD attributes = GetFileAttributesW(path.c_str());
        return (attributes != INVALID_FILE_ATTRIBUTES);
    }

    Vector<Path> GetFilesInDirectory(const Path& directory, const String& extension)
    {
        // For directory iteration, std::filesystem is already a highly optimized wrapper 
        // around Win32 FindFirstFileW/FindNextFileW. It is best practice to keep this as-is.
        Vector<Path> result;
        if(Exists(directory) && std::filesystem::is_directory(directory))
        {
            for(const auto& entry : std::filesystem::directory_iterator(directory))
            {
                if(entry.is_regular_file())
                {
                    if(extension.empty() || entry.path().extension().string() == extension)
                        result.push_back(entry.path());
                }
            }
        }
        return result;
    }

    bool ReadBinaryFile(const Path& path, Vector<uint8_t>& outData)
    {
        HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if(hFile == INVALID_HANDLE_VALUE)
            return false;

        LARGE_INTEGER fileSize;
        if(!GetFileSizeEx(hFile, &fileSize))
        {
            CloseHandle(hFile);
            return false;
        }

        outData.resize(fileSize.QuadPart);
        DWORD bytesRead = 0;

        if(!ReadFile(hFile, outData.data(), static_cast<DWORD>(fileSize.QuadPart), &bytesRead, NULL) || bytesRead != fileSize.QuadPart)
        {
            CloseHandle(hFile);
            return false;
        }

        CloseHandle(hFile);
        return true;
    }

    bool ReadBinaryFilePartial(const Path& path, Vector<Byte>& outBuffer, size_t maxBytes)
    {
        HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if(hFile == INVALID_HANDLE_VALUE)
            return false;

        LARGE_INTEGER fileSize;
        if(!GetFileSizeEx(hFile, &fileSize))
        {
            CloseHandle(hFile);
            return false;
        }

        outBuffer.resize(maxBytes);
        DWORD bytesRead = 0;
        if(!ReadFile(hFile, outBuffer.data(), maxBytes, &bytesRead, NULL) || bytesRead != maxBytes)
        {
            CloseHandle(hFile);
            return false;
        }

        CloseHandle(hFile);
        return true;
    }

    bool ReadTextFile(const Path& path, String& outData)
    {
        HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if(hFile == INVALID_HANDLE_VALUE)
            return false;

        LARGE_INTEGER fileSize;
        if(!GetFileSizeEx(hFile, &fileSize))
        {
            CloseHandle(hFile);
            return false;
        }

        outData.resize(fileSize.QuadPart, '\0');

        DWORD bytesRead = 0;
        if(!ReadFile(hFile, outData.data(), static_cast<DWORD>(fileSize.QuadPart), &bytesRead, NULL) || bytesRead != fileSize.QuadPart)
        {
            CloseHandle(hFile);
            return false;
        }

        CloseHandle(hFile);

        // Strip Windows \r
        outData.erase(std::remove(outData.begin(), outData.end(), '\r'), outData.end());
        return true;
    }

    bool WriteBinaryFile(const Path& path, const void* data, size_t size)
    {
        HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if(hFile == INVALID_HANDLE_VALUE)
            return false;

        DWORD bytesWritten = 0;
        bool success = WriteFile(hFile, data, static_cast<DWORD>(size), &bytesWritten, NULL) && (bytesWritten == size);

        CloseHandle(hFile);
        return success;
    }

    bool WriteTextFile(const Path& path, const String& data)
    {
        HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if(hFile == INVALID_HANDLE_VALUE)
            return false;

        DWORD bytesWritten = 0;
        bool success = WriteFile(hFile, data.data(), static_cast<DWORD>(data.size()), &bytesWritten, NULL) && (bytesWritten == data.size());

        CloseHandle(hFile);
        return success;
    }

    bool CreateOrEnsureDirectories(const Path& path)
    {
        // (Rid)Keeping std::filesystem for writing recursive directory creation
        return std::filesystem::create_directories(path);
    }

    void RemoveFile(const Path& path)
    {
        DeleteFileW(path.c_str());
    }

#endif
}
