// Copyright (c) - SurgeTechnologies - All rights reserved
#ifdef SURGE_ANDROID
#include "Surge/Utility/Platform.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Platform/Android/AndroidApp.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Platform/Android/AndroidWindow.hpp"
#include <android/log.h>
#include <dlfcn.h>
#include <cstdlib>
#include <unistd.h>

#define SURGE_ANDROID_LOG_TAG "SurgeEngine"

namespace Surge
{
    String Platform::GetPersistantStoragePath()
    {
        android_app* app = Android::GAndroidApp;
        if (app && app->activity && app->activity->internalDataPath)
            return String(app->activity->internalDataPath);
        return "/data/data/surge";
    }

    void Platform::RequestExit()
    {
        android_app* app = Android::GAndroidApp;
        if (app && app->activity)
            ANativeActivity_finish(app->activity);
    }

    void Platform::ErrorMessageBox(const char* text)
    {
        __android_log_print(ANDROID_LOG_ERROR, SURGE_ANDROID_LOG_TAG, "ERROR: %s", text);
    }

    glm::vec2 Platform::GetScreenSize()
    {
        Window* window = Core::GetWindow();
        if (window)
            return window->GetSize();
        return {0.0f, 0.0f};
    }

    bool Platform::SetEnvVariable(const String& key, const String& value)
    {
        return setenv(key.c_str(), value.c_str(), 1) == 0;
    }

    bool Platform::HasEnvVariable(const String& key)
    {
        return getenv(key.c_str()) != nullptr;
    }

    String Platform::GetEnvVariable(const String& key)
    {
        const char* val = getenv(key.c_str());
        return val ? String(val) : String();
    }

    void* Platform::LoadSharedLibrary(const String& path)
    {
        return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    }

    void* Platform::GetFunction(void* library, const String& procAddress)
    {
        return dlsym(library, procAddress.c_str());
    }

    void Platform::UnloadSharedLibrary(void* library)
    {
        if (library)
            dlclose(library);
    }

    String Platform::GetCurrentExecutablePath()
    {
        android_app* app = Android::GAndroidApp;
        if (app && app->activity && app->activity->externalDataPath)
            return String(app->activity->externalDataPath);
        return String();
    }

} // namespace Surge

#endif // SURGE_ANDROID
