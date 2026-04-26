// Copyright (c) - SurgeTechnologies - All rights reserved
#include <Surge/Surge.hpp>
#include "Player.hpp"


namespace Surge
{
    void Player::OnInitialize()
    {
        mRenderer = Core::GetRenderer();

        //mRenderer->SetRenderArea(static_cast<Uint>(viewport->GetViewportSize().x), static_cast<Uint>(viewport->GetViewportSize().y));
        mActiveScene = Ref<Scene>::Create(false);
        mRenderer->SetSceneContext(mActiveScene);
        Entity runtimeCamera;
        Entity dirLight;
        Entity pointLight;
        Entity floor;
        Entity cube;
        {
            mActiveScene->CreateEntity(runtimeCamera, "Runtime Camera");
            CameraComponent& cam = runtimeCamera.AddComponent<CameraComponent>();
            cam.Primary = true;
            cam.FixedAspectRatio = true;
            cam.Camera.SetPerspectiveFarClip(1000);
            TransformComponent& transform = runtimeCamera.GetComponent<TransformComponent>();
            transform.Position = glm::vec3(-10, 6, 10);
            transform.Rotation = glm::vec3(-30, -45, 0);
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
        {
            mActiveScene->CreateEntity(floor, "Floor");
            floor.AddComponent<MeshComponent>().Mesh = Ref<Mesh>::Create("Engine/Assets/Mesh/Cube.fbx");
            TransformComponent& transform = floor.GetComponent<TransformComponent>();
            transform.Position = glm::vec3(0, -1, 0);
            transform.Scale = glm::vec3(10, 1, 10);
        }
        {
            mActiveScene->CreateEntity(cube, "Cube");
            cube.AddComponent<MeshComponent>().Mesh = Ref<Mesh>::Create("Engine/Assets/Mesh/Cube.fbx");
        }

        glm::vec2 windowSize = Core::GetWindow()->GetSize();
        mActiveScene->OnResize(windowSize.x, windowSize.y);
    }

    void Player::OnUpdate()
    {
        mActiveScene->Update();
    }

    void Player::OnImGuiRender()
    {
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
            mRenderer->SetRenderArea(width, height);
            mActiveScene->OnResize(static_cast<float>(width), static_cast<float>(height));
        }
    }

    void Player::OnShutdown()
    {
    }

} // namespace Surge

#ifndef SURGE_PLATFORM_ANDROID
// Entry point
int main()
{
    Surge::ClientOptions clientOptions;
    clientOptions.EnableImGui = false;
    clientOptions.WindowDescription = {1280, 720, "Player", Surge::WindowFlags::CreateDefault};

    Surge::Player* app = Surge::MakeClient<Surge::Player>();
    app->SetOptions(clientOptions);

    Surge::Core::Initialize(app);
    Surge::Core::Run();
    Surge::Core::Shutdown();
}
#endif