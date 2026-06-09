// Copyright (c) - SurgeTechnologies - All rights reserved
#include <Surge/Surge.hpp>
#include "Player.hpp"
#include "Surge/Asset/AssetManager.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////

//NOTE THIS VS Project is only for builidng and copying via Editor, it will not run from the VS ///

///////////////////////////////////////////////////////////////////////////////////////////////////

namespace Surge
{
    static Path GetProjectPath()
    {
        // .surgeproj file is located in the Engine folder of the root of shipped .exe
        Path searchDir = "Engine";
        Path foundPath;

        if(Filesystem::Exists(searchDir) && std::filesystem::is_directory(searchDir))
        {
            for(const auto& entry : std::filesystem::directory_iterator(searchDir))
            {
                if(entry.is_regular_file() && entry.path().extension() == ".surgeproj")
                {
                    foundPath = entry.path();
                    break; // Stop at the very first match
                }
            }
        }
        if(foundPath.empty())
            Log<Severity::Fatal>("No .surgeproj file found!");

        return foundPath;
    }

    void Player::OpenProject()
    {
        const Path projectPath = GetProjectPath();

        Project openedProject;
        Serializer::DeserializeProject(projectPath, &openedProject);

        const Path assetManagerPath = Filesystem::GetParentPath(projectPath) / "Assets";
        mAssetManager->Shutdown();
        mAssetManager->Initialize(assetManagerPath);

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
        if (width != 0 && height != 0)
            mActiveScene->OnResize(static_cast<float>(width), static_cast<float>(height));
    }

    void Player::OnShutdown()
    {
    }

} // namespace Surge

#ifndef SURGE_PLATFORM_ANDROID
// Entry point
int main()
{
    Surge::ClientOptions clientOptions;
    clientOptions.RenderFinalImageToSwapchian = true;
    clientOptions.WindowDescription = { 1280, 720, "Runtime", Surge::WindowFlags::CreateDefault /*| Surge::WindowFlags::NoTitlebar*/ };

    Surge::Player* app = Surge::MakeClient<Surge::Player>();
    app->SetOptions(clientOptions);

    Surge::Core::Initialize(app);
    Surge::Core::Run();
    Surge::Core::Shutdown();
}
#endif