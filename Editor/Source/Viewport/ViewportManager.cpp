// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Viewport/ViewportManager.hpp"
#include "Viewport/SceneViewport.hpp"
#include "Viewport/GameViewport.hpp"
#include "Settings/EditorSettings.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Input/Input.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Editor.hpp"
#include "Utility/EditorUI.hpp"

#include <imgui_internal.h>
#include <algorithm>

namespace Surge
{
    namespace
    {
        EditorCamera DefaultSceneCamera() { return EditorCamera(45.0f, 1.778f, 0.1f, 1000.0f); }

        SceneViewport* AsScene(EditorViewport* viewport) { return viewport && viewport->GetType() == ViewportType::SCENE ? static_cast<SceneViewport*>(viewport) : nullptr; }
        GameViewport* AsGame(EditorViewport* viewport) { return viewport && viewport->GetType() == ViewportType::GAME ? static_cast<GameViewport*>(viewport) : nullptr; }
    } // namespace

    void ViewportManager::Restore(const Vector<ViewportSettings>& saved)
    {
        Clear();
        if(saved.empty())
        {
            OpenGameView();
            AddSceneView(); // Last, it gets the focus
            return;
        }

        for(const ViewportSettings& settings : saved)
        {
            if(settings.Type == static_cast<int>(ViewportType::GAME))
            {
                if(mGameView)
                    continue; // Projects saved while several Game views were allowed: the first one wins

                Scope<GameViewport> viewport = CreateScope<GameViewport>();
                viewport->ApplySettings(settings);
                AddViewport(std::move(viewport));
            }
            else
            {
                Scope<SceneViewport> viewport = CreateScope<SceneViewport>(settings.Index, DefaultSceneCamera());
                viewport->ApplySettings(settings);
                AddViewport(std::move(viewport));
            }
        }

        // A layout from before these windows existed (or a deleted imgui.ini): show the Scene view instead of whatever tab shares its dock
        if(mActiveScene && !ImGui::FindWindowSettingsByID(ImHashStr(mActiveScene->GetWindowName().c_str())))
            mActiveScene->RequestFocus();
    }

    void ViewportManager::Capture(Vector<ViewportSettings>& outSaved) const
    {
        outSaved.clear();
        for(const Scope<EditorViewport>& viewport : mViewports)
        {
            ViewportSettings& settings = outSaved.emplace_back();
            settings.Type = static_cast<int>(viewport->GetType());
            settings.Index = viewport->GetIndex();
            if(SceneViewport* sceneView = AsScene(viewport.get()))
                sceneView->CaptureSettings(settings);
            else if(GameViewport* gameView = AsGame(viewport.get()))
                gameView->CaptureSettings(settings);
        }
    }

    void ViewportManager::Clear()
    {
        if(mCameraOwner)
            Input::SetCursorMode(CursorMode::Normal);

        mActiveScene = nullptr;
        mGameView = nullptr;
        mCameraOwner = nullptr;
        mViewports.clear();
    }

    void ViewportManager::OnUpdate()
    {
        // The hovered Scene view owns the camera input, and keeps it while a button is held (a fly/orbit drag may leave the window)
        const bool buttonHeld = Input::IsMouseButtonPressed(Mouse::ButtonLeft) || Input::IsMouseButtonPressed(Mouse::ButtonRight) || Input::IsMouseButtonPressed(Mouse::ButtonMiddle);
        if(!buttonHeld)
        {
            SceneViewport* hovered = nullptr;
            for(const Scope<EditorViewport>& viewport : mViewports)
            {
                if(viewport->IsHovered())
                    hovered = AsScene(viewport.get());
            }

            if(mCameraOwner && hovered != mCameraOwner)
                Input::SetCursorMode(CursorMode::Normal); // Only the active camera unlocks the cursor it locked
            mCameraOwner = hovered;
        }

        for(const Scope<EditorViewport>& viewport : mViewports)
        {
            if(SceneViewport* sceneView = AsScene(viewport.get()))
                sceneView->UpdateCamera(sceneView == mCameraOwner);
        }
    }

    void ViewportManager::Render()
    {
        // The in-game UI is laid out at the Game view's size
        Core::GetRenderer()->SetUIView(mGameView ? mGameView->GetViewID() : INVALID_RENDER_VIEW);

        for(const Scope<EditorViewport>& viewport : mViewports)
        {
            viewport->Render();
            if(SceneViewport* sceneView = AsScene(viewport.get()); sceneView && (sceneView->IsFocused() || sceneView->IsHovered()))
                mActiveScene = sceneView;
        }

        // Closed tabs, the Renderer releases their views at frame end
        for(size_t i = mViewports.size(); i-- > 0;)
        {
            EditorViewport* viewport = mViewports[i].get();
            if(viewport->IsOpen())
                continue;

            if(viewport == mActiveScene)
                mActiveScene = nullptr;
            if(viewport == mGameView)
                mGameView = nullptr;
            if(viewport == mCameraOwner)
            {
                Input::SetCursorMode(CursorMode::Normal);
                mCameraOwner = nullptr;
            }
            mViewports.erase(mViewports.begin() + static_cast<std::ptrdiff_t>(i));
        }

        for(size_t i = 0; i < mViewports.size() && !mActiveScene; i++)
            mActiveScene = AsScene(mViewports[i].get());
    }

    void ViewportManager::OnEvent(Event& e)
    {
        for(const Scope<EditorViewport>& viewport : mViewports)
            viewport->OnEvent(e);

        if(mCameraOwner)
            mCameraOwner->GetCamera().OnEvent(e);
    }

    void ViewportManager::UpdateViewCameras(Scene& scene)
    {
        for(const Scope<EditorViewport>& viewport : mViewports)
            viewport->UpdateViewCamera(scene);
    }

    SceneViewport* ViewportManager::AddSceneView()
    {
        // Starts where the active Scene view looks, like Unity's "Add Tab > Scene"
        const EditorCamera camera = mActiveScene ? mActiveScene->GetCamera() : DefaultSceneCamera();
        Scope<SceneViewport> viewport = CreateScope<SceneViewport>(NextFreeSceneIndex(), camera);
        SceneViewport* raw = viewport.get();
        AddViewport(std::move(viewport));
        raw->RequestFocus();
        return raw;
    }

    GameViewport* ViewportManager::OpenGameView()
    {
        if(!mGameView)
            AddViewport(CreateScope<GameViewport>());

        mGameView->RequestFocus();
        return mGameView;
    }

    void ViewportManager::AddViewport(Scope<EditorViewport>&& viewport)
    {
        viewport->SetInitialDock(GetNewViewportDock());
        if(!mActiveScene)
            mActiveScene = AsScene(viewport.get());
        if(!mGameView)
            mGameView = AsGame(viewport.get());

        mViewports.push_back(std::move(viewport));
    }

    void ViewportManager::DrawMenuItems()
    {
        if(ImGui::MenuItem("Add Scene View"))
            AddSceneView();
        EditorUI::ItemTooltip("Another free camera view of the scene");
        if(ImGui::MenuItem("Game View", nullptr, mGameView != nullptr))
            OpenGameView();
        EditorUI::ItemTooltip("The view through the scene's primary camera, with the in-game UI");
    }

    void ViewportManager::DockWindows(ImGuiID dockNode)
    {
        // Scene views first so the first tab is a Scene view, then the Game view
        for(const Scope<EditorViewport>& viewport : mViewports)
        {
            if(viewport->GetType() == ViewportType::SCENE)
                ImGui::DockBuilderDockWindow(viewport->GetWindowName().c_str(), dockNode);
        }
        if(mGameView)
            ImGui::DockBuilderDockWindow(mGameView->GetWindowName().c_str(), dockNode);
    }

    void ViewportManager::OnPlay()
    {
        OpenGameView();
    }


    void ViewportManager::OnPlayStop()
    {
        if(mActiveScene)
            mActiveScene->RequestFocus();
    }

    void ViewportManager::OnSceneContextChanged()
    {
        const Scene* scene = static_cast<Editor*>(Core::GetClient())->GetContextScene();
        const AssetID assetID = scene ? scene->GetID() : AssetID(AssetID::INVALID);
        if(assetID && Core::GetAssetManager()->IsRegistered(assetID))
            mSceneName = Filesystem::GetFilenameWithExt(Core::GetAssetManager()->GetMetadata(assetID).RelativePath);
        else
            mSceneName = "Untitled Scene";
    }

    bool ViewportManager::IsSceneViewFocusedOrHovered() const
    {
        for(const Scope<EditorViewport>& viewport : mViewports)
        {
            if(viewport->GetType() == ViewportType::SCENE && (viewport->IsFocused() || viewport->IsHovered()))
                return true;
        }
        return false;
    }

    glm::vec2 ViewportManager::GetGameResolution() const
    {
        if(mGameView && mGameView->GetSize().x > 0.0f)
            return mGameView->GetSize();
        if(mActiveScene && mActiveScene->GetSize().x > 0.0f)
            return mActiveScene->GetSize();
        return { 0.0f, 0.0f };
    }

    Uint ViewportManager::NextFreeSceneIndex() const
    {
        for(Uint index = 1;; index++)
        {
            const bool used = std::any_of(mViewports.begin(), mViewports.end(), [&](const Scope<EditorViewport>& viewport) { return viewport->GetType() == ViewportType::SCENE && viewport->GetIndex() == index; });
            if(!used)
                return index;
        }
    }

    ImGuiID ViewportManager::GetNewViewportDock() const
    {
        // Next to the viewport the user worked in last, then where the single "Viewport" window of older layouts was docked
        const EditorViewport* recent = mActiveScene ? static_cast<const EditorViewport*>(mActiveScene) : static_cast<const EditorViewport*>(mGameView);
        if(recent && recent->GetDockID())
            return recent->GetDockID();

        for(const Scope<EditorViewport>& viewport : mViewports)
        {
            if(viewport->GetDockID())
                return viewport->GetDockID();
        }

        if(const ImGuiWindowSettings* legacy = ImGui::FindWindowSettingsByID(ImHashStr("Viewport")))
            return legacy->DockId;
        return 0;
    }

} // namespace Surge
