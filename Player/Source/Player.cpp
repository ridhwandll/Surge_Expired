// Copyright (c) - SurgeTechnologies - All rights reserved
#include <Surge/Surge.hpp>
#include "Player.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include <random>
#include <imgui_internal.h>
#include "stb_image.h"

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

	static glm::vec2 GenRandomPosition(float halfWidth, float halfHeight)
	{
		static std::random_device rd;
		static std::mt19937 gen(rd());
		static std::uniform_real_distribution<float> distX(-halfWidth, halfWidth);
		static std::uniform_real_distribution<float> distY(-halfHeight, halfHeight);
		return glm::vec2(distX(gen), distY(gen));
	}

	static float GenRandomHue()
	{
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<float> hueDist(0.0f, 1.0f);
		float h = hueDist(gen);
		return h;
	}

	void Player::FillTextures()
	{
		stbi_set_flip_vertically_on_load(1);
		for (int i = 0; i < Renderer::MAX_TEXTURE_SLOTS + 300; i++)
		{
			String path = "Engine/Assets/Textures/RidWhite.png";
			int width, height, channels;
			stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
			if (data)
			{
				TextureDesc desc = {};
				desc.Width = width;
				desc.Height = height;
				desc.Format = TextureFormat::RGBA8_SRGB;
				desc.Usage = TextureUsage::SAMPLED | TextureUsage::TRANSFER_DST;
				desc.DebugName = String("QuadTexture_" + std::to_string(i) + ".png");
				desc.InitialData = data;
				desc.DataSize = width * height * 4;
				desc.Sampler = mQuadSampler;
				TextureHandle texture = mRenderer->GetRHI()->CreateTexture(desc);
				mTextures.push_back(texture);

				//if (i >= 1)
				//	mTextures.push_back(mTextures[0]);
				//else
				//{
				//	TextureHandle texture = mRenderer->GetRHI()->CreateTexture(desc);
				//	mTextures.push_back(texture);
				//}
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
		mQuadSampler = mRenderer->GetRHI()->CreateSampler(samplerDesc);

		FillTextures();

		Uint initialQuadCount = 200.0f;
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<float> distX(-halfWidth, halfWidth);
		std::uniform_real_distribution<float> distY(-halfHeight, halfHeight);
		for (Uint i = 0; i < initialQuadCount; i++)
		{
			float x = distX(gen);
			float y = distY(gen);

			Entity& quad = mQuads.emplace_back();
			mActiveScene->CreateEntity(quad, "StressQuad");

			quad.AddComponent<SpriteRenderer>(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), mTextures[i]);

			auto& t = quad.GetComponent<TransformComponent>();
			t.Position = glm::vec3(x, y, 0.0f);
			t.Scale = glm::vec3(0.3f, 0.3f, 1.0f);
		}

 
		mActiveScene->OnResize(windowSize.x, windowSize.y);

		mRenderer->AddImGuiRenderCallback([this]() { OnImGuiRender(); });
	}

	void Player::OnUpdate()
	{
		float dt = Core::GetClock().GetSeconds();
		mActiveScene->Update();
	}

	void Player::OnImGuiRender()
	{
		Clock& clock = Core::GetClock();
		ImGuiID dockspaceID = ImGui::GetID("DockSpace");
		
#ifdef SURGE_PLATFORM_ANDROID
		// On mobile we need a padding, else docking/undocking becomes a nightmare
		float padding = 3.0f;
		ImGuiViewport* viewport = ImGui::GetMainViewport();

		// Create a hidden background window that acts as the "Safe Zone"
		ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + padding, viewport->WorkPos.y + padding));
		ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x - (padding * 2), viewport->WorkSize.y - (padding * 2)));

		ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
			ImGuiWindowFlags_NoBackground;

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

					if (window->DockNode != NULL || window->DockId != 0)
						ImGui::SetWindowDock(window, 0, ImGuiCond_Always);
				}
				ImGui::EndMenuBar();
			}

			ImGui::Text("Total quads: %d\nVertices: %i\n%.1fms (FPS: %.1f)", Core::GetRenderer()->GetQuadCount(), Core::GetRenderer()->GetvertexCount(), clock.GetMilliseconds(), 1 / clock.GetSeconds());
		}
		ImGui::End();
	}

	void Player::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowResizeEvent>([&](WindowResizeEvent& resizeEvent) {
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
		for (auto& texture : mTextures)
			mRenderer->GetRHI()->DestroyTexture(texture);
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