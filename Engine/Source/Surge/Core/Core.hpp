// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Core/Profiler.hpp"
#include "Surge/Core/Client.hpp"
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/Window/Window.hpp"
#include "Surge/Graphics/RenderContext.hpp"
#include "Surge/Core/Time/Clock.hpp"
#include <functional>

namespace Surge::Core
{   
    struct CoreData
    {
        Client* SurgeClient = nullptr; // Provided by the User

        Clock SurgeClock;
        Window* SurgeWindow = nullptr;
        RenderContext* SurgeRenderContext = nullptr;
        Renderer* SurgeRenderer = nullptr;

        bool Running = false;
        Vector<std::function<void()>> FrameEndCallbacks;
    };

    SURGE_API void Initialize(Client* application);
    SURGE_API void Run();
    SURGE_API void Shutdown();

    SURGE_API void AddFrameEndCallback(const std::function<void()>& func); // FrameEndCallbacks are a way to accomplish some task at the very end of a frame

    // Window should be a part of core
    SURGE_API Window* GetWindow();
    SURGE_API Clock& GetClock();

    // Part of renderer module
    SURGE_API RenderContext* GetRenderContext();
    SURGE_API Renderer* GetRenderer();

    SURGE_API Client* GetClient();
    SURGE_API CoreData* GetData();

} // namespace Surge::Core
