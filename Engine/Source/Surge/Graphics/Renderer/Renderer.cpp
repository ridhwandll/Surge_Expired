// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Graphics/Interface/DescriptorSet.hpp"
#include "Surge/Graphics/Camera/EditorCamera.hpp"

namespace Surge
{
    struct UBufCameraData // At binding 0 set 0
    {
        glm::mat4 ViewMatrix;
        glm::mat4 ProjectionMatrix;
        glm::mat4 ViewProjectionMatrix;
    };

    void Renderer::Initialize()
    {
        SURGE_PROFILE_FUNC("Renderer::Initialize()");
        mData = CreateScope<RendererData>();
    }

    void Renderer::BeginFrame(const RuntimeCamera& camera, const glm::mat4& transform)
    {
        SURGE_PROFILE_FUNC("Renderer::BeginFrame(Camera)");
        mData->ViewMatrix = glm::inverse(transform);
        mData->ProjectionMatrix = camera.GetProjectionMatrix();
        mData->ViewProjection = mData->ProjectionMatrix * mData->ViewMatrix;
        mData->CameraPosition = transform[3];
    }

    void Renderer::BeginFrame(const EditorCamera& camera)
    {
        SURGE_PROFILE_FUNC("Renderer::BeginFrame(EditorCamera)");
        mData->ViewMatrix = camera.GetViewMatrix();
        mData->ProjectionMatrix = camera.GetProjectionMatrix();
        mData->ViewProjection = mData->ProjectionMatrix * mData->ViewMatrix;
        mData->CameraPosition = camera.GetPosition();
    }

    void Renderer::EndFrame()
    {
        SURGE_PROFILE_FUNC("Renderer::EndFrame()");
        mData->DrawList.clear();
    }

    void Renderer::SetRenderArea(Uint width, Uint height)
    {
    }

    void Renderer::Shutdown()
    {
        SURGE_PROFILE_FUNC("Renderer::Shutdown()");
        //mData->ShaderSet.Shutdown();
    }

    void Renderer::SubmitPointLight(const PointLightComponent& pointLight, const glm::vec3& position)
    {
    }

    void Renderer::SubmitDirectionalLight(const DirectionalLightComponent& dirLight, const glm::vec3& direction)
    {
    }

} // namespace Surge
