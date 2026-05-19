// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Graphics/Camera/EditorCamera.hpp"
#include "Surge/Core/Core.hpp"

#define ENGINE_SHADER_PATH "Engine/Assets/Shaders"

namespace Surge
{
    void Renderer::Initialize()
    {
        SURGE_PROFILE_FUNC("Renderer::Initialize()");
        mData = CreateScope<RendererData>();

        const ClientOptions& clientOptions = Core::GetClient()->GetClientOptions();
        RHISettings::BLIT_TO_SWAPCHAIN = clientOptions.RenderFinalImageToSwapchian;

        mRHI = CreateScope<GraphicsRHI>();
        mRHI->Initialize(Core::GetWindow());

        mData->ShaderManager_.Initialize(ENGINE_SHADER_PATH);
        mData->ShaderManager_.Load("Renderer2D.glsl");
        mData->ShaderManager_.Load("Renderer3D.glsl");

        //Sampler
        SamplerDesc samplerDesc = {};
        samplerDesc.DebugName = "Renderer DefaultSampler";
        mData->DefaultSampler = mRHI->CreateSampler(samplerDesc);

        // Offscreen color texture
        glm::vec2 size = Core::GetWindow()->GetSize();
        ImageDesc colorDesc = {};
        colorDesc.Width = size.x;
        colorDesc.Height = size.y;
        colorDesc.Format = ImageFormat::B10G11R11_UFLOAT_PACK32;
        colorDesc.Usage = ImageUsage::COLOR_ATTACHMENT;
        colorDesc.DebugName = "Final Texture";
        colorDesc.Sampler = mData->DefaultSampler;

        // TRANSFER_SRC needed for blit
        RHISettings::BLIT_TO_SWAPCHAIN ? colorDesc.Usage |= ImageUsage::TRANSFER_SRC : colorDesc.Usage |= ImageUsage::SAMPLED;
        RHISettings::BLIT_TO_SWAPCHAIN ? colorDesc.GenerateImGuiID = false : colorDesc.GenerateImGuiID = true;
        mData->FinalImage = mRHI->CreateTexture(colorDesc);

        ImageDesc depthDesc = {};
        depthDesc.Width = size.x;
        depthDesc.Height = size.y;
        depthDesc.Format = ImageFormat::D32_SFLOAT;
        depthDesc.Usage = ImageUsage::DEPTH_ATTACHMENT;
        depthDesc.DebugName = "Final Depth Texture";
        mData->DepthImage = mRHI->CreateTexture(depthDesc);

        // Offscreen framebuffer
        FramebufferAttachment colorAttachment = {};
        colorAttachment.Handle = mData->FinalImage;
        colorAttachment.Load = LoadOp::CLEAR;
        colorAttachment.Store = StoreOp::STORE;

        FramebufferAttachment depthAttachment = {};
        depthAttachment.Handle = mData->DepthImage;
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
        mData->OffscreenFramebuffer = mRHI->CreateFramebuffer(fbDesc);

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
        texDesc.Sampler = mData->DefaultSampler;
        mData->WhiteImage = mRHI->CreateTexture(texDesc);

        BufferDesc frameUBODesc = {};
        frameUBODesc.Usage = BufferUsage::UNIFORM;
        frameUBODesc.HostVisible = true;
        frameUBODesc.DebugName = "FrameUBO";
        frameUBODesc.Size = sizeof(FrameUBO);
        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
            mData->FrameUBOs[i] = mRHI->CreateBuffer(frameUBODesc);

        mRenderer2D.Initialize(mRHI.get(), mData.get());
        mRenderer3D.Initialize(mRHI.get(), mData.get());

    }

    void Renderer::BeginFrame(const EditorCamera& camera, Uint submitCount3D)
    {
        SURGE_PROFILE_FUNC("Renderer::BeginFrame(EditorCamera)");
        mData->ViewMatrix = camera.GetViewMatrix();
        mData->ProjectionMatrix = camera.GetProjectionMatrix();
        mData->ViewProjection = mData->ProjectionMatrix * mData->ViewMatrix;
        mData->CameraPosition = camera.GetPosition();

        FrameUBO frameData = {};
        frameData.ViewProjection = mData->ViewProjection;
        frameData.CameraPos = mData->CameraPosition;
        mRHI->UploadBuffer(mData->FrameUBOs[mCurrentFrameCtx.FrameIndex], &frameData, sizeof(FrameUBO));

        mCurrentFrameCtx = mRHI->BeginFrame();
        mRHI->CmdBeginRenderPass(mCurrentFrameCtx, mData->OffscreenFramebuffer, mData->ClearColor);

        mRenderer2D.BeginFrame(mCurrentFrameCtx);
        mRenderer3D.BeginFrame(mCurrentFrameCtx, submitCount3D);
    }

    void Renderer::BeginFrame(const RuntimeCamera& camera, const glm::mat4& transform, Uint submitCount3D)
    {
        SURGE_PROFILE_FUNC("Renderer::BeginFrame(Camera)");
        mData->ViewMatrix = glm::inverse(transform);
        mData->ProjectionMatrix = camera.GetProjectionMatrix();
        mData->ViewProjection = mData->ProjectionMatrix * mData->ViewMatrix;
        mData->CameraPosition = transform[3];

        FrameUBO frameData = {};
        frameData.ViewProjection = mData->ViewProjection;
        frameData.CameraPos = mData->CameraPosition;
        mRHI->UploadBuffer(mData->FrameUBOs[mCurrentFrameCtx.FrameIndex], &frameData, sizeof(FrameUBO));

        mCurrentFrameCtx = mRHI->BeginFrame();
        mRHI->CmdBeginRenderPass(mCurrentFrameCtx, mData->OffscreenFramebuffer, mData->ClearColor);

        mRenderer2D.BeginFrame(mCurrentFrameCtx);
        mRenderer3D.BeginFrame(mCurrentFrameCtx, submitCount3D);
    }

    void Renderer::EndFrame()
    {
        SURGE_PROFILE_FUNC("Renderer::EndFrame()");

        mRenderer3D.EndFrame();
        mRenderer2D.EndFrame();

        mRHI->CmdEndRenderPass(mCurrentFrameCtx, mData->OffscreenFramebuffer);

        // Swapchain
        if (RHISettings::BLIT_TO_SWAPCHAIN) // Copy the Final Image to the swapchain (Used in Player)
            mRHI->CmdBlitToSwapchain(mCurrentFrameCtx, mData->FinalImage);
        else // If not blitting, we need to transition the final image to SAMPLED for ImGui rendering(Used in Editor)
            mRHI->GetBackendRHI().CmdTransitionImageLayout(mCurrentFrameCtx, mData->FinalImage, ImageUsage::SAMPLED);
        
        mRHI->CmdBeginSwapchainRenderpass(mCurrentFrameCtx);

        // ImGui Render
        for (const auto& callback : mImGuiRenderCallbacks)
            callback();
        
        OnImGuiRender();
    
        mRHI->CmdEndSwapchainRenderpass(mCurrentFrameCtx);

        mRHI->EndFrame(mCurrentFrameCtx); // Stops command buffer recording & presents image to swapchain
    }

    void Renderer::OnImGuiRender()
    {
        ImGui::Begin("Renderer");
        mRenderer2D.OnImGuiRender();
        mRenderer3D.OnImGuiRender();
        mRHI->ShowMetricsWindow();
        ImGui::End();
    }

    void Renderer::OnWindowResize(Uint width, Uint height)
    {
        if (RHISettings::BLIT_TO_SWAPCHAIN && (width > 0 && height > 0))
        {
            Core::AddFrameEndCallback([this, width, height]()
                {
                    mRHI->ResizeFramebuffer(mData->OffscreenFramebuffer, width, height);
                });
        }

        mRenderer2D.OnWindowResize(width, height);
        mRenderer3D.OnWindowResize(width, height);
    }

    Ref<Material> Renderer::CreateMaterial(const String& debugName)
    {
        Ref<Material> material = Ref<Material>::Create(mRenderer3D.m3DPipeline, mData->ShaderManager_.Get("Renderer3D.glsl"), "Material");
        return material;
    }

    void Renderer::Shutdown()
    {
        SURGE_PROFILE_FUNC("Renderer::Shutdown()");
        mRHI->WaitIdle();
        mRenderer2D.Shutdown();
        mRenderer3D.Shutdown();

        mRHI->DestroyTexture(mData->WhiteImage);

        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
            mRHI->DestroyBuffer(mData->FrameUBOs[i]);

        mRHI->DestroySampler(mData->DefaultSampler);
        mRHI->DestroyFramebuffer(mData->OffscreenFramebuffer);
        mRHI->DestroyTexture(mData->FinalImage);
        mRHI->DestroyTexture(mData->DepthImage);

        mRHI->Shutdown();
    }

} // namespace Surge
