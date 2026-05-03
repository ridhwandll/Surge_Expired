// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Window/Window.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include "Surge/Graphics/RHI/RHIDescriptors.hpp"
#include "Surge/Graphics/RHI/RHIFrameContext.hpp"

#if defined(SURGE_PLATFORM_WINDOWS) || defined(SURGE_PLATFORM_ANDROID)
#include "Surge/Graphics/RHI/Vulkan/VulkanRHI.hpp"
namespace Surge
{
	using BackendRHI = VulkanRHI;
}
#elif defined(SURGE_PLATFORM_APPLE)
// ERROR: This file doesn't exist yet, but will be implemented in the future when we add support for Apple platforms
#include "Surge/Graphics/RHI/Metal/MetalRHI.hpp"
namespace Surge
{
	using BackendRHI = MetalRHI;
}
#endif


namespace Surge
{
	class GraphicsRHI
	{
	public:
		GraphicsRHI() = default;
		~GraphicsRHI() = default;

		void Initialize(Window* window = nullptr) { mBackendRHI.Initialize(window); }
		void InitiateShutdown() { mBackendRHI.InitiateShutdown(); }
		void Shutdown() { mBackendRHI.Shutdown(); }

		FrameContext BeginFrame() { FrameContext ctx; mBackendRHI.BeginFrame(ctx); return ctx; }
		void EndFrame(const FrameContext& ctx) { mBackendRHI.EndFrame(ctx); }
		void Resize(Uint width, Uint height) { mBackendRHI.Resize(width, height); }

		// Buffer
		BufferHandle CreateBuffer(const BufferDesc& desc) { return mBackendRHI.CreateBuffer(desc); }
		void UploadBuffer(BufferHandle h, const void* data, Uint size, Uint offset) { mBackendRHI.UploadBuffer(h, data, size, offset); }
		void DestroyBuffer(BufferHandle buffer) { mBackendRHI.DestroyBuffer(buffer); }

		void CmdBindVertexBuffer(const FrameContext& ctx, BufferHandle h, Uint offset = 0) { mBackendRHI.CmdBindVertexBuffer(ctx, h, offset); }
		void CmdBindIndexBuffer(const FrameContext& ctx, BufferHandle h, Uint offset = 0) { mBackendRHI.CmdBindIndexBuffer(ctx, h, offset); }

		// Texture
		TextureHandle CreateTexture(const TextureDesc& desc, const void* initialData = nullptr) { return mBackendRHI.CreateTexture(desc, initialData); }
		void DestroyTexture(TextureHandle texture) { mBackendRHI.DestroyTexture(texture); }

		// Framebuffer
		FramebufferHandle CreateFramebuffer(const FramebufferDesc& desc, const void* initialData = nullptr) { return mBackendRHI.CreateFramebuffer(desc, initialData); }
		void DestroyFramebuffer(FramebufferHandle framebuffer) { mBackendRHI.DestroyFramebuffer(framebuffer); }

		// Pipeline
		PipelineHandle CreatePipeline(const PipelineDesc& desc) { return mBackendRHI.CreatePipeline(desc); }
		void DestroyPipeline(PipelineHandle h) { mBackendRHI.DestroyPipeline(h); }

		// DrawCommands
		void CmdDrawIndexed(const FrameContext& ctx, Uint indexCount,Uint instanceCount,Uint firstIndex,int32_t vertexOffset, Uint firstInstance) { mBackendRHI.CmdDrawIndexed(ctx, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance); }
		void CmdDraw(const FrameContext& ctx, Uint vertexCount, Uint instanceCount, Uint firstVertex, Uint firstInstance) { mBackendRHI.CmdDraw(ctx, vertexCount, instanceCount, firstVertex, firstInstance); }
		void CmdBindPipeline(const FrameContext& ctx, PipelineHandle h) { mBackendRHI.CmdBindPipeline(ctx, h); }
		void CmdPushConstants(const FrameContext& ctx, PipelineHandle h, const void* data, Uint size, Uint offset) { mBackendRHI.CmdPushConstants(ctx, h, data, size, offset); }

		// TODO: REMOVE
		BackendRHI& GetBackendRHI() { return mBackendRHI; }

	private:
		BackendRHI mBackendRHI;
	};
}
