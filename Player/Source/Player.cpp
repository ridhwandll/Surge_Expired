// Copyright (c) - SurgeTechnologies - All rights reserved
#include <Surge/Surge.hpp>
#include "Player.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include <random>

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
	
		// Stress test		
		Uint initialQuadCount = Renderer::MAX_QUADS;
		initialQuadCount = 20.0f;
 		mQuads.resize(initialQuadCount);
 		std::random_device rd;
 		std::mt19937 gen(rd());
 		std::uniform_real_distribution<float> distX(-halfWidth, halfWidth);
 		std::uniform_real_distribution<float> distY(-halfHeight, halfHeight);
 		for (Uint i = 0; i < initialQuadCount; i++)
 		{
 			float x = distX(gen);
 			float y = distY(gen);
 		
 			mActiveScene->CreateEntity(mQuads[i], "StressQuad");
 		
 			std::uniform_real_distribution<float> hueDist(0.0f, 1.0f);
 			float h = hueDist(gen);
 			float s = 1.0f;
 			float v = 1.0f;
 			glm::vec3 rgb = HSVtoRGB(h, s, v);
 			mQuads[i].AddComponent<SpriteRenderer>(rgb, 1.0f);
 		
 			auto& t = mQuads[i].GetComponent<TransformComponent>();
 			t.Position = glm::vec3(x, y, 0.0f);
 			t.Scale = glm::vec3(0.08f, 0.08f, 1.0f);
 		}

 		//{
 		//	mActiveScene->CreateEntity(mQuad, "Quad");
 		//
 		//	mQuad.AddComponent<SpriteRenderer>(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
 		//	auto& t = mQuad.GetComponent<TransformComponent>();
 		//	t.Position = glm::vec3(0.0f, 0.0f, 0.0f);
 		//	t.Scale = glm::vec3(1.0f, 1.0f, 1.0f);
 		//
 		//}
 
		mActiveScene->OnResize(windowSize.x, windowSize.y);

		mRenderer->AddImGuiRenderCallback([this]() { OnImGuiRender(); });
	}

	void Player::OnUpdate()
	{
		float dt = Core::GetClock().GetSeconds();

		if (mMoveEnabled && mQuads.size() > 0)
		{
			for (Uint i = 0; i < mQuads.size(); i++)
			{
				TransformComponent& transform = mQuads[i].GetComponent<TransformComponent>();

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
		ImGuiID dockspace_id = ImGui::GetID("DockSpace");
		ImGui::DockSpaceOverViewport(dockspace_id, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

		const RHIStats& stats = Core::GetRenderer()->GetRHI()->GetStats();
		ImGui::Begin("Control & Stats");
		ImGui::Text("Total quads: %d\nVertices: %i\nFPS: %.1f", Core::GetRenderer()->GetQuadCount(), Core::GetRenderer()->GetvertexCount(), 1 / clock.GetSeconds());
		ImGui::Text("Draw call(s): %i", stats.DrawCalls);
		ImGui::Text("GPU: %s", stats.GPUName.c_str());
		ImGui::Text("Vendor: %s", stats.VendorName.c_str());
		ImGui::Text("%s", stats.RHIVersion.c_str());
		ImGui::Separator();
		ImGui::Text("Controls");
		ImGui::Checkbox("Move quads", &mMoveEnabled);

		if (ImGui::SliderInt("Quad Count", &mChangeQuadAmount, 0, Renderer::MAX_QUADS))
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
					mActiveScene->DestroyEntity(mQuads.back());
					mQuads.pop_back();
				}
			}
			else if (currentQuadCount < mChangeQuadAmount)
			{
				Uint toAdd = mChangeQuadAmount - currentQuadCount;
				for (Uint i = 0; i < toAdd; i++)
				{
					glm::vec2 pos = GenRandomPosition(halfWidth, halfHeight);
					Entity quad;
					mActiveScene->CreateEntity(quad, "StressQuad");
			
					std::uniform_real_distribution<float> hueDist(0.0f, 1.0f);
					float h = GenRandomHue();
					float s = 1.0f;
					float v = 1.0f;
					glm::vec3 rgb = HSVtoRGB(h, s, v);
					quad.AddComponent<SpriteRenderer>(rgb, 1.0f);
			
					auto& t = quad.GetComponent<TransformComponent>();
					t.Position = glm::vec3(pos.x, pos.y, 0.0f);
					t.Scale = glm::vec3(0.08f, 0.08f, 1.0f);
					mQuads.push_back(quad);
				}
			}
		}


		ImGui::Separator();
		float used = stats.UsedGPUMemory;
		float total = stats.TotalAllowedGPUMemory;
		float frac = (total > 0.0f) ? (used / total) : 0.0f;
		ImGui::Text("GPU Memory:");
		ImGui::Text("Allocations: %llu", stats.AllocationCount);
		ImGui::Text("%.1f MB / %.1f MB", used, total);
		ImGui::ProgressBar(frac, ImVec2(-1, 0));
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
		{
			mActiveScene->OnResize(static_cast<float>(width), static_cast<float>(height));
		}
	}

	void Player::OnShutdown()
	{}

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