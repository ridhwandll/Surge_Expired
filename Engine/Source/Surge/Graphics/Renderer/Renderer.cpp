// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Graphics/Camera/EditorCamera.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Graphics/RHI/RHI.hpp"
#include "Surge/Graphics/RenderGraph/Passes/Renderer2DPass.hpp"
#include "Surge/Graphics/RenderGraph/Passes/GeometryPass.hpp"
#include "Surge/Graphics/RenderGraph/Passes/SwapchainPass.hpp"
#include <Surge/Graphics/RenderGraph/Passes/OutlinePass.hpp>

#define ENGINE_SHADER_PATH "Engine/Assets/Shaders"

namespace Surge
{
    void Renderer::Initialize()
    {
        SURGE_PROFILE_FUNC("Renderer::Initialize()");

        const ClientOptions& clientOptions = Core::GetClient()->GetClientOptions();
        RHISettings::BLIT_TO_SWAPCHAIN = clientOptions.RenderFinalImageToSwapchian;

        mShaderManager.Initialize(ENGINE_SHADER_PATH);
        mShaderManager.Load("Renderer2D.glsl");
        mShaderManager.Load("Renderer3D.glsl");
        mShaderManager.Load("Outline.glsl");
        mShaderManager.Load("OutlineStencilWrite.glsl");

        mRHI = CreateScope<GraphicsRHI>();
        mRHI->Initialize(Core::GetWindow());

        FrameBlackboard& blackBoard = mGraph.GetBlackboard();

        //Sampler
        SamplerDesc samplerDesc = {};
        samplerDesc.DebugName = "Renderer DefaultSampler";
        blackBoard.DefaultSampler = mRHI->CreateSampler(samplerDesc);

        uint8_t whitePixel[] = { 255, 255, 255, 255 };
        ImageDesc texDesc = {};
        texDesc.Width = 1;
        texDesc.Height = 1;
        texDesc.Format = ImageFormat::RGBA8_UNORM;
        texDesc.Usage = ImageUsage::SAMPLED | ImageUsage::TRANSFER_DST;
        texDesc.DebugName = "WhiteTexture";
        texDesc.InitialData = whitePixel;
        texDesc.GenerateImGuiID = true;
        texDesc.DataSize = sizeof(whitePixel);
        texDesc.Sampler = blackBoard.DefaultSampler;
        blackBoard.WhiteImage = mRHI->CreateImage(texDesc);

        BufferDesc frameUBODesc = {};
        frameUBODesc.Usage = BufferUsage::UNIFORM;
        frameUBODesc.HostVisible = true;
        frameUBODesc.DebugName = "FrameUBO";
        frameUBODesc.Size = sizeof(FrameUBO);
        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
            blackBoard.FrameUBOs[i] = mRHI->CreateBuffer(frameUBODesc);

        mGraph.AddPass<GeometryPass>();  // Must add GeometryPass before Renderer2DPass because it creates the blackboard.FinalImage
        mGraph.AddPass<OutlinePass>();
        mGraph.AddPass<Renderer2DPass>();
        mGraph.AddPass<SwapchainPass>();
        mGraph.Setup(mRHI.get());
        mGraph.Compile();
    }

    void Renderer::BeginFrame(const EditorCamera& camera, Uint submitCount3D)
    {
        SURGE_PROFILE_FUNC("Renderer::BeginFrame(EditorCamera)");
        FrameBlackboard& blackBoard = mGraph.GetBlackboard();

        blackBoard.ViewMatrix = camera.GetViewMatrix();
        blackBoard.ProjectionMatrix = camera.GetProjectionMatrix();
        blackBoard.ViewProjection = blackBoard.ProjectionMatrix * blackBoard.ViewMatrix;
        blackBoard.CameraPosition = camera.GetPosition();

        FrameUBO frameData = {};
        frameData.ViewProjection = blackBoard.ViewProjection;
        frameData.CameraPos = blackBoard.CameraPosition;

        mCurrentFrameCtx = mRHI->BeginFrame();
        mRHI->UploadBuffer(blackBoard.FrameUBOs[mCurrentFrameCtx.FrameIndex], &frameData, sizeof(FrameUBO));
    }

    void Renderer::BeginFrame(const RuntimeCamera& camera, const glm::mat4& transform, Uint submitCount3D)
    {
        SURGE_PROFILE_FUNC("Renderer::BeginFrame(Camera)");
        FrameBlackboard& blackBoard = mGraph.GetBlackboard();

        blackBoard.ViewMatrix = glm::inverse(transform);
        blackBoard.ProjectionMatrix = camera.GetProjectionMatrix();
        blackBoard.ViewProjection = blackBoard.ProjectionMatrix * blackBoard.ViewMatrix;
        blackBoard.CameraPosition = transform[3];

        FrameUBO frameData = {};
        frameData.ViewProjection = blackBoard.ViewProjection;
        frameData.CameraPos = blackBoard.CameraPosition;

        mCurrentFrameCtx = mRHI->BeginFrame();
        mRHI->UploadBuffer(blackBoard.FrameUBOs[mCurrentFrameCtx.FrameIndex], &frameData, sizeof(FrameUBO));
    }

    void Renderer::EndFrame()
    {
        SURGE_PROFILE_FUNC("Renderer::EndFrame()");
        mGraph.Execute(mCurrentFrameCtx);

        mRHI->EndFrame(mCurrentFrameCtx); // Stops command buffer recording & presents image to swapchain
        mGraph.ClearLists();
    }

    void Renderer::OnWindowResize(Uint width, Uint height)
    {
        mGraph.Resize(width, height);
    }

    Ref<Material> Renderer::CreateMaterial(const String& debugName)
    {
        FrameBlackboard& blackBoard = mGraph.GetBlackboard();
        Ref<Material> material = Ref<Material>::Create(blackBoard.MaterialPipeline, mShaderManager.Get("Renderer3D.glsl"), "Material");
        return material;
    }

    void Renderer::Shutdown()
    {
        SURGE_PROFILE_FUNC("Renderer::Shutdown()");
        FrameBlackboard& blackBoard = mGraph.GetBlackboard();

        mRHI->WaitIdle();
        mGraph.Shutdown();

        mRHI->DestroyImage(blackBoard.WhiteImage);

        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
            mRHI->DestroyBuffer(blackBoard.FrameUBOs[i]);

        mRHI->DestroySampler(blackBoard.DefaultSampler);
        mRHI->DestroyFramebuffer(blackBoard.OffscreenFramebuffer);
        mRHI->DestroyImage(blackBoard.FinalImage);
        mRHI->DestroyImage(blackBoard.DepthImage);
        mRHI->Shutdown();
    }

} // namespace Surge
