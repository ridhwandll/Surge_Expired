// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/String.hpp"
#include "Surge/Core/Path.hpp"

namespace Surge::Filesystem
{
    void CreateOrEnsureFile(const Path& path);

    // Returns empty String if failed to open the Path
    template <typename T>
    T ReadFile(const Path& path);

    bool CreateOrEnsureDirectory(const Path& path);
    String RemoveExtension(const Path& path);
    String GetNameWithExtension(const Path& assetFilepath);
    String GetNameWithoutExtension(const Path& assetFilepath);
    Path GetParentPath(const Path& path);
    void RemoveFile(const Path& path);
    bool Exists(const Path& path);

} // namespace Surge
