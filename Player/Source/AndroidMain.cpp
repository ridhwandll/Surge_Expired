// Copyright (c) - SurgeTechnologies - All rights reserved
// Android entry point – replaces the desktop main() when building for Android.
#ifdef SURGE_PLATFORM_ANDROID

#include <android_native_app_glue.h>
#include <Surge/Surge.hpp>
#include "Player.hpp"
#include "Surge/Platform/Android/AndroidApp.hpp"

namespace Surge::Android
{
    android_app* GAndroidApp = nullptr;
} // namespace Surge::Android

void android_main(android_app* app)
{
    // Make GAndroidApp available to platform and window code before Core::Initialize()
    Surge::Android::GAndroidApp = app;

    // Block until the native window is ready
    while (!app->window)
    {
        int events = 0;
        android_poll_source* source = nullptr;
        ALooper_pollOnce(-1, nullptr, &events, reinterpret_cast<void**>(&source));
        if (source != nullptr)
            source->process(app, source);
        if (app->destroyRequested)
            return;
    }

    Surge::ClientOptions clientOptions;
    clientOptions.AndroidApp = (void*)app;
    clientOptions.EnableImGui = false;
    clientOptions.WindowDescription = {
        static_cast<Surge::Uint>(ANativeWindow_getWidth(app->window)),
        static_cast<Surge::Uint>(ANativeWindow_getHeight(app->window)),
        "Player",
        Surge::WindowFlags::CreateDefault};

    Surge::Player* playerApp = Surge::MakeClient<Surge::Player>();
    playerApp->SetOptions(clientOptions);
    Surge::Log<Surge::Severity::Info>("Created client!");

    Surge::Core::Initialize(playerApp);
    Surge::Log<Surge::Severity::Info>("Initialized core!");
    Surge::Core::Run();
    Surge::Core::Shutdown();
    Surge::Log<Surge::Severity::Info>("Shutting down!");
}

#endif // SURGE_PLATFORM_ANDROID