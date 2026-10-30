// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Player.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Physics/Physics.hpp"
#include "Surge/Graphics/UISystem/UIManager.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////

/// NOTE THIS VS Project is only for builidng and copying via Editor, it will not run from the VS ///

///////////////////////////////////////////////////////////////////////////////////////////////////

namespace Surge
{
    void Player::OpenProject()
    {
        // Written by the Editor's export next to the cooked assets
        const Path projectPath = Path(mAssetManager->GetAssetsDirectory()) / Project::COOKED_FILE_NAME;

        Project openedProject;
        if(!openedProject.LoadCooked(projectPath))
            Log<Severity::Fatal>("[Player] No project at {}, export the game again", projectPath.string());
        mCurrentProject = std::move(openedProject);
        Core::GetPhysics()->ApplySettings(mCurrentProject.Physics);
        mActiveScene = CreateRuntimeScene(mCurrentProject.StartScene);
        if(!mActiveScene)
        {
            // Keep running with the error in the log instead of crashing on a null scene
            Log<Severity::Error>("[Player] Start scene {} could not be loaded ({}), running an empty scene", mCurrentProject.StartScene.Get(), projectPath.string());
            mActiveScene = Ref<Scene>::Create("Empty", Core::GetSceneContext());
        }
    }

    Ref<Scene> Player::CreateRuntimeScene(AssetID sceneID)
    {
        // The game runs a copy: loading the same scene again (Scene.Load) must start from the saved state, not from wherever the last run left it
        Ref<Scene> source = mAssetManager->Load<Scene>(sceneID);
        if(!source)
            return nullptr;

        Ref<Scene> runtime = Ref<Scene>::Create("Dummy", Core::GetSceneContext());
        source->CopyTo(runtime.Raw());
        source.Reset();
        mAssetManager->Unload(sceneID);
        return runtime;
    }

    void Player::ChangeRuntimeScene(AssetID sceneID)
    {
        Ref<Scene> next = CreateRuntimeScene(sceneID);
        if(!next)
        {
            Log<Severity::Error>("[Player] Scene.Load: scene {} could not be loaded, staying in the current one", sceneID.Get());
            return;
        }

        // Ends the old scene: destroying it runs OnDisable/OnDestroy and removes its bodies and canvases.
        // Lua modules and Storage are untouched, they belong to the app session
        mActiveScene->OnRuntimeEnd();
        mActiveScene = next;
        Core::GetRenderer()->GetUIManager().ClearCanvases(); // Roots set with SetUIRoot() by the old scene's scripts

        // The old scene is gone: free what only it used (the next scene already holds its own assets, shared ones stay)
        mAssetManager->UnloadUnreferenced();

        const glm::vec2 size = Core::GetWindow()->GetSize();
        mActiveScene->OnResize(size.x, size.y);
        mActiveScene->OnRuntimeStart();
    }

    void Player::OnInitialize()
    {
#ifdef SURGE_RELEASE
        SetMinLogSeverity(Severity::Warn);
#endif

        mRenderer = Core::GetRenderer();
        mAssetManager = Core::GetAssetManager();

        OpenProject();

        const glm::vec2 size = Core::GetWindow()->GetSize();
        mActiveScene->OnResize(static_cast<float>(size.x), static_cast<float>(size.y));
        //mRenderer->AddImGuiRenderCallback([this]() { OnImGuiRender(); });
        mActiveScene->OnRuntimeStart();
        Resize(INITIAL_WIDTH, INITIAL_HEIGHT);
    }

    void Player::OnUpdate()
    {
        mActiveScene->Update();
    }

    void Player::OnImGuiRender()
    {
    }

    void Player::OnEvent(Event& e)
    {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<WindowResizeEvent>([&](WindowResizeEvent& resizeEvent)
                                               {
                                                   Resize(resizeEvent.GetWidth(), resizeEvent.GetHeight());
                                               });
    }

    void Player::Resize(Uint width, Uint height)
    {
        if(width != 0 && height != 0)
        {
            mActiveScene->OnResize(static_cast<float>(width), static_cast<float>(height));
            const float windowW = Core::GetWindow()->GetSize().x;
            const float windowH = Core::GetWindow()->GetSize().y;
            Core::GetRenderer()->GetUIManager().SetViewportBounds(0.0f, 0.0f, windowW, windowH);
        }
    }

    void Player::OnShutdown()
    {
        mActiveScene->OnRuntimeEnd();
    }

} // namespace Surge
