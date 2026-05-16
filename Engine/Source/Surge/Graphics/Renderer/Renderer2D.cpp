// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Renderer/Renderer2D.hpp"
#include "Surge/Graphics/Camera/EditorCamera.hpp"
#include "Surge/Core/Core.hpp"

namespace Surge
{
	static constexpr Uint MAX_VERTICES = Renderer2D::MAX_QUADS_TOTAL * 4;
	static constexpr Uint MAX_INDICES = Renderer2D::MAX_QUADS_PER_BATCH * 6; // IB of 1 batch, we reuse this

    void Renderer2D::Initialize(GraphicsRHI* rhi, RendererData* data)
    {
        SURGE_PROFILE_FUNC("Renderer2D::Initialize()");
        mRHI = rhi;
        mData = data;
       
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
		ibDesc.HostVisible = false;
		ibDesc.InitialData = indices.data();
		ibDesc.DebugName = "BatchIB";
		mIndexBuffer = mRHI->CreateBuffer(ibDesc);

		// We allocate a large VBs[RHI_FRAMES_IN_FLIGHT] for max vertices across all batches, but we only fill the portion
		// needed for the current batch each frame. This is to avoid GPU buffer creation stalls when flushing mid-frame after each batch is submitted.
		BufferDesc vbDesc = {};
		vbDesc.Size = sizeof(QuadVertex) * MAX_VERTICES;
		vbDesc.Usage = BufferUsage::VERTEX;
		vbDesc.HostVisible = true;
		for (Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
		{
			vbDesc.DebugName = std::format("BatchVB Frame: {}", i).c_str();
			mVertexBuffers[i] = mRHI->CreateBuffer(vbDesc);
		}

		mCurrentBatch.Reset();
		// CPU side staging array for 1 batch fill this, then memcpy-ied to GPU buffer
		mCurrentBatch.VertexData.resize(MAX_QUADS_PER_BATCH * 4);
	
		Shader shader;
		shader.Load("Renderer2D.glsl", ShaderType::VERTEX | ShaderType::FRAGMENT);

		PipelineDesc desc = {};
		desc.Shader_ = shader;
		desc.Raster.Cull = CullMode::NONE;
		desc.DebugName = "Renderer2D Pipeline";
		desc.TargetFramebuffer = mData->mOffscreenFramebuffer;
		desc.TargetSwapchain = false;
		desc.Blend.Enable = true;
		mPipeline = mRHI->CreatePipeline(desc);

		// Amount of max draw calls
		mDrawCommands.reserve(MAX_QUADS_TOTAL / MAX_QUADS_PER_BATCH);
    }

	void Renderer2D::Shutdown()
    {
        SURGE_PROFILE_FUNC("Renderer::Shutdown()");

		mRHI->DestroyPipeline(mPipeline);

        for (Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
			mRHI->DestroyBuffer(mVertexBuffers[i]);

		mRHI->DestroyBuffer(mIndexBuffer);
    }

	void Renderer2D::BeginFrame(const FrameContext& frameCtx)
	{
		SURGE_PROFILE_FUNC("Renderer::BeginFrame(Camera)");
	
		mTotalVertexCount = 0;
		mTotalQuadCount = 0;

		// Begins command buffer recording (Off-screen pass)
		// [WE MUST HAVE JUST ONE PRIMARY COMMAND BUFFER PER FRAME as we are targetting mobile]	
		mCurrentFrameCtx = frameCtx;
		mCurrentFrameVertexOffset = 0;

		mRHI->BindBindlessSet(mCurrentFrameCtx, mPipeline);
	}

	void Renderer2D::Submit(const glm::mat4& transform, const glm::vec4& color, TextureHandle texture)
	{
		if (mCurrentBatch.QuadCount >= MAX_QUADS_PER_BATCH)
			WriteToGPUBuffer();

		if (mTotalQuadCount >= MAX_QUADS_TOTAL)
		{
			Log<Severity::Warn>("Max Quads per frame reached!");
			mMaxQuadCountReached = true;
			return;
		}
		mMaxQuadCountReached = false;
		Uint texIndex = mRHI->GetBindlessTextureIndex(texture.IsNull() ? mData->mWhiteTexture : texture);

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
			QuadVertex& v = mCurrentBatch.VertexData[mCurrentBatch.VertexCount++];
			v.Position = transform * sLocalPositions[i];
			v.Color = glm::packUnorm4x8(color);
			v.UV = sUVs[i];
			v.TextureIndex = texIndex;
		}

		mCurrentBatch.QuadCount++;
		mTotalQuadCount++;
	}

    void Renderer2D::EndFrame()
    {
        SURGE_PROFILE_FUNC("Renderer::EndFrame()");

		WriteToGPUBuffer();

		if (mDrawCommands.empty())
			return;

		mRHI->CmdBindPipeline(mCurrentFrameCtx, mPipeline);
		mRHI->CmdBindVertexBuffer(mCurrentFrameCtx, mVertexBuffers[mCurrentFrameCtx.FrameIndex], 0);
		mRHI->CmdBindIndexBuffer(mCurrentFrameCtx, mIndexBuffer, 0);

		// quadData is just a placeholder to match the 3D shader
		PushConstantData pushConstants = { .Transform = glm::mat4(1.0f), .LightCount = 0};
		mRHI->CmdPushConstants(mCurrentFrameCtx, mPipeline, ShaderType::VERTEX | ShaderType::FRAGMENT, 0, sizeof(PushConstantData), &pushConstants);

		for (const QuadDrawCmd& cmd : mDrawCommands)
			mRHI->CmdDrawIndexed(mCurrentFrameCtx, cmd.QuadCount * 6, 1, 0, (int32_t)cmd.VertexOffset, 0);

		mDrawCommands.clear();
	}

	void Renderer2D::WriteToGPUBuffer()
	{
		if (mCurrentBatch.QuadCount == 0)
			return;

		const FrameContext& ctx = mCurrentFrameCtx;

		// Upload current batch vertex data to GPU (only the portion needed for this batch, not the entire VB)
		Uint uploadOffsetInBytes = mCurrentFrameVertexOffset * sizeof(QuadVertex);
		Uint uploadSizeInBytes = mCurrentBatch.VertexCount * sizeof(QuadVertex);
		mRHI->UploadBuffer(mVertexBuffers[ctx.FrameIndex], mCurrentBatch.VertexData.data(), uploadSizeInBytes, uploadOffsetInBytes);

		mDrawCommands.emplace_back(QuadDrawCmd{mCurrentFrameVertexOffset, mCurrentBatch.QuadCount});

		mTotalVertexCount += mCurrentBatch.VertexCount;
		mCurrentFrameVertexOffset += mCurrentBatch.VertexCount;
		mCurrentBatch.Reset();
	}

	void Renderer2D::OnWindowResize(Uint width, Uint height)
	{
	}

	void Renderer2D::OnImGuiRender()
	{
		ImFont* boldFont = ImGui::GetIO().Fonts->Fonts[1];
		ImGui::PushFont(boldFont, 25.0f);
		ImGui::TextUnformatted("Renderer2D");
		ImGui::Separator();
		ImGui::PopFont();

		ImGui::Text("Batches: %u / %u", (mTotalQuadCount + MAX_QUADS_PER_BATCH - 1) / MAX_QUADS_PER_BATCH, (MAX_QUADS_TOTAL + MAX_QUADS_PER_BATCH - 1) / MAX_QUADS_PER_BATCH);

		if (mMaxQuadCountReached)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "Max quad count of Renderer2D reached!\nSome Quads were not rendered.");
			ImGui::Separator();
		}

		float totalVerticesUsed = (float)mTotalVertexCount;
		float usageRatio = totalVerticesUsed / MAX_VERTICES;

		float frameWeightMB = (sizeof(QuadVertex) * totalVerticesUsed) / 1024.0f / 1024.0f;
		float totalWeightMB = (sizeof(QuadVertex) * MAX_VERTICES) / 1024.0f / 1024.0f;
		ImGui::Text("Vertex Buffer(GPU): %.1f MB / %.1f MB", frameWeightMB, totalWeightMB);

		ImVec4 barColor = ImVec4(0.0f, 0.7f, 0.0f, 1.0f); // Green
		if (usageRatio > 0.65f) barColor = ImVec4(1.0f, 0.4f, 0.0f, 1.0f); // Orange
		if (usageRatio > 0.95f) barColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Red

		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);
		ImGui::ProgressBar(usageRatio, ImVec2(-1.0f, 0.0f));
		ImGui::Text("%u / %u Vertices", mTotalVertexCount, MAX_VERTICES);
		ImGui::PopStyleColor();
		ImGui::ColorEdit4("Clear Color", (float*)&mData->mClearColor);
	}

} // namespace Surge
