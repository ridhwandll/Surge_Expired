// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/String.hpp"
#include <glm/glm.hpp>

namespace Surge::Platform
{
    SURGE_API String GetPersistantStoragePath();
    SURGE_API void RequestExit();
    SURGE_API void ErrorMessageBox(const char* text);
    SURGE_API glm::vec2 GetScreenSize();

    SURGE_API bool SetEnvVariable(const String& key, const String& value);
    SURGE_API bool HasEnvVariable(const String& key);
    SURGE_API String GetEnvVariable(const String& key);

    SURGE_API void* LoadSharedLibrary(const String& path);
    SURGE_API void* GetFunction(void* library, const String& procAddress);
    SURGE_API void UnloadSharedLibrary(void* library);
    SURGE_API String GetCurrentExecutablePath();

} // namespace Surge::Platform