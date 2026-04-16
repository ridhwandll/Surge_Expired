// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/ECS/Scene.hpp"
#include "Surge/Graphics/RenderProcedure/PreDepthProcedure.hpp"
#include "Surge/Graphics/RenderProcedure/ShadowMapProcedure.hpp"
#include "Surge/Graphics/RenderProcedure/GeometryProcedure.hpp"
#include "Surge/Graphics/RenderProcedure/LightCullingProcedure.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace Surge
{
    struct UBufCameraData // At binding 0 set 0
    {
        glm::mat4 ViewMatrix;
        glm::mat4 ProjectionMatrix;
        glm::mat4 ViewProjectionMatrix;
    };

    struct UBufRendererData // At binding 0 set 1
    {
        Uint TilesCountX; // Forward+
        int ShowLightComplexity;
        float _Padding_1;
        float _Padding_2;
    };

    void Renderer::Initialize()
    {
        SURGE_PROFILE_FUNC("Renderer::Initialize()");
        mData = CreateScope<RendererData>();
        mData->RenderCmdBuffer = RenderCommandBuffer::Create(false);
        mData->ShaderSet.Initialize(BASE_SHADER_PATH);
        mData->ShaderSet.AddShader("PBR.glsl");
        mData->ShaderSet.AddShader("ShadowMap.glsl");
        mData->ShaderSet.AddShader("PreDepth.glsl");
        mData->ShaderSet.AddShader("LightCulling.glsl");
        mData->ShaderSet.LoadAll();

        Ref<Shader> mainPBRShader = GetShader("PBR");
        mData->LightUniformBuffer = UniformBuffer::Create(sizeof(LightUniformBufferData));

        mData->CameraUniformBuffer = UniformBuffer::Create(sizeof(UBufCameraData));
        mData->RendererDataUniformBuffer = UniformBuffer::Create(sizeof(UBufRendererData));
        mData->DescriptorSet0 = DescriptorSet::Create(mainPBRShader, 0, false);

        Uint whiteTextureData = 0xffffffff;
        mData->WhiteTexture = Texture2D::Create(ImageFormat::RGBA8, 1, 1, &whiteTextureData);

        mProcManager.Init(mData);
        mProcManager.AddProcedure<PreDepthProcedure>();
        mProcManager.AddProcedure<LightCullingProcedure>();
        mProcManager.AddProcedure<ShadowMapProcedure>();
        mProcManager.AddProcedure<GeometryProcedure>();
        mProcManager.Sort<PreDepthProcedure, LightCullingProcedure, ShadowMapProcedure, GeometryProcedure>();
    }

    void Renderer::BeginFrame(const Camera& camera, const glm::mat4& transform)
    {
        SURGE_PROFILE_FUNC("Renderer::BeginFrame(Camera)");
        mData->ViewMatrix = glm::inverse(transform);
        mData->ProjectionMatrix = camera.GetProjectionMatrix();
        mData->ViewProjection = mData->ProjectionMatrix * mData->ViewMatrix;
        mData->CameraPosition = transform[3];
        mData->RenderCmdBuffer->BeginRecording();

        LightCullingProcedure::InternalData* lightCullingProcData = Core::GetRenderer()->GetRenderProcManager()->GetRenderProcData<LightCullingProcedure>();
        GeometryProcedure::InternalData* geometryProcData = Core::GetRenderer()->GetRenderProcManager()->GetRenderProcData<GeometryProcedure>();

        UBufCameraData camData = {mData->ViewMatrix, mData->ProjectionMatrix, mData->ViewProjection};
        UBufRendererData rendererData = {lightCullingProcData->TileCountX, lightCullingProcData->ShowLightComplexity, 0.0, 0.0};

        mData->CameraUniformBuffer->SetData(&camData);
        mData->RendererDataUniformBuffer->SetData(&rendererData);

        mData->DescriptorSet0->SetBuffer(mData->CameraUniformBuffer, 0);
        mData->DescriptorSet0->SetBuffer(mData->RendererDataUniformBuffer, 1);

        mData->DescriptorSet0->UpdateForRendering();
        mData->DescriptorSet0->Bind(mData->RenderCmdBuffer, geometryProcData->GeometryPipeline);
    }

    void Renderer::BeginFrame(const EditorCamera& camera)
    {
        SURGE_PROFILE_FUNC("Renderer::BeginFrame(EditorCamera)");
        mData->ViewMatrix = camera.GetViewMatrix();
        mData->ProjectionMatrix = camera.GetProjectionMatrix();
        mData->ViewProjection = mData->ProjectionMatrix * mData->ViewMatrix;
        mData->CameraPosition = camera.GetPosition();

        LightCullingProcedure::InternalData* lightCullingProcData = mProcManager.GetRenderProcData<LightCullingProcedure>();
        GeometryProcedure::InternalData* geometryProcData = mProcManager.GetRenderProcData<GeometryProcedure>();

        UBufCameraData camData = {mData->ViewMatrix, mData->ProjectionMatrix, mData->ViewProjection};
        UBufRendererData rendererData = {lightCullingProcData->TileCountX, lightCullingProcData->ShowLightComplexity, 0.0, 0.0};

        mData->CameraUniformBuffer->SetData(&camData);
        mData->RendererDataUniformBuffer->SetData(&rendererData);

        mData->DescriptorSet0->SetBuffer(mData->CameraUniformBuffer, 0);
        mData->DescriptorSet0->SetBuffer(mData->RendererDataUniformBuffer, 1);

        mData->DescriptorSet0->UpdateForRendering();

        mData->RenderCmdBuffer->BeginRecording();
        mData->DescriptorSet0->Bind(mData->RenderCmdBuffer, geometryProcData->GeometryPipeline);
    }

    void Renderer::EndFrame()
    {
        SURGE_PROFILE_FUNC("Renderer::EndFrame()");

        mProcManager.UpdateAll();
        mData->RenderCmdBuffer->EndRecording();

        mData->RenderCmdBuffer->Submit();
        mData->DrawList.clear();
        mData->PointLights.clear();
    }

    void Renderer::SetRenderArea(Uint width, Uint height)
    {
        if (width || height)
            mProcManager.ResizeAll(width, height);
    }

    Ref<Shader>& Renderer::GetShader(const String& name)
    {
        Ref<Shader>& result = mData->ShaderSet.GetShader(name);
        return result;
    }

    Ref<Framebuffer>& Renderer::GetFinalPassFramebuffer()
    {
        GeometryProcedure::InternalData* passData = mProcManager.GetRenderProcData<GeometryProcedure>();
        return passData->OutputFrambuffer;
    }

    void Renderer::Shutdown()
    {
        SURGE_PROFILE_FUNC("Renderer::Shutdown()");
        mProcManager.Shutdown();
        mData->ShaderSet.Shutdown();
    }

} // namespace Surge
