// Copyright (c) - SurgeTechnologies - All rights reserved
#include <Surge/Surge.hpp>
#include "Player.hpp"
#include "Surge/Graphics/RenderGraph/Passes/Renderer2DPass.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Asset/Mesh.hpp"
#include "Surge/Asset/DefaultMeshes.hpp"
#include "Surge/Asset/Texture2D.hpp"

#include <random>
#include <imgui_internal.h>


namespace Surge
{
    static glm::vec3 HSVtoRGB(float h, float s, float v)
    {
        float c = v * s;
        float x = c * (1.0f - fabs(fmod(h * 6.0f, 2.0f) - 1.0f));
        float m = v - c;

        glm::vec3 rgb;

        if(h < 1.0f / 6.0f) rgb = { c, x, 0 };
        else if(h < 2.0f / 6.0f) rgb = { x, c, 0 };
        else if(h < 3.0f / 6.0f) rgb = { 0, c, x };
        else if(h < 4.0f / 6.0f) rgb = { 0, x, c };
        else if(h < 5.0f / 6.0f) rgb = { x, 0, c };
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

    void Player::OnInitialize()
    {
        mRenderer = Core::GetRenderer();
        mActiveScene = Ref<Scene>::Create();
        Ref<Mesh> mesh = Mesh::Create(DefaultMesh::CYLINDER);

        {
            {
                mActiveScene->CreateEntity(mRotatingEntity, "Vulkan Scene");
                MeshComponent& meshComp = mRotatingEntity.AddComponent<MeshComponent>();
                meshComp.MeshID = AssetManager::Import("Mesh/VulkanScene.glb", AssetType::MESH);

                TransformComponent& t = mRotatingEntity.GetComponent<TransformComponent>();
                t.Position = glm::vec3(2.0f, 1.6f, 0.0f);
                t.MarkDirty();
            }
        }
        {
            Entity directionalLight;
            mActiveScene->CreateEntity(directionalLight, "Directional Light");
            LightComponent& lightComp = directionalLight.AddComponent<LightComponent>();
            lightComp.Type = LightType::DIRECTIONAL;
            lightComp.Intensity = 5.5f;
            lightComp.Radius = 1.0f;
            TransformComponent& t = directionalLight.GetComponent<TransformComponent>();
            t.Position = glm::vec3(0.0f, 0.0f, 0.0f);
            t.Rotation = glm::vec3(30.0f, -30.0f, 30.0f);
            t.MarkDirty();
        }
        mRenderer->AddImGuiRenderCallback([this]() { OnImGuiRender(); });

        FrameBlackboard& bb =  mRenderer->GetRenderGraphBlackBoard();
        bb.VignetteGrain.Intensity = 0.7f;
        bb.VignetteGrain.Softness = 0.3f;
        bb.VignetteGrain.Grain = 0.01f;

        bb.Skybox.EnableSunDisk = false;
        bb.Skybox.Elevation = 10.0f;
    }

    void Player::OnUpdate()
    {
        constexpr float axesLength = 10000.0f;
        mRenderer->SubmitLine({ -axesLength, 0.0f, 0.0f }, { axesLength, 0.0f, 0.0f }, { 1.0f, 0.3f, 0.3f, 1.0f }); // X
        mRenderer->SubmitLine({ 0.0f, -axesLength, 0.0f }, { 0.0f, axesLength, 0.0f }, { 0.3f, 0.8f, 0.3f, 1.0f }); // Y
        mRenderer->SubmitLine({ 0.0f, 0.0f, -axesLength }, { 0.0f, 0.0f, axesLength }, { 0.3f, 0.3f, 1.0f, 1.0f }); // Z

        float dt = Core::GetClock().GetSeconds();

        TransformComponent& floorTransform = mRotatingEntity.GetComponent<TransformComponent>();
        floorTransform.Rotation.y += 50.0f * dt;
        floorTransform.MarkDirty();

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
                transform.MarkDirty();
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

        const FrameBlackboard& bb = mRenderer->GetRenderGraphBlackBoard();

        ImGui::Text("Vertices: %i\n%.1fms (FPS: %.1f)", bb.QuadList.size() * 4, clock.GetMilliseconds(), 1 / clock.GetSeconds());
        ImGui::Text("Textured Quads: %d", mTexturedQuadCount);
        ImGui::Text("Non-Textured Quads: %d", mColoredQuads.size());
        ImGui::Text("Total Quads: %d", bb.QuadList.size());
        ImGui::Checkbox("Move quads", &mMoveEnabled);

        if(ImGui::SliderInt("ColorQuads", &mChangeQuadAmount, mTexturedQuadCount, Renderer2DPass::MAX_QUADS_TOTAL))
        {
            RuntimeCamera* cam = mActiveScene->GetMainCameraEntity().Data1;
            float size = cam->GetOrthographicSize();
            float aspect = cam->GetAspectRatio();
            float halfWidth = size * aspect * 0.5f;
            float halfHeight = size * 0.5f;

            Uint currentQuadCount = bb.QuadList.size();
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
                    quad.AddComponent<SpriteRendererComponent>(rgb, 1.0f);

                    auto& t = quad.GetComponent<TransformComponent>();
                    t.Position = glm::vec3(pos.x, pos.y, 0.0f);
                    t.Scale = glm::vec3(0.08f, 0.08f, 1.0f);
                }
            }
        }

        ImGui::End();
    }

    static bool showImGui = true;
    void Player::OnEvent(Event& e)
    {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<WindowResizeEvent>([&](WindowResizeEvent& resizeEvent)
                                               {
                                                   Resize(resizeEvent.GetWidth(), resizeEvent.GetHeight());
                                               });

        dispatcher.Dispatch<KeyPressedEvent>([&](KeyPressedEvent& keyEvent)
                                             {
                                                 if(keyEvent.GetKeyCode() == Key::F7)
                                                 {
                                                     showImGui = !showImGui;
                                                     mRenderer->ShowInternalImGui(showImGui);
                                                 }
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
            mRenderer->GetRHI()->DestroyImage(texture);
    }

} // namespace Surge

#ifndef SURGE_PLATFORM_ANDROID
// Entry point
int main()
{
    Surge::ClientOptions clientOptions;
    clientOptions.RenderFinalImageToSwapchian = true;
    clientOptions.WindowDescription = { 1280, 720, "Player", Surge::WindowFlags::CreateDefault /*| Surge::WindowFlags::NoTitlebar*/ };

    Surge::Player* app = Surge::MakeClient<Surge::Player>();
    app->SetOptions(clientOptions);

    Surge::Core::Initialize(app);
    Surge::Core::Run();
    Surge::Core::Shutdown();
}
#endif