// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/String.hpp"
#include "Surge/Core/Path.hpp"

namespace Surge::Filesystem
{
    inline Path ReplaceExtension(Path path, const String& newExtension)
    {
        return path.replace_extension(newExtension);
    }
    inline Path RemoveExtension(Path path)
    {
        return path.replace_extension("");
    }
    inline String GetFilenameWithExt(const Path& assetFilepath)
    {
        return assetFilepath.filename().string();
    }
    inline String GetFilenameWithoutExt(const Path& assetFilepath)
    {
        return assetFilepath.filename().stem().string();
    }
    inline Path GetParentPath(const Path& path)
    {
        return path.parent_path();
    }

    bool CreateOrEnsureDirectories(const Path& path);
    void RemoveFile(const Path& path);
    bool Exists(const Path& path);

    // Returns a list of file paths in a directory matching a specific extension (Example: ".surgeproj")
    Vector<Path> GetFilesInDirectory(const Path& directory, const String& extension = "");

    bool ReadBinaryFile(const Path& path, Vector<uint8_t>& outData);
    bool ReadTextFile(const Path& path, String& outData);

    // PC Only (Logs error on Android)
    bool WriteBinaryFile(const Path& path, const void* data, size_t size);
    bool WriteTextFile(const Path& path, const String& data);

} // namespace Surge
