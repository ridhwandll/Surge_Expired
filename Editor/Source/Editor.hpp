// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Client.hpp"
#include "Surge/Graphics/Camera/EditorCamera.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Panels/PanelManager.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include "Surge/Asset/Texture2D.hpp"
#include "ProjectBrowser.hpp"

namespace Surge
{
    class ViewportPanel;
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

        void LoadScene(Ref<Scene> scene);
        void SetCurrentProject(const Project& project) { mCurrentProject = project; }

        Ref<Scene> GetCurrentScene() { return mActiveScene; }
        PanelManager& GetPanelManager() { return mPanelManager; }
        EditorCamera& GetCamera() { return mCamera; }

    private:
        void CheckResize();
        void OnImGuiRender();
        void RenderEditorSettings();
    private:
        // Editor Settins
        bool mShowRuntimeView = false;
        bool mShowAxes = true;

        Ref<Texture2D> mRidTex;
        EditorCamera mCamera;
        Renderer* mRenderer;
        ViewportPanel* mViewportPanel;
        ProjectBrowser mProjectBrowser;
        PanelManager mPanelManager;
    };
} // namespace Surge