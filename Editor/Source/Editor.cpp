// Copyright (c) - SurgeTechnologies - All rights reserved
#include <Surge/Surge.hpp>
#include "Editor.hpp"
#include "Utility/ImGuiAux.hpp"
#include "Panels/ViewportPanel.hpp"
#include "Panels/SceneHierarchyPanel.hpp"
#include "Panels/InspectorPanel.hpp"
#include <stb_image.h>
#include <random>

namespace Surge
{
	void Editor::FillTextures(Uint texCount)
	{
		stbi_set_flip_vertically_on_load(1);
		SamplerHandle defautSampler = mRenderer->GetDefaultSampler();
		for (int i = 0; i < texCount; i++)
		{
			String path = "Engine/Assets/Textures/RidWhite.png";
			int width, height, channels;
			stbi_uc* data = nullptr;
#ifdef SURGE_PLATFORM_WINDOWS
			data = stbi_load(path.c_str(), &width, &height, &channels, 4);
#elif defined(SURGE_PLATFORM_ANDROID)
			android_app* app = Android::GAndroidApp;
			AAssetManager* assetManager = app->activity->assetManager;
			AAsset* asset = AAssetManager_open(assetManager, path.c_str(), AASSET_MODE_BUFFER);

			Vector<unsigned char> buffer;
			int bufferSize = AAsset_getLength(asset);
			buffer.resize(bufferSize);

			AAsset_read(asset, buffer.data(), bufferSize);
			AAsset_close(asset);

			data = stbi_load_from_memory(buffer.data(), bufferSize, &width, &height, &channels, 4);
#endif
			if (data)
			{
				TextureDesc desc = {};
				desc.Width = width;
				desc.Height = height;
				desc.Format = TextureFormat::RGBA8_SRGB;
				desc.Usage = TextureUsage::SAMPLED | TextureUsage::TRANSFER_DST;
				desc.DebugName = String(std::to_string(i + 1) + ".png");
				desc.InitialData = data;
				desc.DataSize = width * height * 4;
				desc.Sampler = defautSampler;
				TextureHandle texture = mRenderer->GetRHI()->CreateTexture(desc);
				mTextures.push_back(texture);
				stbi_image_free(data);
			}
			else
			{
				Log<Severity::Error>("Failed to load texture at path: {0}", path);
			}
		}
	}

    void Editor::OnInitialize()
    {
        mRenderer = Core::GetRenderer();
        mCamera = EditorCamera(45.0f, 1.778f, 0.1f, 1000.0f);
        mCamera.SetActive(true);

        // Configure panels
        SceneHierarchyPanel* sceneHierarchy;
        sceneHierarchy = mPanelManager.PushPanel<SceneHierarchyPanel>();
        mPanelManager.PushPanel<InspectorPanel>()->SetHierarchy(sceneHierarchy);
        mPanelManager.PushPanel<ViewportPanel>(&mCamera);

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

        //{
        //    mActiveScene->CreateEntity(sprite, "Sprite");
        //    SpriteRendererComponent& s = sprite.AddComponent<SpriteRendererComponent>(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        //}

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
		//{
		//	std::array<Entity, 7> generatedMeshEntities;
		//	for (int i = 0; i < 7; i++)
		//	{
		//		mActiveScene->CreateEntity(generatedMeshEntities[i], MeshGenerator::DefaultMeshToString(static_cast<DefaultMesh>(i)));
		//		MeshComponent& meshComp = generatedMeshEntities[i].AddComponent<MeshComponent>();
		//		meshComp.Mesh = Ref<Mesh>::Create(static_cast<DefaultMesh>(i));
		//
		//		TransformComponent& t = generatedMeshEntities[i].GetComponent<TransformComponent>();
		//		t.Position = glm::vec3(-5.0f + i * 2.0f, 1.0f, 0.0f);
		//		t.Scale = glm::vec3(1.0f, 1.0f, 1.0f);
		//		t.MarkDirty();
		//	}
		//}

		//std::random_device rd;
		//std::mt19937 gen(rd());
		//std::uniform_real_distribution<float> distX(-10, 10);
		//std::uniform_real_distribution<float> distY(-10, 10);
		//Uint texturedQuadCount = 50.0f;
		//FillTextures(texturedQuadCount);
		//for (Uint i = 0; i < texturedQuadCount; i++)
		//{
		//	float x = distX(gen);
		//	float y = distY(gen);
		//
		//	Entity quad;
		//	mActiveScene->CreateEntity(quad, "StressQuad");
		//	quad.AddComponent<SpriteRendererComponent>(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), mTextures[i]);
		//
		//	auto& t = quad.GetComponent<TransformComponent>();
		//	t.Position = glm::vec3(x, y, 0.0f);
		//	t.Scale = glm::vec3(0.3f, 0.3f, 1.0f);
		//}

		{
			{
				Entity floor;
				mActiveScene->CreateEntity(floor, MeshGenerator::DefaultMeshToString(DefaultMesh::CUBE));
				MeshComponent& meshComp = floor.AddComponent<MeshComponent>();
				meshComp.Mesh = Ref<Mesh>::Create(DefaultMesh::CUBE);
				TransformComponent& t = floor.GetComponent<TransformComponent>();
				t.Position = glm::vec3(0.0f, 0.0f, 0.0f);
				t.Scale = glm::vec3(10.0f, 1.0f, 10.0f);
				t.MarkDirty();
			}
			{
				Entity cube;
				mActiveScene->CreateEntity(cube, MeshGenerator::DefaultMeshToString(DefaultMesh::SPHERE));
				MeshComponent& meshComp = cube.AddComponent<MeshComponent>();
				meshComp.Mesh = Ref<Mesh>::Create(DefaultMesh::SPHERE);
				TransformComponent& t = cube.GetComponent<TransformComponent>();
				t.Position = glm::vec3(0.0f, 1.0f, 0.0f);
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
			lightComp.Intensity = 0.5f;
			lightComp.Radius = 1.0f;
			TransformComponent& t = directionalLight.GetComponent<TransformComponent>();
			t.Position = glm::vec3(0.0f, 0.0f, 0.0f);
			t.Rotation = glm::vec3(-30.0f, -40.0f, -30.0f);
			t.MarkDirty();
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
		ViewportPanel* viewportPanel = mPanelManager.GetPanel<ViewportPanel>();
		if (viewportPanel->IsViewportHovered())
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
		for (auto& texture : mTextures)
			mRenderer->GetRHI()->DestroyTexture(texture);
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