// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Client.hpp"
#include "Surge/Graphics/Camera/EditorCamera.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Panels/PaneManager.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include "Surge/Asset/Texture2D.hpp"
#include "ProjectBrowser.hpp"

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

        void SetCurrentProject(const Project& project) { mCurrentProject = project; }
        void SetActiveScene(Ref<Scene> scene) { mActiveScene = scene; }

        PanelManager& GetPanelManager() { return mPanelManager; }
        EditorCamera& GetCamera() { return mCamera; }

    private:
        void CheckResize();
        void OnImGuiRender();
    private:
        bool mShowRuntimeView = false;
        Ref<Texture2D> mRidTex;
        EditorCamera mCamera;
        Renderer* mRenderer;
        ProjectBrowser mProjectBrowser;
        PanelManager mPanelManager;
    };
} // namespace Surge