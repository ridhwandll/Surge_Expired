// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/ECS/Scene.hpp"
#include "Surge/Core/Events/Event.hpp"
#include "Surge/Core/Window/Window.hpp"
#include "Surge/Core/Project.hpp"

namespace Surge
{
    struct ClientOptions
    {
        WindowDesc WindowDescription;
        bool EnableImGui = true;
        bool RenderFinalImageToSwapchian = true;
    };

    class Client
    {
    public:
        Client() = default;
        virtual ~Client() = default;

        virtual void OnInitialize() {};
        virtual void OnUpdate() {};
        virtual void OnEvent(Event& e) { (void)e; };
        virtual void OnShutdown() {};

        void SetOptions(const ClientOptions& appCreateInfo) { mClientOptions = appCreateInfo; }
        const ClientOptions& GetClientOptions() const { return mClientOptions; }

    protected:
        Project mCurrentProject;
        Ref<Scene> mActiveScene;

    private:
        ClientOptions mClientOptions;
    };

    template <typename T>
    FORCEINLINE T* MakeClient()
    {
        static_assert(std::is_base_of_v<Client, T>, "Class MUST derive from Surge::Client");
        T* client = new T();
        return client;
    }

} // namespace Surge