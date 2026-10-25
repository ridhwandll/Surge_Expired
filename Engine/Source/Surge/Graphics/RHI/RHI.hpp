// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Window/Window.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include "Surge/Graphics/RHI/RHIDescs.hpp"
#include "Surge/Graphics/RHI/RHIFrameContext.hpp"

#if defined(SURGE_PLATFORM_WINDOWS) || defined(SURGE_PLATFORM_ANDROID)
#include "Surge/Graphics/RHI/Vulkan/VulkanRHI.hpp"
namespace Surge { using BackendRHI = VulkanRHI; }
#elif defined(SURGE_PLATFORM_APPLE)
#include "Surge/Graphics/RHI/Metal/MetalRHI.hpp" // ERROR: This file doesn't exist yet, but will be implemented in the future when we add support for Apple platforms
namespace Surge { using BackendRHI = MetalRHI; }
#endif

namespace Surge
{
    class GraphicsRHI
    {
    public:
        GraphicsRHI() = default;
        ~GraphicsRHI() = default;

        void Initialize(Window* window, bool enableImGui) { mBackendRHI.Initialize(window, enableImGui); } // enableImGui: ImGui context + GPU timing on by default
        void WaitIdle() { mBackendRHI.WaitIdle(); }
        void Shutdown() { mBackendRHI.Shutdown(); }

        FrameContext BeginFrame() { FrameContext ctx; mBackendRHI.BeginFrame(ctx); return ctx; }
        void EndFrame(const FrameContext& ctx) { mBackendRHI.EndFrame(ctx); }

        void Resize([[maybe_unused]] Uint width, [[maybe_unused]] Uint height) { mBackendRHI.Resize(); }
        void OnSurfaceDestroyed() { mBackendRHI.OnSurfaceDestroyed(); }
        void OnSurfaceCreated(Window* window) { mBackendRHI.OnSurfaceCreated(window); }
        bool SetTargetFrameRate(float fps) { return mBackendRHI.SetTargetFrameRate(fps); } // true = the RHI paces presentation itself
        void SetFramePacingEnabled(bool enabled) { mBackendRHI.SetFramePacingEnabled(enabled); } // Use Core's SetFramePacing, it also drops the CPU cap
        bool IsFormatSupported(ImageFormat format) const { return mBackendRHI.IsFormatSupported(format); }
        // Quarter turns (clockwise) passes drawing into the swapchain rotate their output by: Android pre-rotation, 0 elsewhere
        Uint GetSurfaceRotation() const { return mBackendRHI.GetSurfaceRotation(); }
        const RHIStats& GetStats() { return mBackendRHI.GetStats(); }

        // GPU timing: timestamp queries around nested named scopes (the RenderGraph times its groups and passes), read back FRAMES_IN_FLIGHT frames later
        // SetGPUTimingEnabled is the runtime switch (applies at the next BeginFrame, on by default in the Editor), RHISettings::GPU_TIMING compiles it out
        void SetGPUTimingEnabled(bool enabled) { mBackendRHI.SetGPUTimingEnabled(enabled); }
        bool IsGPUTimingEnabled() const { return mBackendRHI.IsGPUTimingEnabled(); }
        bool SupportsGPUTiming() const { return mBackendRHI.SupportsGPUTiming(); }
        bool SupportsGPUTimingInRenderPass() const { return mBackendRHI.SupportsGPUTimingInRenderPass(); } // False on tile based GPUs: time whole render passes only
        const GPUTimings& GetGPUTimings() const { return mBackendRHI.GetGPUTimings(); }
        void CmdBeginGPUScope([[maybe_unused]] const FrameContext& ctx, [[maybe_unused]] std::string_view name) { if constexpr (RHISettings::GPU_TIMING) mBackendRHI.CmdBeginGPUScope(ctx, name); }
        void CmdEndGPUScope([[maybe_unused]] const FrameContext& ctx) { if constexpr (RHISettings::GPU_TIMING) mBackendRHI.CmdEndGPUScope(ctx); }
        // Right before the swapchain render pass (outside of any render pass): times the GPU's wait for the swapchain image on its own, as
        // GPUTimings::DisplayWaitMilliseconds instead of work. Records nothing while GPU timing is off
        void CmdWaitForSwapchainImage([[maybe_unused]] const FrameContext& ctx) { if constexpr (RHISettings::GPU_TIMING) mBackendRHI.CmdWaitForSwapchainImage(ctx); }

        // Buffer
        BufferHandle CreateBuffer(const BufferDesc& desc) { return mBackendRHI.CreateBuffer(desc); }
        void UploadBuffer(BufferHandle h, const void* data, Uint size, Uint offset = 0) { mBackendRHI.UploadBuffer(h, data, size, offset); }
        void DestroyBuffer(BufferHandle buffer) { mBackendRHI.DestroyBuffer(buffer); }

        // Image
        ImageHandle CreateImage(const ImageDesc& desc) { return mBackendRHI.CreateImage(desc); }
        void DestroyImage(ImageHandle texture) { mBackendRHI.DestroyImage(texture); }
        void ResizeImage(ImageHandle h, Uint newWidth, Uint newHeight) { mBackendRHI.ResizeImage(h, newWidth, newHeight); }
        void RecreateImage(ImageHandle h, const ImageDesc& desc) { mBackendRHI.RecreateImage(h, desc); } // Same handle, new size/format/usage, content undefined
        uint64_t GetImageSize(ImageHandle h) const { return mBackendRHI.GetImageSize(h); }
        const ImageDesc& GetDesc(ImageHandle h) const { return mBackendRHI.GetDesc(h); }
        bool IsValid(ImageHandle h) const { return mBackendRHI.IsValid(h); }                     // False once destroyed (stale handle)
        bool IsImageSampleable(ImageHandle h) const { return mBackendRHI.IsImageSampleable(h); } // Currently in the SAMPLED layout: safe to show with ImGui

        // Framebuffer
        FramebufferHandle CreateFramebuffer(const FramebufferDesc& desc) { return mBackendRHI.CreateFramebuffer(desc); }
        void DestroyFramebuffer(FramebufferHandle h) { mBackendRHI.DestroyFramebuffer(h); }
        void ResizeFramebuffer(FramebufferHandle h, Uint newWidth, Uint newHeight, bool resizeImages = true) { mBackendRHI.ResizeFramebuffer(h, newWidth, newHeight, resizeImages); }
        void RecreateFramebuffer(FramebufferHandle h, const FramebufferDesc& desc) { mBackendRHI.RecreateFramebuffer(h, desc); }
        const FramebufferDesc& GetDesc(FramebufferHandle h) const { return mBackendRHI.GetDesc(h); }

        // Pipeline
        PipelineHandle CreatePipeline(const PipelineDesc& desc) { return mBackendRHI.CreatePipeline(desc); }
        PipelineHandle CreateComputePipeline(const ComputePipelineDesc& desc) { return mBackendRHI.CreateComputePipeline(desc); }
        bool SupportsCompute() const { return mBackendRHI.SupportsCompute(); }
        Uint GetMaxImageDimension2D() const { return mBackendRHI.GetMaxImageDimension2D(); }
        void DestroyPipeline(PipelineHandle h) { mBackendRHI.DestroyPipeline(h); }

        // Samplers
        SamplerHandle CreateSampler(const SamplerDesc& desc) { return mBackendRHI.CreateSampler(desc); }
        void DestroySampler(SamplerHandle h) { mBackendRHI.DestroySampler(h); }

        // Descriptor sets
        DescriptorSetHandle CreateDescriptorSet(PipelineHandle pipelineHandle, DescriptorSetSlot slot, DescriptorUpdateFrequency frequency, const char* debugName = nullptr) { return mBackendRHI.CreateDescriptorSet(pipelineHandle, slot, frequency, debugName); }
        void UpdateDescriptorSet(DescriptorSetHandle setHandle, const DescriptorWrite* writes, Uint writeCount, Uint frameIndex) { mBackendRHI.UpdateDescriptorSet(setHandle, writes, writeCount, frameIndex); }
        void DestroyDescriptorSet(DescriptorSetHandle h) { mBackendRHI.DestroyDescriptorSet(h); }

        // Commands
        void CmdDrawIndexed(const FrameContext& ctx, Uint indexCount, Uint instanceCount, Uint firstIndex, int32_t vertexOffset, Uint firstInstance) { mBackendRHI.CmdDrawIndexed(ctx, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance); }
        void CmdDraw(const FrameContext& ctx, Uint vertexCount, Uint instanceCount, Uint firstVertex, Uint firstInstance) { mBackendRHI.CmdDraw(ctx, vertexCount, instanceCount, firstVertex, firstInstance); }

        // Compute: record outside of a renderpass. Storage writes need the image in ImageUsage::STORAGE (GENERAL), reads in ImageUsage::SAMPLED
        // Push constants for compute pipelines use ShaderType::COMPUTE
        void CmdDispatch(const FrameContext& ctx, Uint groupCountX, Uint groupCountY, Uint groupCountZ = 1) { mBackendRHI.CmdDispatch(ctx, groupCountX, groupCountY, groupCountZ); }

        void CmdBindVertexBuffer(const FrameContext& ctx, BufferHandle h, Uint offset = 0) { mBackendRHI.CmdBindVertexBuffer(ctx, h, offset); }
        void CmdBindIndexBuffer(const FrameContext& ctx, BufferHandle h, Uint offset = 0) { mBackendRHI.CmdBindIndexBuffer(ctx, h, offset); }
        void CmdBindPipeline(const FrameContext& ctx, PipelineHandle h) { mBackendRHI.CmdBindPipeline(ctx, h); }
        void CmdBindDescriptorSet(const FrameContext& ctx, PipelineHandle pipeline, DescriptorSetHandle setHandle, DescriptorSetSlot slot) { mBackendRHI.CmdBindDescriptorSet(ctx, pipeline, setHandle, slot); }

        void CmdPushConstants(const FrameContext& ctx, PipelineHandle h, ShaderType shaderStage, Uint offset, Uint size, const void* data) { mBackendRHI.CmdPushConstants(ctx, h, shaderStage, offset, size, data); }

        [[deprecated("Blit to swapchain won't not work correctly if used, rather use a FullscreenTriangle instead!")]]
        void CmdBlitToSwapchain(const FrameContext& ctx, ImageHandle srcHandle) { mBackendRHI.CmdBlitToSwapchain(ctx, srcHandle); }

        void CmdBeginSwapchainRenderpass(const FrameContext& ctx) { mBackendRHI.CmdBeginSwapchainRenderpass(ctx); }
        void CmdEndSwapchainRenderpass(const FrameContext& ctx) { mBackendRHI.CmdEndSwapchainRenderpass(ctx); }

        void CmdBeginRenderPass(const FrameContext& ctx, FramebufferHandle h) { mBackendRHI.CmdBeginRenderPass(ctx, h); }
        void CmdEndRenderPass(const FrameContext& ctx, FramebufferHandle h) { mBackendRHI.CmdEndRenderPass(ctx, h); }
        void CmdTransitionImageLayout(const FrameContext& ctx, ImageHandle h, ImageUsage newLayout) { mBackendRHI.CmdTransitionImageLayout(ctx, h, newLayout); }
        void CmdSetDepthBias(const FrameContext& ctx, float constantFactor, float clamp, float slopeFactor) { mBackendRHI.CmdSetDepthBias(ctx, constantFactor, clamp, slopeFactor); }
        void CmdSetViewport(const FrameContext& ctx, Uint x, Uint y, Uint width, Uint height) { mBackendRHI.CmdSetViewport(ctx, x, y, width, height); } // Viewport + scissor, inside a render pass (Example: one region of a shadow atlas)
        void ShowMetricsWindow(float frameTimeMilliseconds) { mBackendRHI.ShowPoolDebugImGuiWindow(frameTimeMilliseconds); }

        ImTextureID AddImGuiImage(ImageHandle h) { return mBackendRHI.AddImGuiImage(h); }
        ImTextureID GetImGuiImage(ImageHandle h) { return mBackendRHI.GetImGuiImage(h); }

        BackendRHI& GetBackendRHI() { return mBackendRHI; } // TODO: REMOVE
    private:
        BackendRHI mBackendRHI;
    };
}
