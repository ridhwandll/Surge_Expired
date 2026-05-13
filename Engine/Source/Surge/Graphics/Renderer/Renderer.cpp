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

		mRHI = CreateScope<GraphicsRHI>();
        mRHI->Initialize(Core::GetWindow());
        
		// Offscreen color texture
		glm::vec2 size = Core::GetWindow()->GetSize();
		TextureDesc colorDesc = {};
		colorDesc.Width = size.x;
		colorDesc.Height = size.y;
		colorDesc.Format = TextureFormat::RGBA8_UNORM;
		colorDesc.Usage = TextureUsage::COLOR_ATTACHMENT | TextureUsage::TRANSFER_SRC; // TRANSFER_SRC needed for blit
		colorDesc.DebugName = "Offscreen Color Texture";
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

		mRHI->CmdEndRenderPass(mCurrentFrameCtx);

		// Swapchain
		mRHI->CmdBlitToSwapchain(mCurrentFrameCtx, mData->mFinalImage);
		mRHI->CmdBeginSwapchainRenderpass(mCurrentFrameCtx);	

		// ImGui Render
		for (const auto& callback : mImGuiRenderCallbacks)
			callback();
		
		OnImGuiRender();
	
		mRHI->CmdEndSwapchainRenderpass(mCurrentFrameCtx);
		mRHI->EndFrame(mCurrentFrameCtx); // Stops command buffer recording
    }
		
	void Renderer::SubmitMesh(const glm::mat4& transform, const Ref<Mesh>& mesh)
	{
		mRenderer3D.SubmitMesh(transform, mesh);
	}

	void Renderer::OnImGuiRender()
	{
		mRenderer2D.OnImGuiRender();
		mRenderer3D.OnImGuiRender();
		mRHI->ShowMetricsWindow();
	}

	void Renderer::SubmitQuad(const glm::mat4& transform, const glm::vec4& color, TextureHandle texture)
	{
		mRenderer2D.Submit(transform, color, texture);
	}

	void Renderer::OnWindowResize(Uint width, Uint height)
	{
		mRenderer2D.OnWindowResize(width, height);
		mRenderer3D.OnWindowResize(width, height);
	}

	void Renderer::Shutdown()
    {
        SURGE_PROFILE_FUNC("Renderer::Shutdown()");
        mRHI->WaitIdle();
		mRenderer2D.Shutdown();
		mRenderer3D.Shutdown();

		mRHI->DestroyFramebuffer(mData->mOffscreenFramebuffer);
		mRHI->DestroyTexture(mData->mFinalImage);

        mRHI->Shutdown();
    }

} // namespace Surge
