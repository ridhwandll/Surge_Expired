// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/String.hpp"
#include <glm/glm.hpp>

namespace Surge::Platform
{
    void Initialize();
    void Shutdown();

    String GetPersistantStoragePath();
    void RequestExit();
    void ErrorMessageBox(const char* text);

    // Enters the folder in the system's file explorer. If the path is a file, it will fail to open, use OpenInExplorer() instead.
    void OpenFolderInExplorer(const String& path);

    // Opens the file or folder in the system's file explorer. If the path is a folder, it will open it. If it's a file, it will open the containing folder and select the file.
    void OpenInExplorer(const String& path);

    // If VSCode is installed and available in the system's PATH, this will open the specified workspace and file in VSCode. If the workspacePath is empty, it will only open the file.
    void OpenInVSCode(const String& workspacePath, const String& path);

    glm::vec2 GetScreenSize();

    // Sets/Gets it for the current process only. No registry.
    bool SetEnvVariableForCurrentProcess(const String& key, const String& value);
    String GetEnvVariableForCurrentProcess(const String& key);

    bool SetEnvVariable(const String& key, const String& value);
    bool HasEnvVariable(const String& key);
    String GetEnvVariable(const String& key);

    void* LoadSharedLibrary(const String& path);
    void* GetFunction(void* library, const String& procAddress);
    void UnloadSharedLibrary(void* library);
    String GetCurrentExecutablePath();

} // namespace Surge::Platform