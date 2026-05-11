// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Graphics/Camera/EditorCamera.hpp"
#include "Surge/Core/Core.hpp"
#include <exception>
#include "Surge/Utility/Filesystem.hpp"
#include <algorithm>

#ifdef SURGE_PLATFORM_WINDOWS
#include <shaderc/shaderc.hpp>
#elif defined(SURGE_PLATFORM_ANDROID)
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <android/asset_manager.h>
#include <android/log.h>
#endif

namespace Surge
{
	static constexpr Uint MAX_VERTICES = Renderer::MAX_QUADS_TOTAL * 4;
	static constexpr Uint MAX_INDICES = Renderer::MAX_QUADS_PER_BATCH * 6; // IB of 1 batch, we reuse this

    void Renderer::Initialize()
    {
        SURGE_PROFILE_FUNC("Renderer::Initialize()");
        mData = CreateScope<RendererData>();

		mRHI = CreateScope<GraphicsRHI>();
        mRHI->Initialize(Core::GetWindow());
        
		// Create IB of 1 batch, we resue this for all batches, as the max indices per batch is fixed
		// We fill this with quad index data, and it will reference vertices in the VB based on the current batchs vertex offset
		Vector<Uint> indices(MAX_INDICES);
		Uint offset = 0;
		for (Uint i = 0; i < MAX_INDICES; i += 6)
		{
			indices[i + 0] = offset + 0;
			indices[i + 1] = offset + 1;
			indices[i + 2] = offset + 2;
			indices[i + 3] = offset + 0;
			indices[i + 4] = offset + 2;
			indices[i + 5] = offset + 3;
			offset += 4;
		}
		BufferDesc ibDesc = {};
		ibDesc.Size = sizeof(Uint) * MAX_INDICES;
		ibDesc.Usage = BufferUsage::INDEX;
		ibDesc.HostVisible = true;
		ibDesc.InitialData = indices.data();
		ibDesc.DebugName = "BatchIB";
		mIndexBuffer = mRHI->CreateBuffer(ibDesc);

		// We allocate a large VB for max vertices across all batches, but we only fill and bind the portion
		// needed for the current batch each frame. This is to avoid GPU buffer creation stalls when flushing mid-frame after each batch is submitted.
		BufferDesc vbDesc = {};
		vbDesc.Size = sizeof(QuadVertex) * MAX_VERTICES;
		vbDesc.Usage = BufferUsage::VERTEX;
		vbDesc.HostVisible = true;
		vbDesc.DebugName = "BatchVB";
		mVertexBuffer = mRHI->CreateBuffer(vbDesc);

		// CPU side staging array fill this, then memcpy to GPU buffer
		mVertexData.resize(MAX_QUADS_PER_BATCH * 4);
		mCurrentBatch.Reset();

		SamplerDesc samplerDesc = {};
		mQuadSampler = mRHI->CreateSampler(samplerDesc);

		// Offscreen color texture
		glm::vec2 size = Core::GetWindow()->GetSize();
		TextureDesc colorDesc = {};
		colorDesc.Width = size.x;
		colorDesc.Height = size.y;
		colorDesc.Format = TextureFormat::RGBA8_UNORM;
		colorDesc.Usage = TextureUsage::COLOR_ATTACHMENT | TextureUsage::SAMPLED | TextureUsage::TRANSFER_SRC; // TRANSFER_SRC needed for blit
		colorDesc.DebugName = "Offscreen Color Texture";
		colorDesc.Sampler = mQuadSampler;
		mOffscreenColor = mRHI->CreateTexture(colorDesc);

		// Offscreen framebuffer
		FramebufferAttachment colorAttachment = {};
		colorAttachment.Handle = mOffscreenColor;
		colorAttachment.Load = LoadOp::CLEAR;
		colorAttachment.Store = StoreOp::STORE;

		FramebufferDesc fbDesc = {};
		fbDesc.ColorAttachments[0] = colorAttachment;
		fbDesc.ColorAttachmentCount = 1;
		fbDesc.Width = size.x;
		fbDesc.Height = size.y;
		fbDesc.DebugName = "Offscreen Framebuffer";

		mOffscreenFramebuffer = mRHI->CreateFramebuffer(fbDesc);
	
		Shader shader;
		shader.Load("Renderer2D.shader", ShaderType::VERTEX | ShaderType::FRAGMENT);

		PipelineDesc desc = {};
		desc.Shader_ = shader;
		desc.Raster.Cull = CullMode::NONE;
		desc.DebugName = "Quad Batch Pipeline";
		desc.TargetFramebuffer = mOffscreenFramebuffer;
		desc.TargetSwapchain = false;
		desc.Blend.Enable = true;
		mPipeline = mRHI->CreatePipeline(desc);

		uint8_t whitePixel[] = { 255, 255, 255, 255 };			
		TextureDesc texDesc = {};
		texDesc.Width = 1;
		texDesc.Height = 1;
		texDesc.Format = TextureFormat::RGBA8_UNORM ;
		texDesc.Usage = TextureUsage::SAMPLED | TextureUsage::TRANSFER_DST;
		texDesc.DebugName = "WhiteTexture";
		texDesc.InitialData = whitePixel;
		texDesc.DataSize = sizeof(whitePixel);
		texDesc.Sampler = mQuadSampler;
		mWhiteTexture = mRHI->CreateTexture(texDesc);
    }

    void Renderer::BeginFrame(const EditorCamera& camera)
    {
        SURGE_PROFILE_FUNC("Renderer::BeginFrame(EditorCamera)");
        mData->ViewMatrix = camera.GetViewMatrix();
        mData->ProjectionMatrix = camera.GetProjectionMatrix();
        mData->ViewProjection = mData->ProjectionMatrix * mData->ViewMatrix;
        mData->CameraPosition = camera.GetPosition();

		mTotalVertexCount = 0;
		mTotalQuadCount = 0;
		mCurrentFrameVertexOffset = 0;
		mCurrentFrameCtx = mRHI->BeginFrame();
		mRHI->CmdBeginRenderPass(mCurrentFrameCtx, mOffscreenFramebuffer, mClearColor);
    }

	void Renderer::BeginFrame(const RuntimeCamera& camera, const glm::mat4& transform)
	{
		SURGE_PROFILE_FUNC("Renderer::BeginFrame(Camera)");
		mData->ViewMatrix = glm::inverse(transform);
		mData->ProjectionMatrix = camera.GetProjectionMatrix();
		mData->ViewProjection = mData->ProjectionMatrix * mData->ViewMatrix;
		mData->CameraPosition = transform[3];
	
		mTotalVertexCount = 0;
		mTotalQuadCount = 0;
		mCurrentFrameVertexOffset = 0;

		// Begins command buffer recording (Off-screen pass)
		// [WE MUST HAVE JUST ONE PRIMARY COMMAND BUFFER PER FRAME as we are targetting mobile]	

		mCurrentFrameCtx = mRHI->BeginFrame();
		mRHI->CmdBeginRenderPass(mCurrentFrameCtx, mOffscreenFramebuffer, mClearColor);
		mRHI->BindBindlessSet(mCurrentFrameCtx, mPipeline);
	}

    void Renderer::EndFrame()
    {
        SURGE_PROFILE_FUNC("Renderer::EndFrame()");

		FlushBatch();
		mRHI->CmdEndRenderPass(mCurrentFrameCtx);

		mRHI->CmdBlitToSwapchain(mCurrentFrameCtx, mOffscreenColor); // Copy/blit offscreen color to swapchain backbuffer
		mRHI->CmdBeginSwapchainRenderpass(mCurrentFrameCtx);	

		// ImGui Render
		for (const auto& callback : mImGuiRenderCallbacks)
			callback();

		OnImGuiRender();;

		mRHI->ShowMetricsWindow(); //Shows internal metrics about the RHI resource pools

		mRHI->CmdEndSwapchainRenderpass(mCurrentFrameCtx);
		mRHI->EndFrame(mCurrentFrameCtx); // Stops command buffer recording
    }

	void Renderer::OnImGuiRender()
	{
		ImGui::Begin("Renderer2D Memory");
		ImGui::Text("Batches: %u / %u", (mTotalQuadCount + MAX_QUADS_PER_BATCH - 1) / MAX_QUADS_PER_BATCH, (MAX_QUADS_TOTAL + MAX_QUADS_PER_BATCH - 1) / MAX_QUADS_PER_BATCH);
		ImGui::Separator();

		if (mMaxQuadCountReached)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "Max quad count of Renderer2D reached!\nSome Quads were not rendered.");
			ImGui::Separator();
		}

		float totalVerticesUsed = (float)mTotalVertexCount;
		float maxVerticesPossible = (float)MAX_VERTICES;
		float usageRatio = totalVerticesUsed / maxVerticesPossible;

		float frameWeightMB = (sizeof(QuadVertex) * totalVerticesUsed) / 1024.0f / 1024.0f;
		float totalWeightMB = (sizeof(QuadVertex) * maxVerticesPossible) / 1024.0f / 1024.0f;
		ImGui::Text("GPU Vertex Buffer status: %.3f MB / %.3f MB", frameWeightMB, totalWeightMB);

		ImVec4 barColor = ImVec4(0.0f, 0.7f, 0.0f, 1.0f); // Green
		if (usageRatio > 0.65f) barColor = ImVec4(1.0f, 0.4f, 0.0f, 1.0f); // Orange
		if (usageRatio > 0.95f) barColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Red

		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);
		ImGui::ProgressBar(usageRatio, ImVec2(-1.0f, 0.0f));
		ImGui::Text("%u / %u Vertices", mTotalVertexCount, MAX_VERTICES);
		ImGui::PopStyleColor();

		ImGui::End();
	}

	void Renderer::Submit(const glm::mat4& transform, const glm::vec4& color, TextureHandle texture)
	{
		if (mCurrentBatch.QuadCount >= MAX_QUADS_PER_BATCH)
			FlushBatch();

		if (mTotalQuadCount >= MAX_QUADS_TOTAL)
		{
			Log<Severity::Warn>("Max Quads per frame reached!");
			mMaxQuadCountReached = true;
			return;
		}
		mMaxQuadCountReached = false;
		Uint texIndex = mRHI->GetBindlessIndex(texture.IsNull() ? mWhiteTexture : texture);

		static constexpr glm::vec4 sLocalPositions[4] = {
			{ 0.5f, -0.5f, 0.0f, 1.0f},
			{ 0.5f,  0.5f, 0.0f, 1.0f},
			{-0.5f,  0.5f, 0.0f, 1.0f},
			{-0.5f, -0.5f, 0.0f, 1.0f},
		};
		static constexpr glm::vec2 sUVs[4] = {
			{1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f}
		};

		for (Uint i = 0; i < 4; i++)
		{
			QuadVertex& v = mVertexData[mCurrentBatch.VertexCount++];
			v.Position = transform * sLocalPositions[i];
			v.Color = color;
			v.UV = sUVs[i];
			v.TextureIndex = texIndex;
		}

		mCurrentBatch.QuadCount++;
		mTotalQuadCount++;
	}

	void Renderer::OnWindowResize(Uint width, Uint height)
	{
		Core::AddFrameEndCallback([this, width, height]() {
				mRHI->ResizeTexture(mOffscreenColor, width, height);
				mRHI->ResizeFramebuffer(mOffscreenFramebuffer, width, height);
			});
		
		Log<Severity::Debug>("WindowResized // Latest dimensions: Width:{0} Height:{1}", width, height);
	}

	void Renderer::FlushBatch()
	{
		if (mCurrentBatch.QuadCount == 0)
			return;

		const FrameContext& ctx = mCurrentFrameCtx;

		// Upload current batch vertex data to GPU (only the portion needed for this batch, not the entire VB)
		Uint uploadOffsetInBytes = mCurrentFrameVertexOffset * sizeof(QuadVertex);
		Uint uploadSizeInBytes = mCurrentBatch.VertexCount * sizeof(QuadVertex);
		mRHI->UploadBuffer(mVertexBuffer, mVertexData.data(), uploadSizeInBytes, uploadOffsetInBytes);

		mRHI->CmdBindPipeline(ctx, mPipeline);
		mRHI->CmdBindVertexBuffer(ctx, mVertexBuffer, 0);
		mRHI->CmdBindIndexBuffer(ctx, mIndexBuffer, 0);

		QuadPushConstants push = { .ViewProj = mData->ViewProjection };
		mRHI->CmdPushConstants(ctx, mPipeline, ShaderType::VERTEX, 0, sizeof(QuadPushConstants), &push);
		mRHI->CmdDrawIndexed(ctx, mCurrentBatch.QuadCount * 6, 1, 0, (int32_t)mCurrentFrameVertexOffset, 0);

		mTotalVertexCount += mCurrentBatch.VertexCount;
		mCurrentFrameVertexOffset += mCurrentBatch.VertexCount;

		mCurrentBatch.Reset();
	}

	void Renderer::Shutdown()
    {
        SURGE_PROFILE_FUNC("Renderer::Shutdown()");
        mRHI->WaitIdle();
		mRHI->DestroyFramebuffer(mOffscreenFramebuffer);
		mRHI->DestroySampler(mQuadSampler);
		mRHI->DestroyTexture(mWhiteTexture);
		mRHI->DestroyTexture(mOffscreenColor);
		mRHI->DestroyPipeline(mPipeline);
        mRHI->DestroyBuffer(mVertexBuffer);
        mRHI->DestroyBuffer(mIndexBuffer);
        mRHI->Shutdown();
    }

} // namespace Surge
