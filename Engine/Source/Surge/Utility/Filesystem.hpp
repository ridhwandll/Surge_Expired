// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/String.hpp"
#include "Surge/Core/Path.hpp"

namespace Surge::Filesystem
{
	SURGE_API void CreateOrEnsureFile(const Path& path);

	// Returns empty String if failed to open the Path
	template <typename T>
	T ReadFile(const Path& path);

	SURGE_API bool CreateOrEnsureDirectory(const Path& path);
	SURGE_API String RemoveExtension(const Path& path);
	SURGE_API String GetNameWithExtension(const Path& assetFilepath);
	SURGE_API String GetNameWithoutExtension(const Path& assetFilepath);
	SURGE_API Path GetParentPath(const Path& path);
	SURGE_API void RemoveFile(const Path& path);
	SURGE_API bool Exists(const Path& path);

} // namespace Surge
