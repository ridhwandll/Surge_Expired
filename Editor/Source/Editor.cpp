// Copyright (c) - SurgeTechnologies - All rights reserved
#include <Surge/Surge.hpp>
#include "Editor.hpp"
#include "Utility/ImGuiAux.hpp"
#include "Panels/ViewportPanel.hpp"
#include "Panels/SceneHierarchyPanel.hpp"
#include "Panels/InspectorPanel.hpp"

namespace Surge
{
    void Editor::OnInitialize()
    {
        mRenderer = Core::GetRenderer();
        mCamera = EditorCamera(45.0f, 1.778f, 0.1f, 1000.0f);
        mCamera.SetActive(true);

        // Configure panels
        SceneHierarchyPanel* sceneHierarchy;
        sceneHierarchy = mPanelManager.PushPanel<SceneHierarchyPanel>();
        mPanelManager.PushPanel<InspectorPanel>()->SetHierarchy(sceneHierarchy);
        mPanelManager.PushPanel<ViewportPanel>();

        mActiveScene = Ref<Scene>::Create(false);
        //mRenderer->SetSceneContext(mActiveScene);
        sceneHierarchy->SetSceneContext(mActiveScene.Raw());
        
        Entity runtimeCamera;
        Entity sprite;
        Entity cube;
        {
            mActiveScene->CreateEntity(runtimeCamera, "Runtime Camera");
            CameraComponent& cam = runtimeCamera.AddComponent<CameraComponent>();
            cam.Primary = true;
            TransformComponent& transform = runtimeCamera.GetComponent<TransformComponent>();
			transform.Position = glm::vec3(-10, 6, 10);
			transform.Rotation = glm::vec3(-30, -45, 0);
        }
        {
            mActiveScene->CreateEntity(sprite, "Sprite");
            SpriteRendererComponent& s = sprite.AddComponent<SpriteRendererComponent>(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        }

        // Load meshes from files
		//{
		//	mActiveScene->CreateEntity(cube, "Mesh");
		//	MeshComponent& meshComp = cube.AddComponent<MeshComponent>();
		//	//meshComp.Mesh = Ref<Mesh>::Create("Engine/Assets/Mesh/Box.gltf");
		//	//meshComp.Mesh = Ref<Mesh>::Create("Engine/Assets/Mesh/ABeautifulGame/glTF/ABeautifulGame.gltf");
        //
		//	auto& t = cube.GetComponent<TransformComponent>();
		//	t.Position = glm::vec3(0.0f, 0.0f, 0.0f);
		//	t.Scale = glm::vec3(1.0f, 1.0f, 1.0f);
		//	t.MarkDirty();
		//}

        // Load in-engine generated meshes
		{
			std::array<Entity, 7> generatedMeshEntities;
			for (int i = 0; i < 7; i++)
			{
				mActiveScene->CreateEntity(generatedMeshEntities[i], MeshGenerator::DefaultMeshToString(static_cast<DefaultMesh>(i)));
				MeshComponent& meshComp = generatedMeshEntities[i].AddComponent<MeshComponent>();
				meshComp.Mesh = Ref<Mesh>::Create(static_cast<DefaultMesh>(i));

				TransformComponent& t = generatedMeshEntities[i].GetComponent<TransformComponent>();
				t.Position = glm::vec3(-5.0f + i * 2.0f, 1.0f, 0.0f);
				t.Scale = glm::vec3(1.0f, 1.0f, 1.0f);
				t.MarkDirty();
			}
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
        ImGuiAux::DockSpace();
        mPanelManager.RenderAll();
    }

    void Editor::OnEvent(Event& e)
    {
		if (e.GetEventType() == EventType::MouseMoved)
        {
			ViewportPanel* viewportPanel = mPanelManager.GetPanel<ViewportPanel>();
			if (viewportPanel->IsViewportHovered())
				mCamera.OnEvent(e);
        }
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
		ViewportPanel* viewportPanel = mPanelManager.GetPanel<ViewportPanel>();
        Scope<GraphicsRHI>& rhi = mRenderer->GetRHI();

		glm::vec2 viewportSize = viewportPanel->GetViewportSize();
		FramebufferHandle fbHandle = mRenderer->GetFinalFramebuffer();
        FramebufferDesc desc = rhi->GetDesc(fbHandle);

		if (viewportSize.x > 0.0f && viewportSize.y > 0.0f && (desc.Width != viewportSize.x || desc.Height != viewportSize.y))
		{
			rhi->WaitIdle(); // Ensure GPU is not using the framebuffer before resizing
			mCamera.SetViewportSize(viewportSize);
			rhi->ResizeFramebuffer(fbHandle, (Uint)viewportSize.x, (Uint)viewportSize.y);
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
    clientOptions.RenderFinalImageToSwapchian = false; // We grab the imgui image id from renderer
    clientOptions.WindowDescription = {1280, 720, "Surge Editor", Surge::WindowFlags::CreateDefault};

    Surge::Editor* app = Surge::MakeClient<Surge::Editor>();
    app->SetOptions(clientOptions);

    Surge::Core::Initialize(app);
    Surge::Core::Run();
    Surge::Core::Shutdown();
}