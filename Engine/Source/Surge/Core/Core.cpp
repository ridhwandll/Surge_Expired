// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Time/Clock.hpp"
#include "Surge/Core/Window/Window.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Utility/Platform.hpp"
#include "Profiler.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Physics/Physics.hpp"


#ifdef SURGE_PLATFORM_WINDOWS
#include "Surge/Platform/Windows/WindowsWindow.hpp"
#elif defined(SURGE_PLATFORM_ANDROID)
#include "Surge/Platform/Android/AndroidWindow.hpp"
#endif


#define ENV_VAR_KEY "SURGE_DIR"
namespace Surge::Core
{
    struct CoreData
    {
        Client* SurgeClient = nullptr; // Provided by the User

        Clock SurgeClock;
        Window* SurgeWindow = nullptr;
        Renderer* SurgeRenderer = nullptr;
        AssetManager* SurgeAssetManager = nullptr;
        Physics* SurgePhysics = nullptr;

        bool Running = false;
        Vector<std::function<void()>> FrameEndCallbacks;
    };
    static CoreData GCoreData;

    void OnEvent(Event& e)
    {
        GCoreData.SurgeClient->OnEvent(e);
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<Surge::WindowClosedEvent>([](Surge::WindowClosedEvent& e) { GCoreData.Running = false; });
        dispatcher.Dispatch<Surge::WindowResizeEvent>([](Surge::WindowResizeEvent& e) { GCoreData.SurgeRenderer->OnWindowResize(e.GetWidth(), e.GetHeight()); });
    }

    void Initialize(Client* application)
    {
        SCOPED_TIMER("Core::Initialize");
        SG_ASSERT(application, "Invalid Application!");

        GCoreData.SurgeClock.Start();

        // Reflection Engine
        SurgeReflect::Registry::Initialize();

        String path = Platform::GetEnvVariable(ENV_VAR_KEY);
        if (!Filesystem::Exists(path))
            Platform::SetEnvVariable(ENV_VAR_KEY, std::filesystem::current_path().string());

        GCoreData.SurgeClient = application;
        const ClientOptions& clientOptions = GCoreData.SurgeClient->GetClientOptions();

        // Window
#ifdef SURGE_PLATFORM_ANDROID
        GCoreData.SurgeWindow = new AndroidWindow(clientOptions.WindowDescription);
#else
        GCoreData.SurgeWindow = new WindowsWindow(clientOptions.WindowDescription);
#endif
        GCoreData.SurgeWindow->RegisterEventCallback(OnEvent);

        // Renderer
        GCoreData.SurgeRenderer = new Renderer();
        GCoreData.SurgeRenderer->Initialize();

        // Asset Manager
        GCoreData.SurgeAssetManager = new AssetManager();
        GCoreData.SurgeAssetManager->Initialize("Engine/Assets");

        // Physics
        GCoreData.SurgePhysics = new Physics();
        GCoreData.SurgePhysics->Initialize();

        GCoreData.Running = true;
        GCoreData.SurgeClient->OnInitialize();
    }

    void Core::Run()
    {
        while (GCoreData.Running)
        {
            SURGE_PROFILE_FRAME("Core::Frame");
            GCoreData.SurgeClock.Update();
            GCoreData.SurgeWindow->Update();

            if (GCoreData.SurgeWindow->GetWindowState() == WindowState::Minimized)
                continue;

            GCoreData.SurgePhysics->Update(GCoreData.SurgeClock.GetMilliseconds());
            GCoreData.SurgeClient->OnUpdate();

            if (!GCoreData.FrameEndCallbacks.empty())
            {
                for (std::function<void()>& function : GCoreData.FrameEndCallbacks)
                    function();

                GCoreData.FrameEndCallbacks.clear();
            }
        }
    }

    void Core::Shutdown()
    {
        SCOPED_TIMER("Core::Shutdown");

        // TODO: Remove this
        GCoreData.SurgeRenderer->GetRHI()->WaitIdle();

        // NOTE(Rid): Order Matters here
        GCoreData.SurgeClient->OnShutdown();
        delete GCoreData.SurgeClient;

        GCoreData.SurgeAssetManager->Shutdown();
        delete GCoreData.SurgeAssetManager;

        GCoreData.SurgePhysics->Shutdown();
        delete GCoreData.SurgePhysics;

        GCoreData.SurgeRenderer->Shutdown();
        delete GCoreData.SurgeRenderer;

        delete GCoreData.SurgeWindow;

        SurgeReflect::Registry::Shutdown();
    }

    void Core::AddFrameEndCallback(const std::function<void()>& func)
{
        GCoreData.FrameEndCallbacks.push_back(func);
    }

    Window* GetWindow() { return GCoreData.SurgeWindow; }
    Physics* GetPhysics() { return GCoreData.SurgePhysics; }
    Renderer* GetRenderer() { return GCoreData.SurgeRenderer; }
    AssetManager* GetAssetManager() { return GCoreData.SurgeAssetManager; }
    Client* GetClient() { return GCoreData.SurgeClient; }
    Clock& GetClock() { return GCoreData.SurgeClock; }

} // namespace Surge::Core