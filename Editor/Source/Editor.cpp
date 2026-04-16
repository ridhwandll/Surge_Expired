// Copyright (c) - SurgeTechnologies - All rights reserved
#include <Surge/Surge.hpp>
#include "Editor.hpp"
#include "Utility/ImGuiAux.hpp"
#include "Panels/ViewportPanel.hpp"
#include "Panels/PerformancePanel.hpp"
#include "Panels/SceneHierarchyPanel.hpp"
#include "Panels/InspectorPanel.hpp"
#include "Panels/RenderProcedurePanel.hpp"

namespace Surge
{
    void Editor::OnInitialize()
    {
        ImGui::SetCurrentContext((ImGuiContext*)Core::GetRenderContext()->GetImGuiContext());

        mRenderer = Core::GetRenderer();
        mCamera = EditorCamera(45.0f, 1.778f, 0.1f, 1000.0f);
        mCamera.SetActive(true);

        // Configure panels
        mTitleBar = Titlebar();
        SceneHierarchyPanel* sceneHierarchy;
        sceneHierarchy = mPanelManager.PushPanel<SceneHierarchyPanel>();
        mPanelManager.PushPanel<InspectorPanel>()->SetHierarchy(sceneHierarchy);
        mPanelManager.PushPanel<PerformancePanel>();
        ViewportPanel* viewport = mPanelManager.PushPanel<ViewportPanel>();
        mPanelManager.PushPanel<RenderProcedurePanel>();
        mTitleBar.OnInit();

        mRenderer->SetRenderArea(static_cast<Uint>(viewport->GetViewportSize().x), static_cast<Uint>(viewport->GetViewportSize().y));
        mActiveScene = Ref<Scene>::Create(false);
        mRenderer->SetSceneContext(mActiveScene);
        sceneHierarchy->SetSceneContext(mActiveScene.Raw());
    }

    void Editor::OnUpdate()
    {
        Resize();
        mActiveScene->Update(mCamera);
    }

    void Editor::OnImGuiRender()
    {
        mTitleBar.Render();
        ImGuiAux::DockSpace();
        mPanelManager.RenderAll();
    }

    void Editor::OnEvent(Event& e)
    {
        mCamera.OnEvent(e);
        mPanelManager.OnEvent(e);
    }

    void Editor::OnRuntimeStart()
    {
        //mActiveProject.SetState(ProjectState::Play);
        //mActiveProject.OnRuntimeStart();
        //Ref<Scene> activeScene = mActiveProject.GetActiveScene();
        //mPanelManager.GetPanel<SceneHierarchyPanel>()->SetSceneContext(activeScene.Raw());
        //mRenderer->SetSceneContext(activeScene);
    }

    void Editor::OnRuntimeEnd()
    {
        // mActiveProject.OnRuntimeEnd();
        // mActiveProject.SetState(ProjectState::Edit);
        // Ref<Scene> activeScene = mActiveProject.GetActiveScene();
        // mPanelManager.GetPanel<SceneHierarchyPanel>()->SetSceneContext(activeScene.Raw());
        // mRenderer->SetSceneContext(activeScene);
    }

    void Editor::Resize()
    {
        ViewportPanel* viewportPanel = mPanelManager.GetPanel<ViewportPanel>();
        glm::vec2 viewportSize = viewportPanel->GetViewportSize();
        Ref<Framebuffer> framebuffer = mRenderer->GetFinalPassFramebuffer();

        if (FramebufferSpecification spec = framebuffer->GetSpecification(); viewportSize.x > 0.0f && viewportSize.y > 0.0f && (spec.Width != viewportSize.x || spec.Height != viewportSize.y))
        {
            mRenderer->SetRenderArea((Uint)viewportSize.x, (Uint)viewportSize.y);
            mCamera.SetViewportSize(viewportSize);
            mActiveScene->OnResize(viewportSize.x, viewportSize.y);
        }
    }

    void Editor::OnShutdown()
    {
    }

} // namespace Surge

// Entry point
int main()
{
    Surge::ClientOptions clientOptions;
    clientOptions.EnableImGui = true;
    clientOptions.WindowDescription = {1280, 720, "Surge Editor", Surge::WindowFlags::CreateDefault | Surge::WindowFlags::EditorAcceleration};

    Surge::Editor* app = Surge::MakeClient<Surge::Editor>();
    app->SetOptions(clientOptions);

    Surge::Core::Initialize(app);
    Surge::Core::Run();
    Surge::Core::Shutdown();
}