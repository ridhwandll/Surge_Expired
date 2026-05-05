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
#include <android_native_app_glue.h>
#include <android/asset_manager.h>
#include <android/log.h>
#endif

namespace Surge
{
    void Renderer::Initialize()
    {
        SURGE_PROFILE_FUNC("Renderer::Initialize()");
        mData = CreateScope<RendererData>();

		mRHI = CreateScope<GraphicsRHI>();
        mRHI->Initialize(Core::GetWindow());
        
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
		ibDesc.Size = sizeof(uint32_t) * MAX_INDICES;
		ibDesc.Usage = BufferUsage::INDEX;
		ibDesc.HostVisible = true;
		ibDesc.InitialData = indices.data();
		ibDesc.DebugName = "BatchIB";
		mIndexBuffer = mRHI->CreateBuffer(ibDesc);

		BufferDesc vbDesc = {};
		vbDesc.Size = sizeof(QuadVertex) * MAX_VERTICES;
		vbDesc.Usage = BufferUsage::VERTEX;
		vbDesc.HostVisible = true;   // Persistently mapped — memcpy every frame
		vbDesc.DebugName = "BatchVB";
		mVertexBuffer = mRHI->CreateBuffer(vbDesc);

		// CPU-side staging array fill this, then memcpy to GPU buffer
		mVertexData.resize(MAX_VERTICES);
		mVertexCount = 0;
		mQuadCount = 0;

		PipelineDesc desc = {};
		desc.VertShaderName = "Quad.vert";
		desc.FragShaderName = "Quad.frag";
		desc.Bindings[0] = { 0, sizeof(QuadVertex) };
		desc.BindingCount = 1;
		desc.Attributes[0] = { 0, 0, VertexFormat::FLOAT3, offsetof(QuadVertex, Position) };
		desc.Attributes[1] = { 1, 0, VertexFormat::FLOAT4, offsetof(QuadVertex, Color) };
		desc.Attributes[2] = { 2, 0, VertexFormat::FLOAT2, offsetof(QuadVertex, UV) };
		desc.AttributeCount = 3;
		desc.Raster.Cull = CullMode::NONE;
		desc.PushConstantSize = sizeof(QuadPushConstants);
		desc.DebugName = "BatchPipeline";
		mPipeline = mRHI->CreatePipeline(desc);
    }

    void Renderer::BeginFrame(const RuntimeCamera& camera, const glm::mat4& transform)
    {
        SURGE_PROFILE_FUNC("Renderer::BeginFrame(Camera)");
        mData->ViewMatrix = glm::inverse(transform);
        mData->ProjectionMatrix = camera.GetProjectionMatrix();
        mData->ViewProjection = mData->ProjectionMatrix * mData->ViewMatrix;
        mData->CameraPosition = transform[3];

		mVertexCount = 0;
		mQuadCount = 0;
    }

    void Renderer::BeginFrame(const EditorCamera& camera)
    {
        SURGE_PROFILE_FUNC("Renderer::BeginFrame(EditorCamera)");
        mData->ViewMatrix = camera.GetViewMatrix();
        mData->ProjectionMatrix = camera.GetProjectionMatrix();
        mData->ViewProjection = mData->ProjectionMatrix * mData->ViewMatrix;
        mData->CameraPosition = camera.GetPosition();
    }

    void Renderer::EndFrame()
    {
        SURGE_PROFILE_FUNC("Renderer::EndFrame()");
		if (mQuadCount == 0)
			return;

		mRHI->UploadBuffer(mVertexBuffer, mVertexData.data(), sizeof(QuadVertex) * mVertexCount, 0);

		// Begins command buffer recording
		// [WE MUST HAVE JUST ONE PRIMARY COMMAND BUFFER PER FRAME as we are targetting mobile]
        FrameContext ctx = mRHI->BeginFrame();

		// Off-screen passes go here

		mRHI->CmdBeginSwapchainRenderpass(ctx);

		// mRHI->BlitImage(sourceTexture); //In future when we have offscreen render targets, postprocessing, etc, we will need to blit or sample from those here.
		// For now we render directly to the swapchain image

		// Render directly to swapchain buffer, no offscreen renderpass or postprocessing yet
		mRHI->CmdBindPipeline(ctx, mPipeline);
		mRHI->CmdBindVertexBuffer(ctx, mVertexBuffer, 0);
		mRHI->CmdBindIndexBuffer(ctx, mIndexBuffer, 0);

		QuadPushConstants push = { .ViewProj = mData->ViewProjection };
		mRHI->CmdPushConstants(ctx, mPipeline, &push, sizeof(QuadPushConstants), 0);
		mRHI->CmdDrawIndexed(ctx, mQuadCount * 6, 1, 0, 0, 0);

		mRHI->CmdEndSwapchainRenderpass(ctx);
		mRHI->EndFrame(ctx); // Stops command buffer recording
    }

	void Renderer::Submit(const glm::mat4& transform, const glm::vec4& color)
	{
		SG_ASSERT(mQuadCount < MAX_QUADS, "BatchRenderer: exceeded MAX_QUADS");

		// Unit quad positions transform applied per vertex on CPU
		static constexpr glm::vec4 sLocalPositions[4] = {
			{ 0.5f, -0.5f, 0.0f, 1.0f}, // top right
			{ 0.5f,  0.5f, 0.0f, 1.0f}, // bottom right
			{-0.5f,  0.5f, 0.0f, 1.0f}, // bottom left
			{-0.5f, -0.5f, 0.0f, 1.0f}, // top left
		};

		static constexpr glm::vec2 sUVs[4] =
		{
			{1.0f, 0.0f}, // top right
			{1.0f, 1.0f}, // bottom right
			{0.0f, 1.0f}, // bottom left
			{0.0f, 0.0f}, // top left
		};

		for (Uint i = 0; i < 4; i++)
		{
			QuadVertex& v = mVertexData[mVertexCount++];
			v.Position = transform * sLocalPositions[i]; // CPU transform
			v.Color = color;
			v.UV = sUVs[i];
		}

		mQuadCount++;
	}

	void Renderer::Shutdown()
    {
        SURGE_PROFILE_FUNC("Renderer::Shutdown()");
        mRHI->InitiateShutdown();
		mRHI->DestroyPipeline(mPipeline);
        mRHI->DestroyBuffer(mVertexBuffer);
        mRHI->DestroyBuffer(mIndexBuffer);
        mRHI->Shutdown();
    }

} // namespace Surge
