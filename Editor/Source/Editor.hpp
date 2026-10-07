// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Client.hpp"
#include "Surge/Graphics/Camera/EditorCamera.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include "Panels/PanelManager.hpp"
#include "ProjectBrowser.hpp"
#include "History/EditorHistory.hpp"
#include "History/SceneCommands.hpp"

#include "Asset/AssetImporter.hpp"
#include <functional>

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
        virtual Ref<Scene> GetRuntimeScene() const override { return mRuntimeScene ? mRuntimeScene : mActiveScene; }

        bool IsPlaying() const { return (bool)mRuntimeScene; }
        void ShowTitlebar(bool show) { mShowTitlebar = show; }
        void OnRuntimeStart();
        void OnRuntimeEnd();

        void LoadScene(Ref<Scene>&& scene);
        void SetCurrentProject(const Project& project) { mCurrentProject = project; }
        const Project& GetCurrentProject() const { return mCurrentProject; }

        const AssetImporter& GetAssetImporter() const { return mAssetImporter; }
        AssetImporter& GetAssetImporter() { return mAssetImporter; }
        Ref<Scene> GetCurrentScene() { return mActiveScene; }
        PanelManager& GetPanelManager() { return mPanelManager; }
        EditorCamera& GetCamera() { return mCamera; }

        // Scene editing: every operation below is recorded into the undo history while in edit mode
        Scene* GetContextScene() { return mRuntimeScene ? mRuntimeScene.Raw() : mActiveScene.Raw(); } // The scene the panels show
        Entity GetSelectedEntity();
        void SelectEntity(Entity entity);
        void OnEntityCreated(Entity entity);   // Called after the entity got its components, records "Create" and selects it
        void DeleteEntity(Entity entity);
        Entity DuplicateEntity(Entity entity); // Children included, placed right after the original
        void CopyEntity(Entity entity);
        void PasteEntity();                    // As a sibling right after the selection (root level without one)
        bool HasEntityInClipboard() const { return mClipboard.IsValid(); }
        void ReparentEntity(Entity entity, Entity newParent);
        void SaveScene();

        // Undo/Redo, applied at the end of the frame (entities are created/destroyed, nothing may be iterating the registry)
        EditorHistory& GetHistory() { return mHistory; }
        void RequestUndo();
        void RequestRedo();
        void RequestHistoryJump(size_t position); //Only used in history panel

        void ConfirmDiscardChanges(const char* action, std::function<void()> onConfirm); // Runs onConfirm right away when nothing is unsaved

    private:
        void CheckResize();
        void OnImGuiRender();
        void HandleShortcuts();
        void DrawCustomTitlebar(const char* title, float titleBarHeight, bool showMenuItems = true);
        void DrawEditMenu();
        void DrawConsoleCounters(float rightEdgeX, float y);
        void RenderEditorSettings();
        void RenderShortcutsWindow();
        void SelectEntityByUUID(UUID id);
        void QueueHistoryStep(std::function<bool(EditorHistory&, UUID&)> step); // Runs at the frame end, then restores the selection
        void RefreshCameraViewports();
    private:
        bool mShowTitlebar = true;
        bool mShowAxes = true;
        bool mShowShortcutsWindow = false;
        ImageHandle mEngineLogo;
        ImageHandle mMinimize;
        ImageHandle mMaximize;
        ImageHandle mClose;

        Ref<Scene> mRuntimeScene;

        AssetImporter mAssetImporter;
        EditorCamera mCamera;
        Renderer* mRenderer;
        AssetManager* mAssetManager;
        ViewportPanel* mViewportPanel;
        ProjectBrowser mProjectBrowser;
        PanelManager mPanelManager;

        EditorHistory mHistory;
        EntityTreeSnapshot mClipboard;
    };
} // namespace Surge