// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Graphics/Camera/EditorCamera.hpp"
#include "Surge/Core/Core.hpp"
#include "../RHI/RHI.hpp"
#include "Surge/Graphics/RenderGraph/Passes/Renderer2DPass.hpp"
#include <Surge/Graphics/RenderGraph/Passes/GeometryPass.hpp>

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

        mRHI = CreateScope<GraphicsRHI>();
        mRHI->Initialize(Core::GetWindow());

        FrameBlackboard& blackBoard = mGraph.GetBlackboard();

        //Sampler
        SamplerDesc samplerDesc = {};
        samplerDesc.DebugName = "Renderer DefaultSampler";
        blackBoard.DefaultSampler = mRHI->CreateSampler(samplerDesc);

        // Offscreen color texture
        glm::vec2 size = Core::GetWindow()->GetSize();
        ImageDesc colorDesc = {};
        colorDesc.Width = size.x;
        colorDesc.Height = size.y;
        colorDesc.Format = ImageFormat::B10G11R11_UFLOAT_PACK32;
        colorDesc.Usage = ImageUsage::COLOR_ATTACHMENT;
        colorDesc.DebugName = "Final Texture";
        colorDesc.Sampler = blackBoard.DefaultSampler;

        // TRANSFER_SRC needed for blit
        RHISettings::BLIT_TO_SWAPCHAIN ? colorDesc.Usage |= ImageUsage::TRANSFER_SRC : colorDesc.Usage |= ImageUsage::SAMPLED;
        RHISettings::BLIT_TO_SWAPCHAIN ? colorDesc.GenerateImGuiID = false : colorDesc.GenerateImGuiID = true;
        blackBoard.FinalImage = mRHI->CreateImage(colorDesc);

        ImageDesc depthDesc = {};
        depthDesc.Width = size.x;
        depthDesc.Height = size.y;
        depthDesc.Format = ImageFormat::D32_SFLOAT;
        depthDesc.Usage = ImageUsage::DEPTH_ATTACHMENT;
        depthDesc.DebugName = "Final Depth Texture";
        blackBoard.DepthImage = mRHI->CreateImage(depthDesc);

        // Offscreen framebuffer
        FramebufferAttachment colorAttachment = {};
        colorAttachment.Handle = blackBoard.FinalImage;
        colorAttachment.Load = LoadOp::CLEAR;
        colorAttachment.Store = StoreOp::STORE;

        FramebufferAttachment depthAttachment = {};
        depthAttachment.Handle = blackBoard.DepthImage;
        depthAttachment.Load = LoadOp::CLEAR;
        depthAttachment.Store = StoreOp::DONT_CARE;

        FramebufferDesc fbDesc = {};
        fbDesc.ColorAttachments[0] = colorAttachment;
        fbDesc.ColorAttachmentCount = 1;
        fbDesc.DepthAttachment = depthAttachment;
        fbDesc.HasDepth = true;
        fbDesc.Width = size.x;
        fbDesc.Height = size.y;
        fbDesc.DebugName = "Offscreen Framebuffer";
        blackBoard.OffscreenFramebuffer = mRHI->CreateFramebuffer(fbDesc);

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

        mGraph.AddPass<GeometryPass>();
        mGraph.AddPass<Renderer2DPass>();
        mGraph.Setup(mRHI.get());
        mGraph.Compile(); // TODO: Implement
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
        mRHI->UploadBuffer(blackBoard.FrameUBOs[mCurrentFrameCtx.FrameIndex], &frameData, sizeof(FrameUBO));

        mCurrentFrameCtx = mRHI->BeginFrame();
        mRHI->CmdBeginRenderPass(mCurrentFrameCtx, blackBoard.OffscreenFramebuffer, blackBoard.ClearColor);
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
        mRHI->UploadBuffer(blackBoard.FrameUBOs[mCurrentFrameCtx.FrameIndex], &frameData, sizeof(FrameUBO));

        mCurrentFrameCtx = mRHI->BeginFrame();
        mRHI->CmdBeginRenderPass(mCurrentFrameCtx, blackBoard.OffscreenFramebuffer, blackBoard.ClearColor);
    }

    void Renderer::EndFrame()
    {
        SURGE_PROFILE_FUNC("Renderer::EndFrame()");
        FrameBlackboard& blackBoard = mGraph.GetBlackboard();

        mGraph.Execute(mCurrentFrameCtx);
        mRHI->CmdEndRenderPass(mCurrentFrameCtx, blackBoard.OffscreenFramebuffer);

        // Swapchain
        if (RHISettings::BLIT_TO_SWAPCHAIN) // Copy the Final Image to the swapchain (Used in Player)
            mRHI->CmdBlitToSwapchain(mCurrentFrameCtx, blackBoard.FinalImage);
        else // If not blitting, we need to transition the final image to SAMPLED for ImGui rendering(Used in Editor)
            mRHI->GetBackendRHI().CmdTransitionImageLayout(mCurrentFrameCtx, blackBoard.FinalImage, ImageUsage::SAMPLED);
        
        mRHI->CmdBeginSwapchainRenderpass(mCurrentFrameCtx);

        // ImGui Render
        for (const auto& callback : mImGuiRenderCallbacks)
            callback();
        
        OnImGuiRender();
        mGraph.OnImGuiRender();

        mRHI->CmdEndSwapchainRenderpass(mCurrentFrameCtx);

        mRHI->EndFrame(mCurrentFrameCtx); // Stops command buffer recording & presents image to swapchain
        mGraph.ClearLists();
    }

    void Renderer::OnImGuiRender()
    {
        ImGui::Begin("Renderer");
        mRHI->ShowMetricsWindow();
        ImGui::End();
    }

    void Renderer::OnWindowResize(Uint width, Uint height)
    {
        if (RHISettings::BLIT_TO_SWAPCHAIN && (width > 0 && height > 0))
        {
            Core::AddFrameEndCallback([this, width, height]()
                {
                    FrameBlackboard& blackBoard = mGraph.GetBlackboard();
                    mRHI->ResizeFramebuffer(blackBoard.OffscreenFramebuffer, width, height);
                });
        }

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
