// Copyright (c) - SurgeTechnologies - All rights reserved
#include <Surge/Surge.hpp>
#include "Editor.hpp"
#include "Surge/Asset/AssetManager.hpp"

#include "Utility/ImGuiAux.hpp"
#include "Panels/ViewportPanel.hpp"
#include "Panels/SceneHierarchyPanel.hpp"
#include "Panels/InspectorPanel.hpp"
#include "Panels/ContentBrowserPanel.hpp"

namespace Surge
{
    void Editor::OnInitialize()
    {
        mActiveScene = Ref<Scene>::Create();

        mRenderer = Core::GetRenderer();
        mRenderer->SetOutlineThickness(1);

        mCamera = EditorCamera(45.0f, 1.778f, 0.1f, 1000.0f);
        mCamera.SetActive(true);

        // Configure panels
        SceneHierarchyPanel* sceneHierarchy = mPanelManager.PushPanel<SceneHierarchyPanel>();
        sceneHierarchy->SetSceneContext(mActiveScene.Raw());

        mPanelManager.PushPanel<InspectorPanel>()->SetHierarchy(sceneHierarchy);
        mViewportPanel = mPanelManager.PushPanel<ViewportPanel>(&mCamera);
        mPanelManager.PushPanel<ContentBrowserPanel>();

        mProjectBrowser.Init();

        mRenderer->AddImGuiRenderCallback([this]() { OnImGuiRender(); });
    }

    void Editor::OnUpdate()
    {
        if(mCurrentProject.IsValid())
        {
            // Axes
            constexpr float axesLength = 10000.0f;
            if (mShowAxes)
            {
                mRenderer->SubmitLine({ -axesLength, 0.0f, 0.0f }, { axesLength, 0.0f, 0.0f }, { 1.0f, 0.3f, 0.3f, 1.0f }); // X
                mRenderer->SubmitLine({ 0.0f, -axesLength, 0.0f }, { 0.0f, axesLength, 0.0f }, { 0.3f, 0.8f, 0.3f, 1.0f }); // Y
                mRenderer->SubmitLine({ 0.0f, 0.0f, -axesLength }, { 0.0f, 0.0f, axesLength }, { 0.3f, 0.3f, 1.0f, 1.0f }); // Z
            }

            CheckResize();
            if(mShowRuntimeView && mActiveScene->GetMainCameraEntity().Data1)
                mActiveScene->Update();
            else
            {
                mCamera.OnUpdate(mViewportPanel->IsViewportHovered());
                mActiveScene->Update(mCamera);
            }
        }
        else
        {
            // (Rid) We have to do this to render the ImGUI! Is this a design flaw? Maybe. But it works for now and we can refactor later if needed
            mActiveScene->Update(mCamera);
        }
    }

    void Editor::OnImGuiRender()
    {
        if(mCurrentProject.IsValid())
        {
            mRenderer->ShowInternalImGui(true);
            ImGuiAux::DockSpace();
            mPanelManager.RenderPanels();
            RenderEditorSettings();
        }
        else
        {
            mRenderer->ShowInternalImGui(false);
            mProjectBrowser.Render();
        }
    }

    void Editor::RenderEditorSettings()
    {
        ImGui::Begin("Editor Settings");
        if (ImGuiAux::Button("Save Scene (F2)")) { AssetManager::Save(mActiveScene->GetID()); }
        ImGui::Checkbox("Show Runtime View", &mShowRuntimeView);
        ImGui::Checkbox("Show Axes", &mShowAxes);
        ImGui::End();
    }

    void Editor::OnEvent(Event& e)
    {
        if (mViewportPanel->IsViewportHovered())
            mCamera.OnEvent(e);

        mPanelManager.OnEvent(e);

        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<KeyPressedEvent>([&](KeyPressedEvent& keyEvent)
                                             {
                                                 if(keyEvent.GetKeyCode() == Key::F2)
                                                 {
                                                     AssetManager::Save(mActiveScene->GetID());
                                                 }
                                             });
    }

    void Editor::OnRuntimeStart()
    {
    }

    void Editor::OnRuntimeEnd()
    {
    }

    void Editor::LoadScene(Ref<Scene> scene)
    {
        mPanelManager.GetPanel<SceneHierarchyPanel>()->SetSceneContext(scene.Raw());
        mPanelManager.GetPanel<ViewportPanel>()->OnSceneContextChanged();
        mActiveScene = scene;
    }

    void Editor::CheckResize()
    {
        ViewportPanel* viewportPanel = mPanelManager.GetPanel<ViewportPanel>();
        Scope<GraphicsRHI>& rhi = mRenderer->GetRHI();

        glm::vec2 viewportSize = viewportPanel->GetViewportSize();
        FramebufferHandle fbHandle = mRenderer->GetFinalFramebuffer();
        FramebufferDesc desc = rhi->GetDesc(fbHandle);

        if (viewportSize.x > 0.0f && viewportSize.y > 0.0f && (desc.Width != (Uint)viewportSize.x || desc.Height != (Uint)viewportSize.y))
        {
            rhi->WaitIdle();

            mCamera.SetViewportSize(viewportSize);
            mRenderer->ForceResize((Uint)viewportSize.x, (Uint)viewportSize.y);

            mActiveScene->OnResize(viewportSize.x, viewportSize.y);
        }
    }

    void Editor::OnShutdown() {}

} // namespace Surge

// Entry point
int main()
{
    Surge::ClientOptions clientOptions;
    clientOptions.RenderFinalImageToSwapchian = false; // We grab the imgui image id from renderer
    clientOptions.WindowDescription = {1280, 720, "Surge Editor", Surge::WindowFlags::CreateDefault};

    Surge::Editor* app = Surge::MakeClient<Surge::Editor>();
    app->SetOptions(clientOptions);

    Surge::Core::Initialize(app);
    Surge::Core::Run();
    Surge::Core::Shutdown();
}