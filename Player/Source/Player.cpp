// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Player.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Graphics/UISystem/UIManager.hpp"
#include "Surge/Serializer/Serializer.hpp"
#include "Surge/Utility/Filesystem.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////

/// NOTE THIS VS Project is only for builidng and copying via Editor, it will not run from the VS ///

///////////////////////////////////////////////////////////////////////////////////////////////////

namespace Surge
{
    static Path GetProjectPath()
    {
        Path searchDir = "Engine";
        Path foundPath;

        Vector<Path> files = Filesystem::GetFilesInDirectory("Engine", ".surgeproj");

        if(files.empty())
            Log<Severity::Fatal>("No .surgeproj file found!");

        return files.front();
    }

    void Player::OpenProject()
    {
        const Path projectPath = GetProjectPath();

        Project openedProject;
        Serializer::DeserializeProject(projectPath, &openedProject);
        mActiveScene = mAssetManager->Load<Scene>(openedProject.StartScene);
        mCurrentProject = std::move(openedProject);
    }

    void Player::OnInitialize()
    {
        mRenderer = Core::GetRenderer();
        mAssetManager = Core::GetAssetManager();
        mRenderer->ShowInternalImGui(false);

        OpenProject();

        const glm::vec2 size = Core::GetWindow()->GetSize();
        mActiveScene->OnResize(static_cast<float>(size.x), static_cast<float>(size.y));
        //mRenderer->AddImGuiRenderCallback([this]() { OnImGuiRender(); });
        mActiveScene->OnRuntimeStart();
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

#ifndef SURGE_PLATFORM_ANDROID
// Entry point
int main()
{
    Surge::ClientOptions clientOptions;
    clientOptions.EnableImGui = false;
    clientOptions.RenderFinalImageToSwapchian = true;
    clientOptions.WindowDescription = { 1280, 720, "Runtime", Surge::WindowFlags::DEFAULT };

    Surge::Player* app = Surge::MakeClient<Surge::Player>();
    app->SetOptions(clientOptions);

    Surge::Core::Initialize(app);
    Surge::Core::Run();
    Surge::Core::Shutdown();
}
#endif