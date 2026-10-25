// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Time/Clock.hpp"
#include "Surge/Core/Window/Window.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Utility/Platform.hpp"
#include "Surge/Core/Profiler.hpp"
#include "Surge/Core/FrameCapper.hpp"
#include "Surge/Core/Diagnostics/Diagnostics.hpp"
#include "Surge/Core/Input/InputState.hpp"

#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Physics/Physics.hpp"
#include "Surge/ScriptEngine/ScriptEngine.hpp"
#include "Surge/ScriptEngine/Builtin/PerformanceOverlay.hpp"
#include "Surge/Audio/AudioEngine.hpp"
#include "Surge/ECS/SceneContext.hpp"
#include "SurgeReflect/SurgeReflectRegistry.hpp"

#ifdef SURGE_PLATFORM_WINDOWS
#include "Surge/Platform/Windows/WindowsWindow.hpp"
#elif defined(SURGE_PLATFORM_ANDROID)
#include "Surge/Platform/Android/AndroidWindow.hpp"
#endif

#define ENV_VAR_KEY "SURGE_DIR"

namespace Surge::Core
{
    // Holds one subsystem: Shutdown() (when it has one) runs right before the delete. The pointer stays readable through both,
    // what the subsystem frees while shutting down can still reach it (assets released by the AssetManager use their AssetManager)
    template <typename T>
    class Subsystem
    {
    public:
        Subsystem() = default;
        ~Subsystem() { Destroy(); }
        SURGE_DISABLE_COPY_AND_MOVE(Subsystem);

        template <typename U = T, typename... Args>
        T* Create(Args&&... args)
        {
            SG_ASSERT(!mInstance, "Subsystem created twice!");
            mInstance = new U(std::forward<Args>(args)...);
            return mInstance;
        }

        T* Get() const { return mInstance; }
        T* operator->() const { return mInstance; }

    private:
        void Destroy()
        {
            if (!mInstance)
                return;

            if constexpr (requires(T& subsystem) { subsystem.Shutdown(); })
                mInstance->Shutdown();
            delete mInstance;
            mInstance = nullptr;
        }

    private:
        T* mInstance = nullptr;
    };

    // Owns every engine subsystem. The members are declared in dependency order: Initialize() creates them top to bottom,
    // the destructor shuts them down and deletes them bottom to top, so the order lives in this declaration and nowhere else
    class Engine
    {
    public:
        Engine(Client* client) : mClient(client) {}
        ~Engine();
        SURGE_DISABLE_COPY_AND_MOVE(Engine);

        void Initialize();

        Client* GetClient() const { return mClient; }
        Window* GetWindow() const { return mWindow.Get(); }
        Renderer* GetRenderer() const { return mRenderer.Get(); }
        Physics* GetPhysics() const { return mPhysics.Get(); }
        AudioEngine* GetAudioEngine() const { return mAudioEngine.Get(); }
        Diagnostics* GetDiagnostics() const { return mDiagnostics.Get(); }
        ScriptEngine* GetScriptEngine() const { return mScriptEngine.Get(); }
        AssetManager* GetAssetManager() const { return mAssetManager.Get(); }
        const SceneContext& GetSceneContext() const { return mSceneContext; }

    private:
        Client* mClient; // Provided by the user, shut down and deleted before every subsystem

        Subsystem<Window> mWindow;
        Subsystem<Renderer> mRenderer;
        Subsystem<Physics> mPhysics;
        Subsystem<AudioEngine> mAudioEngine;
        Subsystem<Diagnostics> mDiagnostics;   // Reads the RHI
        Subsystem<ScriptEngine> mScriptEngine; // Closes the Lua state: everything holding Lua references must be gone by then
        Subsystem<AssetManager> mAssetManager; // Shut down first: the scenes and scripts it frees hold Lua references and GPU resources

        SceneContext mSceneContext; // Pointers into the subsystems above, handed to every Scene
    };

    struct CoreData
    {
        Engine* SurgeEngine = nullptr; // Raw on purpose: stays readable while the engine is deleted (a unique_ptr is null by then)

        Clock SurgeClock;
#ifdef SURGE_PLATFORM_ANDROID
        float FrameCap = 60.0f; // Mobile default: 60 fps, paced by the RHI (Swappy) when available
#else
        float FrameCap = 144.0f; // 0.0f = uncapped
#endif
        FrameCapper SurgeFrameCapper;
        bool FramePacing = true;
        bool Running = false;
        bool Suspended = false;
        Vector<std::function<void()>> FrameEndCallbacks;
    };
    static CoreData sCoreData;

    void OnEvent(Event& e)
    {
        InputState::OnEvent(e); // (Rid) Order matters here, input state first, so the client/UI already see this event reflected in Input::
        PerformanceOverlay::OnEvent(e); // Ctrl+F3, before the client so every app gets it

        Engine& engine = *sCoreData.SurgeEngine;
        engine.GetClient()->OnEvent(e);
        engine.GetRenderer()->OnEvent(e);

        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<Surge::WindowSurfaceDestroyedEvent>([&engine](Surge::WindowSurfaceDestroyedEvent&) { engine.GetRenderer()->OnSurfaceDestroyed(); });
        dispatcher.Dispatch<Surge::WindowSurfaceCreatedEvent>([&engine](Surge::WindowSurfaceCreatedEvent&) { engine.GetRenderer()->OnSurfaceCreated(engine.GetWindow()); });
        dispatcher.Dispatch<Surge::WindowClosedEvent>([](Surge::WindowClosedEvent&) { sCoreData.Running = false; });
        dispatcher.Dispatch<Surge::WindowResizeEvent>([&engine](Surge::WindowResizeEvent& e) { engine.GetRenderer()->OnWindowResize(e.GetWidth(), e.GetHeight()); });
    }

    void Engine::Initialize()
    {
        const ClientOptions& clientOptions = mClient->GetClientOptions();

#ifdef SURGE_PLATFORM_ANDROID
        mWindow.Create<AndroidWindow>(clientOptions.WindowDescription);
#else
        mWindow.Create<WindowsWindow>(clientOptions.WindowDescription);
#endif
        mWindow->RegisterEventCallback(OnEvent);

        mRenderer.Create();
        mRenderer->Initialize(mWindow.Get(), clientOptions);
        SetFrameCap(sCoreData.FrameCap);

        mPhysics.Create();
        mPhysics->Initialize();

        mAudioEngine.Create();
        mAudioEngine->Initialize();

        mDiagnostics.Create();
        mDiagnostics->Initialize();

        mScriptEngine.Create();
        mScriptEngine->Initialize();

        mAssetManager.Create();
        mSceneContext = { .Window_ = mWindow.Get(), .Clock_ = &sCoreData.SurgeClock, .Renderer_ = mRenderer.Get(), .Physics_ = mPhysics.Get(),
                          .AudioEngine_ = mAudioEngine.Get(), .ScriptEngine_ = mScriptEngine.Get(), .AssetManager_ = mAssetManager.Get() };
        mAssetManager->Initialize("Engine/Assets", { .AssetManager_ = mAssetManager.Get(), .Renderer_ = mRenderer.Get(), .Scenes = mSceneContext });
    }

    Engine::~Engine()
    {
        // The window outlives the client/renderer and destroying it still sends messages (WM_KILLFOCUS, WM_SIZE...),
        // which would reach Core::OnEvent with the client and renderer already deleted
        mWindow->RegisterEventCallback([](Event&) {});

        // TODO: Remove this
        mRenderer->GetRHI()->WaitIdle();

        mClient->OnShutdown();
        delete mClient;
        mClient = nullptr;

        // The subsystems follow, in reverse declaration order
    }

    void Initialize(Client* application)
    {
        SCOPED_TIMER("Core::Initialize");
        SG_ASSERT(application, "Invalid Application!");

        sCoreData.SurgeClock.Start();
        Platform::Initialize();

        // Reflection Engine
        SurgeReflect::Registry::Initialize();

        String path = Platform::GetEnvVariable(ENV_VAR_KEY);
        if (!Filesystem::Exists(path))
            Platform::SetEnvVariable(ENV_VAR_KEY, std::filesystem::current_path().string());

        // Assigned before Initialize: a subsystem reaches the ones created before it through Core:: while initializing
        sCoreData.SurgeEngine = new Engine(application);
        sCoreData.SurgeEngine->Initialize();

        sCoreData.Running = true;
        application->OnInitialize();
    }

    static void SetSuspended(bool suspended)
    {
        if (sCoreData.Suspended == suspended)
            return;

        sCoreData.Suspended = suspended;
        if (suspended)
        {
            Log<Severity::Info>("[Core] Engine Suspended");
            sCoreData.SurgeEngine->GetAudioEngine()->Suspend();
        }
        else
        {
            Log<Severity::Info>("[Core] Engine Resumed");
            sCoreData.SurgeEngine->GetAudioEngine()->Resume();
            sCoreData.SurgeClock.ResetDelta(); // The pause must not show up as one giant frame in physics/scripts
        }
    }

    void Run()
    {
        FrameCapper& frameCapper = sCoreData.SurgeFrameCapper;
        Engine& engine = *sCoreData.SurgeEngine;
        while (sCoreData.Running)
        {
            SURGE_PROFILE_FRAME("Core::Frame");

            frameCapper.BeginFrame();

            sCoreData.SurgeClock.Update();
            InputState::BeginFrame(sCoreData.SurgeClock.GetSeconds()); // Per frame edges/touch phases, before the window pumps new events
            engine.GetWindow()->Update();

            if (engine.GetWindow()->GetWindowState() == WindowState::MINIMIZED)
            {
                SetSuspended(true);
                continue;
            }
            SetSuspended(false);

            engine.GetPhysics()->Update(sCoreData.SurgeClock.GetSeconds() * sCoreData.SurgeClock.GetTimeScale());
            engine.GetClient()->OnUpdate();
            PerformanceOverlay::Update(); // After the frame was rendered: its widget changes show next frame, the GPU numbers are 2 frames old anyway

            if (!sCoreData.FrameEndCallbacks.empty())
            {
                // By index, moved out before the call: a callback may queue another (e.g. Scene.Load from an OnDestroy during a
                // scene switch), which grows the vector; the new one runs in this same pass
                for (size_t i = 0; i < sCoreData.FrameEndCallbacks.size(); i++)
                {
                    std::function<void()> function = std::move(sCoreData.FrameEndCallbacks[i]);
                    function();
                }

                sCoreData.FrameEndCallbacks.clear();
            }

            engine.GetDiagnostics()->EndFrame(frameCapper.GetElapsedMilliseconds()); // The whole frame, before the cap sleeps
            frameCapper.EndFrameAndCap();
        }
    }

    void Shutdown()
    {
        SCOPED_TIMER("Core::Shutdown");

        delete sCoreData.SurgeEngine; // The client, then every subsystem in reverse dependency order (see Engine)
        sCoreData.SurgeEngine = nullptr;

        SurgeReflect::Registry::Shutdown();

        Platform::Shutdown();
    }

    void AddFrameEndCallback(const std::function<void()>& func)
    {
        sCoreData.FrameEndCallbacks.push_back(func);
    }

    // Null before Initialize and after Shutdown
    Window* GetWindow() { return sCoreData.SurgeEngine ? sCoreData.SurgeEngine->GetWindow() : nullptr; }
    Physics* GetPhysics() { return sCoreData.SurgeEngine ? sCoreData.SurgeEngine->GetPhysics() : nullptr; }
    Renderer* GetRenderer() { return sCoreData.SurgeEngine ? sCoreData.SurgeEngine->GetRenderer() : nullptr; }
    AssetManager* GetAssetManager() { return sCoreData.SurgeEngine ? sCoreData.SurgeEngine->GetAssetManager() : nullptr; }
    ScriptEngine* GetScriptEngine() { return sCoreData.SurgeEngine ? sCoreData.SurgeEngine->GetScriptEngine() : nullptr; }
    AudioEngine* GetAudioEngine() { return sCoreData.SurgeEngine ? sCoreData.SurgeEngine->GetAudioEngine() : nullptr; }
    Diagnostics* GetDiagnostics() { return sCoreData.SurgeEngine ? sCoreData.SurgeEngine->GetDiagnostics() : nullptr; }
    Client* GetClient() { return sCoreData.SurgeEngine ? sCoreData.SurgeEngine->GetClient() : nullptr; }
    Clock& GetClock() { return sCoreData.SurgeClock; }

    const SceneContext& GetSceneContext()
    {
        SG_ASSERT(sCoreData.SurgeEngine, "Core::GetSceneContext called before Core::Initialize!");
        return sCoreData.SurgeEngine->GetSceneContext();
    }

    void SetTimeScale(float scale)
    {
        sCoreData.SurgeClock.SetTimeScale(scale);
    }

    float GetTimeScale()
    {
        return sCoreData.SurgeClock.GetTimeScale();
    }

    void SetFrameCap(float fps)
    {
        sCoreData.FrameCap = fps;

        // When the RHI paces presentation itself (Swappy on Android) a CPU sleep on top would only add jitter
        Renderer* renderer = GetRenderer();
        const bool rhiPaces = renderer && renderer->GetRHI()->SetTargetFrameRate(fps);
        sCoreData.SurgeFrameCapper.SetTargetFPS(rhiPaces || !sCoreData.FramePacing ? 0.0f : fps);
    }

    float GetFrameCap()
    {
        return sCoreData.FrameCap;
    }

    void SetFramePacing(bool enabled)
    {
        sCoreData.FramePacing = enabled;
        if (Renderer* renderer = GetRenderer())
            renderer->GetRHI()->SetFramePacingEnabled(enabled);
        SetFrameCap(sCoreData.FrameCap);
    }

    bool IsFramePacingEnabled()
    {
        return sCoreData.FramePacing;
    }

    String GetEngineBuildConfiguration()
    {
#if defined(SURGE_DEBUG)
        return "DEBUG";
#elif defined(SURGE_RELEASE)
        return "RELEASE";
#endif
    }

} // namespace Surge::Core
