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

		// Offscreen framebuffer
		FramebufferAttachment colorAttachment = {};
		colorAttachment.Handle = mData->mFinalImage;
		colorAttachment.Load = LoadOp::CLEAR;
		colorAttachment.Store = StoreOp::STORE;

		FramebufferDesc fbDesc = {};
		fbDesc.ColorAttachments[0] = colorAttachment;
		fbDesc.ColorAttachmentCount = 1;
		fbDesc.Width = size.x;
		fbDesc.Height = size.y;
		fbDesc.DebugName = "Offscreen Framebuffer";
		mData->mOffscreenFramebuffer = mRHI->CreateFramebuffer(fbDesc);

		mRenderer2D.Initialize(mRHI.get(), mData.get());
		mRenderer3D.Initialize(mRHI.get(), mData.get());
    }

    void Renderer::BeginFrame(const EditorCamera& camera)
    {
        SURGE_PROFILE_FUNC("Renderer::BeginFrame(EditorCamera)");
        mData->ViewMatrix = camera.GetViewMatrix();
        mData->ProjectionMatrix = camera.GetProjectionMatrix();
        mData->ViewProjection = mData->ProjectionMatrix * mData->ViewMatrix;
        mData->CameraPosition = camera.GetPosition();

		mCurrentFrameCtx = mRHI->BeginFrame();
		mRHI->CmdBeginRenderPass(mCurrentFrameCtx, mData->mOffscreenFramebuffer, mData->mClearColor);

		mRenderer2D.BeginFrame(mCurrentFrameCtx);
		mRenderer3D.BeginFrame(mCurrentFrameCtx);
    }

	void Renderer::BeginFrame(const RuntimeCamera& camera, const glm::mat4& transform)
	{
		SURGE_PROFILE_FUNC("Renderer::BeginFrame(Camera)");
		mData->ViewMatrix = glm::inverse(transform);
		mData->ProjectionMatrix = camera.GetProjectionMatrix();
		mData->ViewProjection = mData->ProjectionMatrix * mData->ViewMatrix;
		mData->CameraPosition = transform[3];
	
		mCurrentFrameCtx = mRHI->BeginFrame();
		mRHI->CmdBeginRenderPass(mCurrentFrameCtx, mData->mOffscreenFramebuffer, mData->mClearColor);

		mRenderer2D.BeginFrame(mCurrentFrameCtx);
		mRenderer3D.BeginFrame(mCurrentFrameCtx);
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
		
	void Renderer::SubmitMesh(const glm::mat4& transform, const Ref<Mesh>& mesh)
	{
		mRenderer3D.SubmitMesh(transform, mesh);
	}

	void Renderer::OnImGuiRender()
	{
		ImGui::Begin("Renderer");
		mRenderer2D.OnImGuiRender();
		mRenderer3D.OnImGuiRender();
		mRHI->ShowMetricsWindow();
		ImGui::End();
	}

	void Renderer::SubmitQuad(const glm::mat4& transform, const glm::vec4& color, TextureHandle texture)
	{
		mRenderer2D.Submit(transform, color, texture);
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
		mRHI->DestroySampler(mData->mDefaultSampler);
		mRHI->DestroyFramebuffer(mData->mOffscreenFramebuffer);
		mRHI->DestroyTexture(mData->mFinalImage);

        mRHI->Shutdown();
    }

} // namespace Surge
