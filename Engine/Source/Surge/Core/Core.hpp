// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Client.hpp"
#include <functional>

namespace Surge
{
    class Clock;
    class Window;
    class Physics;
    class Renderer;
    class AssetManager;
    class ScriptEngine;
    class AudioEngine;
    class Diagnostics;
    struct SceneContext;

    // Global access to the engine's subsystems, owned by Core::Engine (Core.cpp). For the layers above the engine (Client, Editor, Player,
    // Lua bindings): RHI, RenderGraph, HighLevel, Asset and ECS code get what they use handed in instead (no header includes Core.hpp, so they can't reach it)
    namespace Core
    {
        void Initialize(Client* application);
        void Run();
        void Shutdown();
        void SetFrameCap(float fps);
        float GetFrameCap(); // 0 = uncapped

        // Profiling switch: false presents every frame as soon as it is ready (no Swappy, no CPU frame cap) so the GPU stays fully
        // loaded and its timings show the real cost. Paced, a phone lowers the GPU clock until the work fills the frame interval
        void SetFramePacing(bool enabled);
        bool IsFramePacingEnabled();

        // Game time scale: scales the physics step accumulation and the delta time scripts get (0 = frozen, 1 = normal)
        void SetTimeScale(float scale);
        float GetTimeScale();
        void AddFrameEndCallback(const std::function<void()>& func); // FrameEndCallbacks are a way to accomplish some task at the very end of a frame

        Window* GetWindow();
        Physics* GetPhysics();
        Renderer* GetRenderer();
        AssetManager* GetAssetManager();
        ScriptEngine* GetScriptEngine();
        AudioEngine* GetAudioEngine();
        Diagnostics* GetDiagnostics();
        Client* GetClient();
        Clock& GetClock();
        const SceneContext& GetSceneContext(); // Handed to every Scene on creation, valid between Initialize and Shutdown

        String GetEngineBuildConfiguration();
    }

} // namespace Surge::Core
