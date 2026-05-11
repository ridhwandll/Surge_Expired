// Copyright (c) - SurgeTechnologies - All rights reserved
// Android entry point – replaces the desktop main() when building for Android.
#ifdef SURGE_PLATFORM_ANDROID

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <Surge/Surge.hpp>
#include "Player.hpp"
#include "Surge/Platform/Android/AndroidApp.hpp"

namespace Surge::Android
{
    android_app* GAndroidApp = nullptr;

} // namespace Surge::Android

void android_main(android_app* app)
{
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
    clientOptions.EnableImGui = false;
    Surge::WindowDesc desc;
    desc.Width = static_cast<Surge::Uint>(ANativeWindow_getWidth(app->window));
    desc.Height = static_cast<Surge::Uint>(ANativeWindow_getHeight(app->window));
    desc.Title = "SurgePlayer";
    desc.Flags = Surge::WindowFlags::CreateDefault;
    clientOptions.WindowDescription = desc;

    auto* playerApp = Surge::MakeClient<Surge::Player>();
    playerApp->SetOptions(clientOptions);

    Surge::Core::Initialize(playerApp);
    Surge::Core::Run();
    Surge::Core::Shutdown();
    Surge::Log<Surge::Severity::Info>("Shutting down");
}

#endif // SURGE_PLATFORM_ANDROID