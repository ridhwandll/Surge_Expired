// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/String.hpp"
#include <glm/glm.hpp>

namespace Surge::Platform
{
    String GetPersistantStoragePath();
    void RequestExit();
    void ErrorMessageBox(const char* text);
    glm::vec2 GetScreenSize();

    bool SetEnvVariable(const String& key, const String& value);
    bool HasEnvVariable(const String& key);
    String GetEnvVariable(const String& key);

    void* LoadSharedLibrary(const String& path);
    void* GetFunction(void* library, const String& procAddress);
    void UnloadSharedLibrary(void* library);
    String GetCurrentExecutablePath();

} // namespace Surge::Platform