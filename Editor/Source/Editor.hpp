// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Client.hpp"
#include "Surge/Graphics/Camera/EditorCamera.hpp"
#include "Panels/PanelManager.hpp"
#include "ProjectBrowser.hpp"

#include "Asset/AssetImporter.hpp"

namespace Surge
{
    class ViewportPanel;
    class Renderer;
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

        void LoadScene(Ref<Scene>&& scene);
        void SetCurrentProject(const Project& project) { mCurrentProject = project; }
        const Project& GetCurrentProject() const { return mCurrentProject; }

        const AssetImporter& GetAssetImporter() const { return mAssetImporter; }
        Ref<Scene> GetCurrentScene() { return mActiveScene; }
        PanelManager& GetPanelManager() { return mPanelManager; }
        EditorCamera& GetCamera() { return mCamera; }

    private:
        void CheckResize();
        void OnImGuiRender();
        void RenderEditorSettings();
    private:
        bool mShowAxes = true;

        Ref<Scene> mRuntimeScene;

        AssetImporter mAssetImporter;
        EditorCamera mCamera;
        Renderer* mRenderer;
        AssetManager* mAssetManager;
        ViewportPanel* mViewportPanel;
        ProjectBrowser mProjectBrowser;
        PanelManager mPanelManager;
    };
} // namespace Surge