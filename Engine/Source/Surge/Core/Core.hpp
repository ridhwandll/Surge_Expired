// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Client.hpp"
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/Window/Window.hpp"
#include "Surge/Core/Time/Clock.hpp"
#include "Surge/Graphics/Renderer/ForwardRenderer.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanImGuiContext.hpp"
#include "Surge/Graphics/RHI/RHIResources.hpp"

namespace Surge::Core
{
    struct CoreData
    {
        Client* SurgeClient = nullptr;

        Clock SurgeClock;
        Window* SurgeWindow = nullptr;
        ForwardRenderer* SurgeRenderer = nullptr;
        VulkanImGuiContext* SurgeImGuiContext = nullptr;
        RHI::SwapchainHandle SurgeSwapchain;

        bool Running = false;
        Vector<std::function<void()>> FrameEndCallbacks;
    };

    SURGE_API void Initialize(Client* application);
    SURGE_API void Run();
    SURGE_API void Shutdown();

    SURGE_API void AddFrameEndCallback(const std::function<void()>& func);

    SURGE_API Window* GetWindow();
    SURGE_API Clock& GetClock();
    SURGE_API ForwardRenderer* GetRenderer();
    SURGE_API VulkanImGuiContext* GetImGuiContext();
    SURGE_API RHI::SwapchainHandle GetSwapchain();
    SURGE_API Client* GetClient();
    SURGE_API CoreData* GetData();

} // namespace Surge::Core
