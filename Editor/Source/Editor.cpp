// Copyright (c) - SurgeTechnologies - All rights reserved
#include <Surge/Surge.hpp>
#include "Editor.hpp"
#include "Utility/ImGuiAux.hpp"
#include "Panels/ViewportPanel.hpp"
#include "Panels/PerformancePanel.hpp"
#include "Panels/SceneHierarchyPanel.hpp"
#include "Panels/InspectorPanel.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <random>

namespace Surge
{
    void Editor::OnInitialize()
    {
        Log<Severity::Info>("{0}", UUID().ToString());
        Log<Severity::Info>("{0}", UUID().ToString());
        Log<Severity::Info>("{0}", UUID().ToString());
        Log<Severity::Info>("{0}", UUID().ToString());
        Log<Severity::Info>("{0}", UUID().ToString());

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
        mTitleBar.OnInit();

        mActiveScene = Ref<Scene>::Create(false);
        //mRenderer->SetSceneContext(mActiveScene);
        sceneHierarchy->SetSceneContext(mActiveScene.Raw());
        
        Entity runtimeCamera;
        Entity dirLight;
        Entity pointLight;
        Entity floor;
        Entity cube;
        {
            mActiveScene->CreateEntity(runtimeCamera, "Runtime Camera");
            CameraComponent& cam = runtimeCamera.AddComponent<CameraComponent>();
            cam.Primary = true;
            TransformComponent& transform = runtimeCamera.GetComponent<TransformComponent>();
            transform.Position = glm::vec3(0, 0, -10);
        }
        {
            mActiveScene->CreateEntity(dirLight, "Directional Light");
            DirectionalLightComponent& d = dirLight.AddComponent<DirectionalLightComponent>();
            d.Intensity = 4;

            TransformComponent& transform = dirLight.GetComponent<TransformComponent>();
            transform.Rotation = glm::vec3(30, -60, -80);
        }
        {
            mActiveScene->CreateEntity(pointLight, "Point Light");
            PointLightComponent& p = pointLight.AddComponent<PointLightComponent>();
            p.Color = glm::vec3(0.8f, 0.0f, 1.0f);
            p.Intensity = 8;
            p.Radius = 5;

            TransformComponent& transform = pointLight.GetComponent<TransformComponent>();
            transform.Position = glm::vec3(-1, 1, 1);
        }
        mRenderer->AddImGuiRenderCallback([this]() { OnImGuiRender(); });
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
    }

    void Editor::OnRuntimeEnd()
    {
    }

    void Editor::Resize()
    {
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