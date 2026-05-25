// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Window/Window.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include "Surge/Graphics/RHI/RHIDescs.hpp"
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
        void WaitIdle() { mBackendRHI.WaitIdle(); }
        void Shutdown() { mBackendRHI.Shutdown(); }

        FrameContext BeginFrame() { FrameContext ctx; mBackendRHI.BeginFrame(ctx); return ctx; }
        void EndFrame(const FrameContext& ctx) { mBackendRHI.EndFrame(ctx); }

        void Resize(Uint width, Uint height) { mBackendRHI.Resize(); }
        const RHIStats& GetStats() { return mBackendRHI.GetStats(); }

        // Buffer
        BufferHandle CreateBuffer(const BufferDesc& desc) { return mBackendRHI.CreateBuffer(desc); }
        void UploadBuffer(BufferHandle h, const void* data, Uint size, Uint offset = 0) { mBackendRHI.UploadBuffer(h, data, size, offset); }
        void DestroyBuffer(BufferHandle buffer) { mBackendRHI.DestroyBuffer(buffer); }

        // Texture
        ImageHandle CreateImage(const ImageDesc& desc) { return mBackendRHI.CreateImage(desc); }
        void DestroyImage(ImageHandle texture) { mBackendRHI.DestroyImage(texture); }
        void ResizeImage(ImageHandle h, Uint newWidth, Uint newHeight) { mBackendRHI.ResizeImage(h, newWidth, newHeight); }
        const ImageDesc& GetDesc(ImageHandle h) const { return mBackendRHI.GetDesc(h); }

        // Framebuffer
        FramebufferHandle CreateFramebuffer(const FramebufferDesc& desc) { return mBackendRHI.CreateFramebuffer(desc); }
        void DestroyFramebuffer(FramebufferHandle h) { mBackendRHI.DestroyFramebuffer(h); }
        void ResizeFramebuffer(FramebufferHandle h, Uint newWidth, Uint newHeight) { mBackendRHI.ResizeFramebuffer(h, newWidth, newHeight); }
        const FramebufferDesc& GetDesc(FramebufferHandle h) const { return mBackendRHI.GetDesc(h); }

        // Pipeline
        PipelineHandle CreatePipeline(const PipelineDesc& desc) { return mBackendRHI.CreatePipeline(desc); }
        void DestroyPipeline(PipelineHandle h) { mBackendRHI.DestroyPipeline(h); }

        // Samplers
        SamplerHandle CreateSampler(const SamplerDesc& desc) { return mBackendRHI.CreateSampler(desc); }
        void DestroySampler(SamplerHandle h) { mBackendRHI.DestroySampler(h); }

        // Descriptor sets
        DescriptorSetHandle CreateDescriptorSet(PipelineHandle pipelineHandle, DescriptorSetSlot slot, DescriptorUpdateFrequency frequency, const char* debugName = nullptr) { return mBackendRHI.CreateDescriptorSet(pipelineHandle, slot, frequency, debugName); }
        void UpdateDescriptorSet(DescriptorSetHandle setHandle, const DescriptorWrite* writes, Uint writeCount, Uint frameIndex) { mBackendRHI.UpdateDescriptorSet(setHandle, writes, writeCount, frameIndex); }
        void DestroyDescriptorSet(DescriptorSetHandle h) { mBackendRHI.DestroyDescriptorSet(h); }

        // Commands
        void CmdDrawIndexed(const FrameContext& ctx, Uint indexCount,Uint instanceCount,Uint firstIndex,int32_t vertexOffset, Uint firstInstance) { mBackendRHI.CmdDrawIndexed(ctx, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance); }
        void CmdDraw(const FrameContext& ctx, Uint vertexCount, Uint instanceCount, Uint firstVertex, Uint firstInstance) { mBackendRHI.CmdDraw(ctx, vertexCount, instanceCount, firstVertex, firstInstance); }

        void CmdBindVertexBuffer(const FrameContext& ctx, BufferHandle h, Uint offset = 0) { mBackendRHI.CmdBindVertexBuffer(ctx, h, offset); }
        void CmdBindIndexBuffer(const FrameContext& ctx, BufferHandle h, Uint offset = 0) { mBackendRHI.CmdBindIndexBuffer(ctx, h, offset); }
        void CmdBindPipeline(const FrameContext& ctx, PipelineHandle h) { mBackendRHI.CmdBindPipeline(ctx, h); }
        void CmdBindDescriptorSet(const FrameContext& ctx, PipelineHandle pipeline, DescriptorSetHandle setHandle, DescriptorSetSlot slot) { mBackendRHI.CmdBindDescriptorSet(ctx, pipeline, setHandle, slot); }

        void CmdPushConstants(const FrameContext& ctx, PipelineHandle h, ShaderType shaderStage, Uint offset, Uint size, const void* data) { mBackendRHI.CmdPushConstants(ctx, h, shaderStage, offset, size, data); }

        [[deprecated("Blit to swapchain won't not work correctly if used, rather use a FullscreenTriangle instead!")]]
        void CmdBlitToSwapchain(const FrameContext& ctx, ImageHandle srcHandle) { mBackendRHI.CmdBlitToSwapchain(ctx, srcHandle); }

        void CmdBeginSwapchainRenderpass(const FrameContext& ctx) { mBackendRHI.CmdBeginSwapchainRenderpass(ctx); }
        void CmdEndSwapchainRenderpass(const FrameContext& ctx) { mBackendRHI.CmdEndSwapchainRenderpass(ctx); }

        void CmdBeginRenderPass(const FrameContext& ctx, FramebufferHandle h, glm::vec4 clearColor = { 1.0f, 0.0f, 1.0f, 1.0f }) { mBackendRHI.CmdBeginRenderPass(ctx, h, clearColor); }
        void CmdEndRenderPass(const FrameContext& ctx, FramebufferHandle h) { mBackendRHI.CmdEndRenderPass(ctx, h); }
        void CmdTransitionImageLayout(const FrameContext& ctx, ImageHandle h, ImageUsage newLayout) { mBackendRHI.CmdTransitionImageLayout(ctx, h, newLayout); }

        void ShowMetricsWindow() { mBackendRHI.ShowPoolDebugImGuiWindow(); }

        ImTextureID AddImGuiImage(ImageHandle h) { return mBackendRHI.AddImGuiImage(h); }
        ImTextureID GetImGuiImage(ImageHandle h) { return mBackendRHI.GetImGuiImage(h); }

        //BackendRHI& GetBackendRHI() { return mBackendRHI; } // TODO: REMOVE
    private:
        BackendRHI mBackendRHI;
    };
}
