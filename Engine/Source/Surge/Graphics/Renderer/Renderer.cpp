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

		//Sampler
		SamplerDesc samplerDesc = {};
		samplerDesc.DebugName = "Renderer DefaultSampler";
		mData->mDefaultSampler = mRHI->CreateSampler(samplerDesc);

		// Offscreen color texture
		glm::vec2 size = Core::GetWindow()->GetSize();
		TextureDesc colorDesc = {};
		colorDesc.Width = size.x;
		colorDesc.Height = size.y;
		colorDesc.Format = TextureFormat::RGBA8_UNORM;
		colorDesc.Usage = TextureUsage::COLOR_ATTACHMENT | TextureUsage::SAMPLED | TextureUsage::TRANSFER_SRC; // TRANSFER_SRC needed for blit
		colorDesc.DebugName = "Final Texture";
		colorDesc.Sampler = mData->mDefaultSampler;
		RHISettings::BLIT_TO_SWAPCHAIN ? colorDesc.GenerateImGuiID = false : colorDesc.GenerateImGuiID = true;
		mData->mFinalImage = mRHI->CreateTexture(colorDesc);

		TextureDesc depthDesc = {};
		depthDesc.Width = size.x;
		depthDesc.Height = size.y;
		depthDesc.Format = TextureFormat::D32_SFLOAT;
		depthDesc.Usage = TextureUsage::DEPTH_ATTACHMENT;
		depthDesc.DebugName = "Final Depth Texture";
		depthDesc.GenerateImGuiID = false;
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

		// Frame UBO
		BufferDesc frameUBODesc = {};
		frameUBODesc.Usage = BufferUsage::UNIFORM;
		frameUBODesc.HostVisible = true;
		frameUBODesc.DebugName = "FrameUBO";
		frameUBODesc.Size = sizeof(FrameUBO);
		mData->mFrameUBO = mRHI->CreateBuffer(frameUBODesc);

		mRenderer2D.Initialize(mRHI.get(), mData.get());
		mRenderer3D.Initialize(mRHI.get(), mData.get());

		mData->mFrameDescriptorSet = mRHI->CreateDescriptorSet(mRHI->GetDescriptorLayout(mRenderer3D.mPipeline), DescriptorUpdateFrequency::STATIC, "Frame DescriptorSet");
		DescriptorWrite frameDescriptorWrite = {};
		frameDescriptorWrite.Slot = 0;
		frameDescriptorWrite.Type = DescriptorType::UNIFORM_BUFFER;
		frameDescriptorWrite.Buffer = mData->mFrameUBO;
		mRHI->UpdateDescriptorSet(mData->mFrameDescriptorSet, &frameDescriptorWrite, 1);
    }

    void Renderer::BeginFrame(const EditorCamera& camera)
    {
        SURGE_PROFILE_FUNC("Renderer::BeginFrame(EditorCamera)");
        mData->ViewMatrix = camera.GetViewMatrix();
        mData->ProjectionMatrix = camera.GetProjectionMatrix();
        mData->ViewProjection = mData->ProjectionMatrix * mData->ViewMatrix;
        mData->CameraPosition = camera.GetPosition();

		FrameUBO frameData = {};
		frameData.ViewProjection = mData->ViewProjection;
		frameData.CameraPos = mData->CameraPosition;
		mRHI->UploadBuffer(mData->mFrameUBO, &frameData, sizeof(FrameUBO));

		mCurrentFrameCtx = mRHI->BeginFrame();
		mRHI->CmdBeginRenderPass(mCurrentFrameCtx, mData->mOffscreenFramebuffer, mData->mClearColor);

		mRenderer2D.BeginFrame(mCurrentFrameCtx);
		mRenderer3D.BeginFrame(mCurrentFrameCtx);

		mRHI->BindDescriptorSet(mCurrentFrameCtx, mRenderer3D.mPipeline, mData->mFrameDescriptorSet, 0);
    }

	void Renderer::BeginFrame(const RuntimeCamera& camera, const glm::mat4& transform)
	{
		SURGE_PROFILE_FUNC("Renderer::BeginFrame(Camera)");
		mData->ViewMatrix = glm::inverse(transform);
		mData->ProjectionMatrix = camera.GetProjectionMatrix();
		mData->ViewProjection = mData->ProjectionMatrix * mData->ViewMatrix;
		mData->CameraPosition = transform[3];
	
		FrameUBO frameData = {};
		frameData.ViewProjection = mData->ViewProjection;
		frameData.CameraPos = mData->CameraPosition;
		mRHI->UploadBuffer(mData->mFrameUBO, &frameData, sizeof(FrameUBO));

		mCurrentFrameCtx = mRHI->BeginFrame();
		mRHI->CmdBeginRenderPass(mCurrentFrameCtx, mData->mOffscreenFramebuffer, mData->mClearColor);

		mRenderer2D.BeginFrame(mCurrentFrameCtx);
		mRenderer3D.BeginFrame(mCurrentFrameCtx);

		mRHI->BindDescriptorSet(mCurrentFrameCtx, mRenderer3D.mPipeline, mData->mFrameDescriptorSet, 0);
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
			mRHI->GetBackendRHI().CmdTransitionTextureLayout(mCurrentFrameCtx, mData->mFinalImage, TextureUsage::SAMPLED);
		
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

	void Renderer::Shutdown()
    {
        SURGE_PROFILE_FUNC("Renderer::Shutdown()");
        mRHI->WaitIdle();
		mRenderer2D.Shutdown();
		mRenderer3D.Shutdown();

		mRHI->DestroyDescriptorSet(mData->mFrameDescriptorSet);

		mRHI->DestroyBuffer(mData->mFrameUBO);
		mRHI->DestroySampler(mData->mDefaultSampler);
		mRHI->DestroyFramebuffer(mData->mOffscreenFramebuffer);
		mRHI->DestroyTexture(mData->mFinalImage);
		mRHI->DestroyTexture(mData->mDepthImage);

        mRHI->Shutdown();
    }

} // namespace Surge
