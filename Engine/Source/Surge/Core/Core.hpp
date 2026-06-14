// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Core/Client.hpp"
#include "Surge/Core/Window/Window.hpp"
#include "Surge/Core/Time/Clock.hpp"
#include <functional>


namespace Surge
{
    class Physics;
    namespace Core
    {
        void Initialize(Client* application);
        void Run();
        void Shutdown();

        void AddFrameEndCallback(const std::function<void()>& func); // FrameEndCallbacks are a way to accomplish some task at the very end of a frame

        // Window should be a part of core
        Window* GetWindow();
        Clock& GetClock();

        Physics* GetPhysics();
        Renderer* GetRenderer();
        AssetManager* GetAssetManager();
        Client* GetClient();
    }

} // namespace Surge::Core
