// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Time/Clock.hpp"
#include "Surge/Core/Window/Window.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Utility/Platform.hpp"
#include "Surge/Core/Profiler.hpp"

#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Physics/Physics.hpp"
#include "Surge/ScriptEngine/ScriptEngine.hpp"
#include "SurgeReflect/SurgeReflectRegistry.hpp"

#ifdef SURGE_PLATFORM_WINDOWS
#include "Surge/Platform/Windows/WindowsWindow.hpp"
#elif defined(SURGE_PLATFORM_ANDROID)
#include "Surge/Platform/Android/AndroidWindow.hpp"
#endif
#include "../Audio/AudioEngine.hpp"


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
        ScriptEngine* SurgeScriptEngine = nullptr;
        AudioEngine* SurgeAudioEngine = nullptr;

        bool Running = false;
        Vector<std::function<void()>> FrameEndCallbacks;
    };
    static CoreData sCoreData;

    void OnEvent(Event& e)
    {
        sCoreData.SurgeClient->OnEvent(e);
        sCoreData.SurgeRenderer->OnEvent(e);

        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<Surge::WindowClosedEvent>([](Surge::WindowClosedEvent&) { sCoreData.Running = false; });
        dispatcher.Dispatch<Surge::WindowResizeEvent>([](Surge::WindowResizeEvent& e) { sCoreData.SurgeRenderer->OnWindowResize(e.GetWidth(), e.GetHeight()); });
    }

    void Initialize(Client* application)
    {
        SCOPED_TIMER("Core::Initialize");
        SG_ASSERT(application, "Invalid Application!");

        sCoreData.SurgeClock.Start();

        // Reflection Engine
        SurgeReflect::Registry::Initialize();

        String path = Platform::GetEnvVariable(ENV_VAR_KEY);
        if (!Filesystem::Exists(path))
            Platform::SetEnvVariable(ENV_VAR_KEY, std::filesystem::current_path().string());

        sCoreData.SurgeClient = application;
        const ClientOptions& clientOptions = sCoreData.SurgeClient->GetClientOptions();

        // Window
#ifdef SURGE_PLATFORM_ANDROID
        sCoreData.SurgeWindow = new AndroidWindow(clientOptions.WindowDescription);
#else
        sCoreData.SurgeWindow = new WindowsWindow(clientOptions.WindowDescription);
#endif
        sCoreData.SurgeWindow->RegisterEventCallback(OnEvent);

        // Renderer
        sCoreData.SurgeRenderer = new Renderer();
        sCoreData.SurgeRenderer->Initialize();

        // Asset Manager
        sCoreData.SurgeAssetManager = new AssetManager();
        sCoreData.SurgeAssetManager->Initialize("Engine/Assets");

        // Physics
        sCoreData.SurgePhysics = new Physics();
        sCoreData.SurgePhysics->Initialize();

        // Audio Engine
        sCoreData.SurgeAudioEngine = new AudioEngine();
        sCoreData.SurgeAudioEngine->Initialize();

        // Script Engine
        sCoreData.SurgeScriptEngine = new ScriptEngine();
        sCoreData.SurgeScriptEngine->Initialize();

        sCoreData.Running = true;
        sCoreData.SurgeClient->OnInitialize();
    }

    void Run()
    {
        while (sCoreData.Running)
        {
            SURGE_PROFILE_FRAME("Core::Frame");
            sCoreData.SurgeClock.Update();
            sCoreData.SurgeWindow->Update();

            if (sCoreData.SurgeWindow->GetWindowState() == WindowState::MINIMIZED)
                continue;

            sCoreData.SurgePhysics->Update(sCoreData.SurgeClock.GetSeconds());
            sCoreData.SurgeClient->OnUpdate();

            if (!sCoreData.FrameEndCallbacks.empty())
            {
                for (std::function<void()>& function : sCoreData.FrameEndCallbacks)
                    function();

                sCoreData.FrameEndCallbacks.clear();
            }
        }
    }

    void Shutdown()
    {
        SCOPED_TIMER("Core::Shutdown");

        // TODO: Remove this
        sCoreData.SurgeRenderer->GetRHI()->WaitIdle();

        // NOTE(Rid): Order Matters here
        sCoreData.SurgeClient->OnShutdown();
        delete sCoreData.SurgeClient;
        
        sCoreData.SurgeScriptEngine->Shutdown();
        delete sCoreData.SurgeScriptEngine;

        sCoreData.SurgeAudioEngine->Shutdown();
        delete sCoreData.SurgeAudioEngine;

        sCoreData.SurgeAssetManager->Shutdown();
        delete sCoreData.SurgeAssetManager;

        sCoreData.SurgePhysics->Shutdown();
        delete sCoreData.SurgePhysics;

        sCoreData.SurgeRenderer->Shutdown();
        delete sCoreData.SurgeRenderer;

        delete sCoreData.SurgeWindow;

        SurgeReflect::Registry::Shutdown();
    }

    void AddFrameEndCallback(const std::function<void()>& func)
{
        sCoreData.FrameEndCallbacks.push_back(func);
    }

    Window* GetWindow() { return sCoreData.SurgeWindow; }
    Physics* GetPhysics() { return sCoreData.SurgePhysics; }
    Renderer* GetRenderer() { return sCoreData.SurgeRenderer; }
    AssetManager* GetAssetManager() { return sCoreData.SurgeAssetManager; }
    ScriptEngine* GetScriptEngine() { return sCoreData.SurgeScriptEngine; }
    AudioEngine* GetAudioEngine() { return sCoreData.SurgeAudioEngine; }
    Client* GetClient() { return sCoreData.SurgeClient; }
    Clock& GetClock() { return sCoreData.SurgeClock; }

} // namespace Surge::Core
