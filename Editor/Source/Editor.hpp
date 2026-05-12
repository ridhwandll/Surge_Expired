// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Client.hpp"
#include "Surge/Graphics/Camera/EditorCamera.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Panels/Titlebar.hpp"
#include "Panels/PaneManager.hpp"

namespace Surge
{
    class Editor : public Surge::Client
    {
    public:
        Editor() = default;
        virtual ~Editor() = default;

        virtual void OnInitialize() override;
        virtual void OnUpdate() override;
        virtual void OnEvent(Event& e) override;
        virtual void OnShutdown() override;

        // Editor specific
        void OnRuntimeStart();
        void OnRuntimeEnd();

        PanelManager& GetPanelManager() { return mPanelManager; }
        Titlebar& GetTitlebar() { return mTitleBar; }
        EditorCamera& GetCamera() { return mCamera; }

    private:
        void Resize();
        void OnImGuiRender();
    private:
        EditorCamera mCamera;
        Renderer* mRenderer;

        PanelManager mPanelManager;
        Titlebar mTitleBar {};
    };
} // namespace Surge