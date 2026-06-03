// Copyright (c) - SurgeTechnologies - All rights reserved
#include <Surge/Surge.hpp>
#include "Editor.hpp"
#include "Surge/Asset/AssetManager.hpp"

#include "Utility/ImGuiAux.hpp"
#include "Panels/ViewportPanel.hpp"
#include "Panels/SceneHierarchyPanel.hpp"
#include "Panels/InspectorPanel.hpp"
#include "SurgeReflect/Enum.hpp"
#include "Surge/Asset/DefaultMeshes.hpp"

namespace Surge
{
    void Editor::OnInitialize()
    {
        mRenderer = Core::GetRenderer();
        mRenderer->SetOutlineThickness(1);

        mCamera = EditorCamera(45.0f, 1.778f, 0.1f, 1000.0f);
        mCamera.SetActive(true);

        // Configure panels
        SceneHierarchyPanel* sceneHierarchy = mPanelManager.PushPanel<SceneHierarchyPanel>();
        mPanelManager.PushPanel<InspectorPanel>()->SetHierarchy(sceneHierarchy);
        mPanelManager.PushPanel<ViewportPanel>(&mCamera);

        mActiveScene = Ref<Scene>::Create();
        sceneHierarchy->SetSceneContext(mActiveScene.Raw());
        
        Entity runtimeCamera;
        {
            mActiveScene->CreateEntity(runtimeCamera, "Runtime Camera");
            CameraComponent& cam = runtimeCamera.AddComponent<CameraComponent>();
            cam.Primary = true;
            TransformComponent& transform = runtimeCamera.GetComponent<TransformComponent>();
            transform.Position = glm::vec3(-10, 6, 10);
            transform.Rotation = glm::vec3(-30, -45, 0);
        }

        AssetID assetID = AssetManager::Import("Textures/RidWhite.png", AssetType::TEXTURE2D);
        mRidTex = AssetManager::Load<Texture2D>(assetID) ;

        {
            {
                Entity floor;
                mActiveScene->CreateEntity(floor, "Cube");
                MeshComponent& meshComp = floor.AddComponent<MeshComponent>();
                meshComp.MeshID = AssetManager::Import(DefaultMesh::CUBE, AssetType::MESH);

                TransformComponent& t = floor.GetComponent<TransformComponent>();
                t.Position = glm::vec3(0.0f, 0.0f, 0.0f);
                t.Scale = glm::vec3(10.0f, 1.0f, 10.0f);
                t.MarkDirty();
            }
            {
                Entity sphere;
                mActiveScene->CreateEntity(sphere, "Sphere");
                MeshComponent& meshComp = sphere.AddComponent<MeshComponent>();
                meshComp.MeshID = AssetManager::Import(DefaultMesh::SPHERE, AssetType::MESH);
            
                TransformComponent& t = sphere.GetComponent<TransformComponent>();
                t.Position = glm::vec3(0.0f, 2.0f, 0.0f);
                t.Scale = glm::vec3(1.0f, 1.0f, 1.0f);
                t.MarkDirty();
            }
            {
                Entity e;
                mActiveScene->CreateEntity(e, "Vulkan Scene");
                MeshComponent& meshComp = e.AddComponent<MeshComponent>();
                meshComp.MeshID = AssetManager::Import("Mesh/VulkanScene.glb", AssetType::MESH);

                TransformComponent& t = e.GetComponent<TransformComponent>();
                t.Position = glm::vec3(2.0f, 1.7f, 1.0f);
                t.Scale = glm::vec3(1.0f, 1.0f, 1.0f);
                t.MarkDirty();
            }
        }
        {
            Entity pointLight;
            mActiveScene->CreateEntity(pointLight, "Point Light");
            LightComponent& lightComp = pointLight.AddComponent<LightComponent>();
            lightComp.Type = LightType::POINT;
            lightComp.Intensity = 1.2f;
            lightComp.Radius = 10.0f;
            TransformComponent& t = pointLight.GetComponent<TransformComponent>();
            t.Position = glm::vec3(1.0f, 2.0f, 1.0f);
            t.MarkDirty();
        }
        {
            Entity directionalLight;
            mActiveScene->CreateEntity(directionalLight, "Directional Light");
            LightComponent& lightComp = directionalLight.AddComponent<LightComponent>();
            lightComp.Type = LightType::DIRECTIONAL;
            lightComp.Intensity = 4.5f;
            TransformComponent& t = directionalLight.GetComponent<TransformComponent>();
            t.Position = glm::vec3(0.0f, 0.0f, 0.0f);
            t.Rotation = glm::vec3(30.0f, -30.0f, 30.0f);
            t.MarkDirty();
        }

        mRenderer->AddImGuiRenderCallback([this]() { OnImGuiRender(); });
    }

    void Editor::OnUpdate()
    {
        // Axes
        constexpr float axesLength = 10000.0f;
        mRenderer->SubmitLine({ -axesLength, 0.0f, 0.0f }, { axesLength, 0.0f, 0.0f }, { 1.0f, 0.3f, 0.3f, 1.0f }); // X
        mRenderer->SubmitLine({ 0.0f, -axesLength, 0.0f }, { 0.0f, axesLength, 0.0f }, { 0.3f, 0.8f, 0.3f, 1.0f }); // Y
        mRenderer->SubmitLine({ 0.0f, 0.0f, -axesLength }, { 0.0f, 0.0f, axesLength }, { 0.3f, 0.3f, 1.0f, 1.0f }); // Z

        CheckResize();
        if (mShowRuntimeView && mActiveScene->GetMainCameraEntity().Data1)
            mActiveScene->Update();
        else
            mActiveScene->Update(mCamera);
    }

    void Editor::OnImGuiRender()
    {
        ImGuiAux::DockSpace();
        mPanelManager.RenderAll();
    }

    void Editor::OnEvent(Event& e)
    {
        ViewportPanel* viewportPanel = mPanelManager.GetPanel<ViewportPanel>();
        if (viewportPanel->IsViewportHovered())
            mCamera.OnEvent(e);

        mPanelManager.OnEvent(e);

        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<KeyPressedEvent>([&](KeyPressedEvent& keyEvent)
                                             {
                                                 if(keyEvent.GetKeyCode() == Key::F5)
                                                     mShowRuntimeView = !mShowRuntimeView;
                                             });
    }

    void Editor::OnRuntimeStart()
    {
    }

    void Editor::OnRuntimeEnd()
    {
    }

    void Editor::CheckResize()
    {
        ViewportPanel* viewportPanel = mPanelManager.GetPanel<ViewportPanel>();
        Scope<GraphicsRHI>& rhi = mRenderer->GetRHI();

        glm::vec2 viewportSize = viewportPanel->GetViewportSize();
        FramebufferHandle fbHandle = mRenderer->GetFinalFramebuffer();
        FramebufferDesc desc = rhi->GetDesc(fbHandle);

        if (viewportSize.x > 0.0f && viewportSize.y > 0.0f && (desc.Width != viewportSize.x || desc.Height != viewportSize.y))
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