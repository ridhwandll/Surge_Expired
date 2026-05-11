// Copyright (c) - SurgeTechnologies - All rights reserved
#include <Surge/Surge.hpp>
#include "Player.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include <random>
#include <imgui_internal.h>
#include "stb_image.h"

#ifdef SURGE_PLATFORM_ANDROID
#include "Surge/Platform/Android/AndroidApp.hpp"
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <android/asset_manager.h>
#endif


namespace Surge
{
	static glm::vec3 HSVtoRGB(float h, float s, float v)
	{
		float c = v * s;
		float x = c * (1.0f - fabs(fmod(h * 6.0f, 2.0f) - 1.0f));
		float m = v - c;

		glm::vec3 rgb;

		if (h < 1.0f / 6.0f) rgb = { c, x, 0 };
		else if (h < 2.0f / 6.0f) rgb = { x, c, 0 };
		else if (h < 3.0f / 6.0f) rgb = { 0, c, x };
		else if (h < 4.0f / 6.0f) rgb = { 0, x, c };
		else if (h < 5.0f / 6.0f) rgb = { x, 0, c };
		else rgb = { c, 0, x };

		return rgb + glm::vec3(m);
	}

	static float GenRandomHue()
	{
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<float> hueDist(0.0f, 1.0f);
		float h = hueDist(gen);
		return h;
	}

	static glm::vec2 GenRandomPosition(float halfWidth, float halfHeight)
	{
		static std::random_device rd;
		static std::mt19937 gen(rd());
		static std::uniform_real_distribution<float> distX(-halfWidth, halfWidth);
		static std::uniform_real_distribution<float> distY(-halfHeight, halfHeight);
		return glm::vec2(distX(gen), distY(gen));
	}

	void Player::FillTextures(Uint texCount)
	{
		stbi_set_flip_vertically_on_load(1);
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
				desc.Sampler = mQuadSampler;
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

	void Player::OnInitialize()
	{
		mRenderer = Core::GetRenderer();
		mActiveScene = Ref<Scene>::Create(false);
		Entity runtimeCamera;

		glm::vec2 windowSize = Core::GetWindow()->GetSize();
		float halfWidth = 0;
		float halfHeight = 0;

		{
			mActiveScene->CreateEntity(runtimeCamera, "Runtime Camera");
			CameraComponent& cam = runtimeCamera.AddComponent<CameraComponent>();
			cam.Primary = true;
			cam.FixedAspectRatio = true;

			cam.Camera.SetProjectionType(RuntimeCamera::ProjectionType::Orthographic);
			TransformComponent& transform = runtimeCamera.GetComponent<TransformComponent>();
			transform.Position = glm::vec3(0, 0, 0);
			transform.Rotation = glm::vec3(0, 0, 0);

			cam.Camera.SetViewportSize(windowSize.x, windowSize.y);
			float size = cam.Camera.GetOrthographicSize();
			float aspect = cam.Camera.GetAspectRatio();
			halfWidth = size * aspect * 0.5f;
			halfHeight = size * 0.5f;
		}

		SamplerDesc samplerDesc = {};
		samplerDesc.DebugName = "PlayerTexture sampler";
		mQuadSampler = mRenderer->GetRHI()->CreateSampler(samplerDesc);

		mTexturedQuadCount = 500.0f;
		mChangeQuadAmount = mTexturedQuadCount;
		FillTextures(mTexturedQuadCount);
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<float> distX(-halfWidth, halfWidth);
		std::uniform_real_distribution<float> distY(-halfHeight, halfHeight);
		for (Uint i = 0; i < mTexturedQuadCount; i++)
		{
			float x = distX(gen);
			float y = distY(gen);
		
			Entity quad;
			mActiveScene->CreateEntity(quad, "StressQuad");
			quad.AddComponent<SpriteRenderer>(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), mTextures[i]);
		
			auto& t = quad.GetComponent<TransformComponent>();
			t.Position = glm::vec3(x, y, 0.0f);
			t.Scale = glm::vec3(0.3f, 0.3f, 1.0f);
		}
		
		Uint basicQuadCount = 500;
		for (Uint i = 0; i < basicQuadCount; i++)
		{
			float x = distX(gen);
			float y = distY(gen);
			Entity& quad = mColoredQuads.emplace_back();
			mActiveScene->CreateEntity(quad, "StressQuad");
			quad.AddComponent<SpriteRenderer>(glm::vec4(1.0f, 0.79f, 0.0f, 1.0f), TextureHandle::Invalid());
			auto& t = quad.GetComponent<TransformComponent>();
			t.Position = glm::vec3(x, y, 0.0f);
			t.Scale = glm::vec3(0.02f, 0.02f, 1.0f);
		} 
		mActiveScene->OnResize(windowSize.x, windowSize.y);
		mRenderer->AddImGuiRenderCallback([this]() { OnImGuiRender(); });
	}

	void Player::OnUpdate()
	{
		float dt = Core::GetClock().GetSeconds();

		if (mMoveEnabled && mColoredQuads.size() > 0)
		{
			for (Uint i = 0; i < mColoredQuads.size(); i++)
			{
				TransformComponent& transform = mColoredQuads[i].GetComponent<TransformComponent>();
		
				float rotSpeed = 10.0f + (i % 15);
				float moveSpeed = 100.0f;
				float dir = (i % 3 == 0) ? -1.0f : 1.0f;
		
				transform.Rotation.z += dir * rotSpeed * dt;
		
				transform.Position.x += sin(dt + i) * 0.001f * dt * moveSpeed;
				transform.Position.y += cos(dt + i * 0.5f) * 0.001f * dt * moveSpeed;
			}
		}

		mActiveScene->Update();
	}

	void Player::OnImGuiRender()
	{
		Clock& clock = Core::GetClock();
		ImGuiID dockspaceID = ImGui::GetID("DockSpace");
		
#ifdef SURGE_PLATFORM_ANDROID
		// On mobile we need a padding, else docking/undocking becomes a nightmare
		float padding = 2.0f;
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + padding, viewport->WorkPos.y + padding));
		ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x - (padding * 2), viewport->WorkSize.y - (padding * 2)));
		ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
		ImGui::Begin("SafeDockSpaceHost", nullptr, hostFlags);
		ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
		ImGui::End();
		ImGui::PopStyleColor();
#elif defined(SURGE_PLATFORM_WINDOWS)
		ImGui::DockSpaceOverViewport(dockspaceID, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
#endif

		ImGui::Begin("Control & Stats");
		{
			if (ImGui::BeginMenuBar())
			{
				if (ImGui::Button("Undock"))
				{
					ImGuiContext& g = *GImGui;
					ImGuiWindow* window = g.CurrentWindow;

					if (window->DockNode != nullptr || window->DockId != 0)
						ImGui::SetWindowDock(window, 0, ImGuiCond_Always);
				}
				ImGui::EndMenuBar();
			}

			ImGui::Text("Vertices: %i\n%.1fms (FPS: %.1f)", Core::GetRenderer()->GetvertexCount(), clock.GetMilliseconds(), 1 / clock.GetSeconds());

			ImGui::Text("Textured Quads: %d", mTexturedQuadCount);
			ImGui::Text("Non-Textured Quads: %d", mColoredQuads.size());
			ImGui::Text("Total Quads: %d", Core::GetRenderer()->GetQuadCount());
			ImGui::Checkbox("Move quads", &mMoveEnabled);
			if (ImGui::SliderInt("ColorQuads", &mChangeQuadAmount, mTexturedQuadCount, Renderer::MAX_QUADS_TOTAL))
			{
				RuntimeCamera* cam = mActiveScene->GetMainCameraEntity().Data1;
				float size = cam->GetOrthographicSize();
				float aspect = cam->GetAspectRatio();
				float halfWidth = size * aspect * 0.5f;
				float halfHeight = size * 0.5f;

				Uint currentQuadCount = Core::GetRenderer()->GetQuadCount();
				if (currentQuadCount > mChangeQuadAmount)
				{
					Uint toRemove = currentQuadCount - mChangeQuadAmount;
					for (Uint i = 0; i < toRemove; i++)
					{
						mActiveScene->DestroyEntity(mColoredQuads.back());
						mColoredQuads.pop_back();
					}
				}
				else if (currentQuadCount < mChangeQuadAmount)
				{
					Uint toAdd = mChangeQuadAmount - currentQuadCount;
					for (Uint i = 0; i < toAdd; i++)
					{
						glm::vec2 pos = GenRandomPosition(halfWidth, halfHeight);
						Entity& quad = mColoredQuads.emplace_back();
						mActiveScene->CreateEntity(quad, "StressQuad");

						float h = GenRandomHue();
						float s = 1.0f;
						float v = 1.0f;
						glm::vec3 rgb = HSVtoRGB(h, s, v);
						quad.AddComponent<SpriteRenderer>(rgb, 1.0f);

						auto& t = quad.GetComponent<TransformComponent>();
						t.Position = glm::vec3(pos.x, pos.y, 0.0f);
						t.Scale = glm::vec3(0.08f, 0.08f, 1.0f);
					}
				}
			}


		}
		ImGui::End();
	}

	void Player::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowResizeEvent>([&](WindowResizeEvent& resizeEvent) {
				Resize(resizeEvent.GetWidth(), resizeEvent.GetHeight());
			});
		dispatcher.Dispatch<KeyPressedEvent>([&](KeyPressedEvent& e) {
				Log<Severity::Debug>(e.ToString());
			});
		dispatcher.Dispatch<KeyReleasedEvent>([&](KeyReleasedEvent& e) {
				Log<Severity::Debug>(e.ToString());
			});
		dispatcher.Dispatch<KeyTypedEvent>([&](KeyTypedEvent& e) {
				Log<Severity::Debug>(e.ToString());
			});
		dispatcher.Dispatch<MouseButtonPressedEvent>([&](MouseButtonPressedEvent& e) {
				Log<Severity::Debug>(e.ToString());
			});
		dispatcher.Dispatch<MouseButtonReleasedEvent>([&](MouseButtonReleasedEvent& e) {
				Log<Severity::Debug>(e.ToString());
			});
		dispatcher.Dispatch<MouseMovedEvent>([&](MouseMovedEvent& e) {
				Log<Severity::Debug>(e.ToString());
			});
		dispatcher.Dispatch<MouseScrolledEvent>([&](MouseScrolledEvent& e) {
				Log<Severity::Debug>(e.ToString());
			});
	}

	void Player::Resize(Uint width, Uint height)
	{
		if (width != 0 && height != 0)		
			mActiveScene->OnResize(static_cast<float>(width), static_cast<float>(height));		
	}

	void Player::OnShutdown()
	{
		for (auto& texture : mTextures)
			mRenderer->GetRHI()->DestroyTexture(texture);

		mRenderer->GetRHI()->DestroySampler(mQuadSampler);
	}

} // namespace Surge

#ifndef SURGE_PLATFORM_ANDROID
// Entry point
int main()
{
	Surge::ClientOptions clientOptions;
	clientOptions.EnableImGui = false;
	clientOptions.WindowDescription = { 1280, 720, "Player", Surge::WindowFlags::CreateDefault /*| Surge::WindowFlags::EditorAcceleration*/ };

	Surge::Player* app = Surge::MakeClient<Surge::Player>();
	app->SetOptions(clientOptions);

	Surge::Core::Initialize(app);
	Surge::Core::Run();
	Surge::Core::Shutdown();
}
#endif