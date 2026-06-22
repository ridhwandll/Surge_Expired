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

    namespace Core
    {
        void Initialize(Client* application);
        void Run();
        void Shutdown();

        void AddFrameEndCallback(const std::function<void()>& func); // FrameEndCallbacks are a way to accomplish some task at the very end of a frame

        Window* GetWindow();
        Physics* GetPhysics();
        Renderer* GetRenderer();
        AssetManager* GetAssetManager();
        ScriptEngine* GetScriptEngine();
        Client* GetClient();
        Clock& GetClock();
    }

} // namespace Surge::Core
