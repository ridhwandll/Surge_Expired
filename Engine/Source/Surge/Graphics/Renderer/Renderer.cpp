// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Graphics/Camera/EditorCamera.hpp"
#include "Surge/Core/Core.hpp"

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
        mData->mMaterialRegistry.Initialize(mRHI.get());

        //Sampler
        SamplerDesc samplerDesc = {};
        samplerDesc.DebugName = "Renderer DefaultSampler";
        mData->mDefaultSampler = mRHI->CreateSampler(samplerDesc);

        // Offscreen color texture
        glm::vec2 size = Core::GetWindow()->GetSize();
        ImageDesc colorDesc = {};
        colorDesc.Width = size.x;
        colorDesc.Height = size.y;
        colorDesc.Format = ImageFormat::B10G11R11_UFLOAT_PACK32;
        colorDesc.Usage = ImageUsage::COLOR_ATTACHMENT;
        colorDesc.DebugName = "Final Texture";
        colorDesc.Sampler = mData->mDefaultSampler;

        // TRANSFER_SRC needed for blit
        RHISettings::BLIT_TO_SWAPCHAIN ? colorDesc.Usage |= ImageUsage::TRANSFER_SRC : colorDesc.Usage |= ImageUsage::SAMPLED;
        RHISettings::BLIT_TO_SWAPCHAIN ? colorDesc.GenerateImGuiID = false : colorDesc.GenerateImGuiID = true;
        mData->mFinalImage = mRHI->CreateTexture(colorDesc);

        ImageDesc depthDesc = {};
        depthDesc.Width = size.x;
        depthDesc.Height = size.y;
        depthDesc.Format = ImageFormat::D32_SFLOAT;
        depthDesc.Usage = ImageUsage::DEPTH_ATTACHMENT;
        depthDesc.DebugName = "Final Depth Texture";
        mData->mDepthImage = mRHI->CreateTexture(depthDesc);

        // Offscreen framebuffer
        FramebufferAttachment colorAttachment = {};
        colorAttachment.Handle = mData->mFinalImage;
        colorAttachment.Load = LoadOp::CLEAR;
        colorAttachment.Store = StoreOp::STORE;

        FramebufferAttachment depthAttachment = {};
        depthAttachment.Handle = mData->mDepthImage;
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
        mData->mOffscreenFramebuffer = mRHI->CreateFramebuffer(fbDesc);

        uint8_t whitePixel[] = { 255, 255, 255, 255 };
        ImageDesc texDesc = {};
        texDesc.Width = 1;
        texDesc.Height = 1;
        texDesc.Format = ImageFormat::RGBA8_UNORM;
        texDesc.Usage = ImageUsage::SAMPLED | ImageUsage::TRANSFER_DST;
        texDesc.DebugName = "WhiteTexture";
        texDesc.InitialData = whitePixel;
        texDesc.DataSize = sizeof(whitePixel);
        texDesc.Sampler = mData->mDefaultSampler;
        mData->mWhiteImage = mRHI->CreateTexture(texDesc);

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

        mCurrentFrameCtx = mRHI->BeginFrame();
        mRHI->CmdBeginRenderPass(mCurrentFrameCtx, mData->mOffscreenFramebuffer, mData->mClearColor);

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
    
        mCurrentFrameCtx = mRHI->BeginFrame();
        mRHI->CmdBeginRenderPass(mCurrentFrameCtx, mData->mOffscreenFramebuffer, mData->mClearColor);

        mRenderer2D.BeginFrame(mCurrentFrameCtx);
        mRenderer3D.BeginFrame(mCurrentFrameCtx, submitCount3D);
    }

    void Renderer::EndFrame()
    {
        SURGE_PROFILE_FUNC("Renderer::EndFrame()");

        mRenderer3D.EndFrame();
        mRenderer2D.EndFrame();

        mRHI->CmdEndRenderPass(mCurrentFrameCtx, mData->mOffscreenFramebuffer);

        // Swapchain
        if (RHISettings::BLIT_TO_SWAPCHAIN) // Copy the Final Image to the swapchain (Used in Player)
            mRHI->CmdBlitToSwapchain(mCurrentFrameCtx, mData->mFinalImage);
        else // If not blitting, we need to transition the final image to SAMPLED for ImGui rendering(Used in Editor)
            mRHI->GetBackendRHI().CmdTransitionImageLayout(mCurrentFrameCtx, mData->mFinalImage, ImageUsage::SAMPLED);
        
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
                    mRHI->ResizeFramebuffer(mData->mOffscreenFramebuffer, width, height);
                });
        }

        mRenderer2D.OnWindowResize(width, height);
        mRenderer3D.OnWindowResize(width, height);
    }

    Ref<Material> Renderer::CreateMaterial(const String& debugName)
    {
        return Ref<Material>::Create(mData->mMaterialRegistry, debugName);
    }

    void Renderer::Shutdown()
    {
        SURGE_PROFILE_FUNC("Renderer::Shutdown()");
        mRHI->WaitIdle();
        mRenderer2D.Shutdown();
        mRenderer3D.Shutdown();

        mRHI->DestroyTexture(mData->mWhiteImage);

        mRHI->DestroySampler(mData->mDefaultSampler);
        mRHI->DestroyFramebuffer(mData->mOffscreenFramebuffer);
        mRHI->DestroyTexture(mData->mFinalImage);
        mRHI->DestroyTexture(mData->mDepthImage);

        mData->mMaterialRegistry.Shutdown();
        mRHI->Shutdown();
    }

} // namespace Surge
