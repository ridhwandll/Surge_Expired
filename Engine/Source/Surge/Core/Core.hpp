// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Core/Profiler.hpp"
#include "Surge/Core/Client.hpp"
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/Window/Window.hpp"
#include "Surge/Core/Time/Clock.hpp"
#include <functional>

namespace Surge::Core
{   
    struct CoreData
    {
        Client* SurgeClient = nullptr; // Provided by the User

        Clock SurgeClock;
        Window* SurgeWindow = nullptr;
        Renderer* SurgeRenderer = nullptr;

        bool Running = false;
        Vector<std::function<void()>> FrameEndCallbacks;
    };

    void Initialize(Client* application);
    void Run();
    void Shutdown();

    void AddFrameEndCallback(const std::function<void()>& func); // FrameEndCallbacks are a way to accomplish some task at the very end of a frame

    // Window should be a part of core
    Window* GetWindow();
    Clock& GetClock();

    Renderer* GetRenderer();

    Client* GetClient();
    CoreData* GetData();

} // namespace Surge::Core
