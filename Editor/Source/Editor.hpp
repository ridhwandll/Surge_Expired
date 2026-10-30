// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Client.hpp"
#include "Surge/Graphics/Camera/EditorCamera.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include "Panels/PanelManager.hpp"
#include "Panels/EditorTitleBar.hpp"
#include "ProjectBrowser.hpp"
#include "History/EditorHistory.hpp"
#include "History/SceneCommands.hpp"
#include "Gizmos/SceneGizmos.hpp"
#include "Settings/EditorSettings.hpp"
#include "Viewport/ViewportManager.hpp"

#include "Asset/AssetDatabase.hpp"
#include <functional>

namespace Surge
{

    // Viewport toolbar icons
    enum class ToolIcon
    {
        SELECT,
        MOVE,
        ROTATE,
        SCALE,
        UNIVERSAL,
        COUNT
    };

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
        void ShowTitlebar(bool show); // Hidden (with the status bar) while a viewport is fullscreen
        bool IsTitlebarShown() const { return mShowTitlebar; }
        void OnRuntimeStart();
        void OnRuntimeEnd();
        virtual void ChangeRuntimeScene(AssetID sceneID) override; // Scene.Load from Lua while playing, the edited scene stays as it is

        // Play mode pause: the runtime scene keeps rendering, physics/scripts/audio stop. StepFrame() pauses first if needed
        bool IsPaused() const { return mRuntimeScene && mRuntimeScene->IsPaused(); }
        void SetPaused(bool paused);
        void StepFrame();

        void LoadScene(Ref<Scene>&& scene);
        void SetCurrentProject(const Project& project, const Path& projectFile); // Also loads the project's editor settings
        const Project& GetCurrentProject() const { return mCurrentProject; }
        const Path& GetProjectFilePath() const { return mProjectFilePath; }
        PhysicsSettings& GetPhysicsSettings() { return mCurrentProject.Physics; } // Physics panel, applied with Physics::ApplySettings
        void SaveProject(); // Writes the .surgeproj
        void LoadStartupScene(); // The scene that was open last time, the project's start scene otherwise
        void CloseProject();     // Back to the Project Browser (asks first when there are unsaved changes)

        // Scene files
        void NewScene();
        void OpenScene(AssetID sceneID); // Asks first when there are unsaved changes
        void SaveSceneAs(const String& relativePath); // Relative to the Assets folder, the editor switches to the new file
        void SetStartScene(AssetID sceneID);          // Writes the project file
        void OpenSaveSceneAsPopup() { mOpenSaveSceneAsPopup = true; }
        void OpenShortcutsWindow() { mShowShortcutsWindow = true; }

        EditorSettings& GetSettings() { return mSettings; }
        bool IsLightTheme() const { return mPreferences.LightTheme; }
        void SetLightTheme(bool light); // Saved right away, applied at frame end (pushed ImGui colors would restore the old theme when popped)
        void FocusEntity(Entity entity);
        void RequestResetLayout() { mResetLayoutRequested = true; }
        ImageHandle GetToolIcon(ToolIcon icon) const { return mToolIcons[static_cast<size_t>(icon)]; }
        ImageHandle LoadEditorIcon(const String& path); // Mipmapped, sampled with GetIconSampler(): for images drawn smaller than their source, caller destroys it
        SamplerHandle GetIconSampler() const { return mIconSampler; }

        Ref<Scene> GetCurrentScene() { return mActiveScene; }
        PanelManager& GetPanelManager() { return mPanelManager; }
        ViewportManager& GetViewports() { return mViewports; }
        SceneGizmos& GetGizmos() { return mGizmos; }

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
        void MoveEntity(Entity entity, Entity newParent, Entity insertAfter); // Also sets the sibling order: right after 'insertAfter' (first child when null)
        void SaveScene();

        // Undo/Redo, applied at the end of the frame (entities are created/destroyed, nothing may be iterating the registry)
        EditorHistory& GetHistory() { return mHistory; }
        void RequestUndo();
        void RequestRedo();
        void RequestHistoryJump(size_t position); //Only used in history panel

        void ConfirmDiscardChanges(const char* action, std::function<void()> onConfirm); // Runs onConfirm right away when nothing is unsaved

    private:
        void UpdateGameResolution(); // Runtime cameras follow the Game view's size
        void OnImGuiRender();
        void HandleShortcuts();
        void RenderEditorSettings();
        void RenderShortcutsWindow();
        void RenderSaveSceneAsPopup();
        void SelectEntityByUUID(UUID id);
        void QueueHistoryStep(std::function<bool(EditorHistory&, UUID&)> step); // Runs at the frame end, then restores the selection
        void RefreshCameraViewports();
        void SaveSettings(); // Captures the camera view first
    private:
        bool mShowTitlebar = true;
        bool mShowShortcutsWindow = false;
        bool mResetLayoutRequested = false;
        bool mOpenSaveSceneAsPopup = false;
        String mSaveSceneAsPath;
        EditorTitleBar mTitleBar;
        SamplerHandle mIconSampler; // Linear + mipmapped, icons are drawn well below their source size
        ImageHandle mToolIcons[static_cast<size_t>(ToolIcon::COUNT)];

        Ref<Scene> mRuntimeScene;

        EditorCamera mFrameCamera; // Only drives Renderer::BeginFrame, the viewports draw the scene with their own cameras
        Renderer* mRenderer;
        AssetManager* mAssetManager;
        ViewportManager mViewports;
        glm::vec2 mGameResolution = { 0.0f, 0.0f };
        ProjectBrowser mProjectBrowser;
        PanelManager mPanelManager;

        EditorHistory mHistory;
        EntityTreeSnapshot mClipboard;

        SceneGizmos mGizmos;
        std::function<void(Scene&)> mSceneRenderHook; // View cameras + editor-only debug geometry, runs inside the scene's rendering

        Ref<Scene> mRetiredScene;          // The scene LoadScene() replaced, kept alive until the next OnUpdate()
        Vector<AssetID> mScenesToUnload;   // Scenes whose unsaved edits were discarded, dropped from the asset cache next OnUpdate()

        Path mProjectFilePath;
        EditorSettings mSettings;
        EditorSettingsStore mSettingsStore;
        EditorPreferences mPreferences;
    };

} // namespace Surge