// Copyright (c) - SurgeTechnologies - All rights reserved
#include <Surge/Surge.hpp>
#include "Player.hpp"
#include "Surge/Asset/AssetManager.hpp"

#ifdef SURGE_PLATFORM_ANDROID
#include "Surge/Platform/Android/AndroidApp.hpp"
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <android/asset_manager.h>
#endif


///////////////////////////////////////////////////////////////////////////////////////////////////

//NOTE THIS VS Project is only for builidng and copying via Editor, it will not run from the VS ///

///////////////////////////////////////////////////////////////////////////////////////////////////

namespace Surge
{
    static Path GetProjectPath()
    {
        Path searchDir = "Engine";
        Path foundPath;
        
#ifdef SURGE_PLATFORM_ANDROID
        android_app* app = Android::GAndroidApp;
        AAssetManager* androidAssetMgr = app->activity->assetManager;

        AAssetDir* assetDir = AAssetManager_openDir(androidAssetMgr, searchDir.string().c_str());
        if(assetDir)
        {
            const char* filename = nullptr;
            while((filename = AAssetDir_getNextFileName(assetDir)) != nullptr)
            {
                String fileStr(filename);
                if(fileStr.ends_with(".surgeproj"))
                {
                    foundPath = searchDir / fileStr;
                    break;
                }
            }
            AAssetDir_close(assetDir);
        }
#else
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
#endif

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