// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Input/Input.hpp"
#include "Surge/Core/Time/Clock.hpp"
#include "Surge/Core/Window/Window.hpp"
#include "Surge/Platform/Windows/WindowsWindow.hpp"
#include "SurgeReflect/SurgeReflect.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHIDevice.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHISwapchain.hpp"
#include "Surge/Graphics/RHI/RHIDevice.hpp"
#include <filesystem>

#define ENV_VAR_KEY "SURGE_DIR"
namespace Surge::Core
{
    static CoreData GCoreData;

    void OnEvent(Event& e)
    {
        GCoreData.SurgeClient->OnEvent(e);
        Surge::EventDispatcher dispatcher(e);
        dispatcher.Dispatch<Surge::WindowResizeEvent>([](Surge::WindowResizeEvent& e) {
            if (GetWindow()->GetWindowState() != WindowState::Minimized)
            {
                Uint w = GetWindow()->GetWidth();
                Uint h = GetWindow()->GetHeight();
                RHI::vkRHISwapchainResize(GCoreData.SurgeSwapchain, w, h);
                GCoreData.SurgeRenderer->SetRenderArea(w, h);
            }
        });
        dispatcher.Dispatch<Surge::AppClosedEvent>([](Surge::AppClosedEvent& e) { GCoreData.Running = false; });
        dispatcher.Dispatch<Surge::WindowClosedEvent>([](Surge::WindowClosedEvent& e) { GCoreData.Running = false; });
    }

    void Initialize(Client* application)
    {
        SCOPED_TIMER("Core::Initialize");
        SG_ASSERT(application, "Invalid Application!");

        GCoreData.SurgeClock.Start();

        String path = Platform::GetEnvVariable(ENV_VAR_KEY);
        if (!Filesystem::Exists(path))
            Platform::SetEnvVariable(ENV_VAR_KEY, std::filesystem::current_path().string());

        GCoreData.SurgeClient = application;
        const ClientOptions& clientOptions = GCoreData.SurgeClient->GeClientOptions();

        // Window
        GCoreData.SurgeWindow = new WindowsWindow(clientOptions.WindowDescription);
        GCoreData.SurgeWindow->RegisterEventCallback(OnEvent);

        // RHI device
        RHI::gVulkanRHIDevice.Initialize();

        // Swapchain
        RHI::SwapchainDesc scDesc {};
        scDesc.WindowHandle = GCoreData.SurgeWindow->GetNativeWindowHandle();
        scDesc.Width        = GCoreData.SurgeWindow->GetWidth();
        scDesc.Height       = GCoreData.SurgeWindow->GetHeight();
        scDesc.Vsync        = false;
        GCoreData.SurgeSwapchain = RHI::vkRHICreateSwapchain(scDesc);

        // Renderer
        GCoreData.SurgeRenderer = new ForwardRenderer();
        GCoreData.SurgeRenderer->Initialize();

        // ImGui
        if (clientOptions.EnableImGui)
        {
            GCoreData.SurgeImGuiContext = new VulkanImGuiContext();
            GCoreData.SurgeImGuiContext->Initialize(GCoreData.SurgeSwapchain);
        }

        // Reflection
        SurgeReflect::Registry::Initialize();

        GCoreData.Running = true;
        GCoreData.SurgeClient->OnInitialize();
    }

    void Run()
    {
        while (GCoreData.Running)
        {
            SURGE_PROFILE_FRAME("Core::Frame");
            GCoreData.SurgeClock.Update();
            GCoreData.SurgeWindow->Update();

            if (GCoreData.SurgeWindow->GetWindowState() != WindowState::Minimized)
            {
                uint32_t frameIdx = RHI::vkRHISwapchainGetFrameIndex(GCoreData.SurgeSwapchain);
                RHI::gVulkanRHIDevice.ResetDescriptorPool(frameIdx);

                RHI::vkRHISwapchainBeginFrame(GCoreData.SurgeSwapchain);

                if (GCoreData.SurgeImGuiContext)
                    GCoreData.SurgeImGuiContext->BeginFrame();

                GCoreData.SurgeClient->OnUpdate();

                if (GCoreData.SurgeImGuiContext)
                {
                    GCoreData.SurgeClient->OnImGuiRender();
                    GCoreData.SurgeImGuiContext->Render();
                    GCoreData.SurgeImGuiContext->EndFrame();
                }

                RHI::vkRHISwapchainEndFrame(GCoreData.SurgeSwapchain, {});

                if (!GCoreData.FrameEndCallbacks.empty())
                {
                    for (std::function<void()>& function : GCoreData.FrameEndCallbacks)
                        function();
                    GCoreData.FrameEndCallbacks.clear();
                }
            }
        }
    }

    void Shutdown()
    {
        SCOPED_TIMER("Core::Shutdown");

        GCoreData.SurgeClient->OnShutdown();
        delete GCoreData.SurgeClient;
        GCoreData.SurgeClient = nullptr;

        GCoreData.SurgeRenderer->Shutdown();
        delete GCoreData.SurgeRenderer;
        GCoreData.SurgeRenderer = nullptr;

        if (GCoreData.SurgeImGuiContext)
        {
            GCoreData.SurgeImGuiContext->Destroy();
            delete GCoreData.SurgeImGuiContext;
            GCoreData.SurgeImGuiContext = nullptr;
        }

        RHI::vkRHIDestroySwapchain(GCoreData.SurgeSwapchain);
        RHI::gVulkanRHIDevice.Shutdown();

        delete GCoreData.SurgeWindow;
        GCoreData.SurgeWindow = nullptr;

        SurgeReflect::Registry::Shutdown();
    }

    void AddFrameEndCallback(const std::function<void()>& func)
    {
        GCoreData.FrameEndCallbacks.push_back(func);
    }

    Window* GetWindow() { return GCoreData.SurgeWindow; }
    ForwardRenderer* GetRenderer() { return GCoreData.SurgeRenderer; }
    VulkanImGuiContext* GetImGuiContext() { return GCoreData.SurgeImGuiContext; }
    RHI::SwapchainHandle GetSwapchain() { return GCoreData.SurgeSwapchain; }
    CoreData* GetData() { return &GCoreData; }
    Client* GetClient() { return GCoreData.SurgeClient; }
    Surge::Clock& GetClock() { return GCoreData.SurgeClock; }

} // namespace Surge::Core