// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RHI/RHIResources.hpp"

// ---------------------------------------------------------------------------
// RHIDevice.hpp
//   Backend dispatch table – a plain struct of function pointers.
//   No vtable. No virtual calls per draw.
//
//   Pattern inspired by bgfx RendererContextI and LLGL backend selection:
//   The active backend fills this table once at startup and registers it
//   via SetRHIDevice(). All higher-level code then calls GetRHIDevice() to
//   route API calls to the correct backend implementation.
// ---------------------------------------------------------------------------

namespace Surge::RHI
{
    struct SURGE_API RHIDevice
    {
        // ── Buffer ─────────────────────────────────────────────────────────
        BufferHandle (*createBuffer)(const BufferDesc&)                              = nullptr;
        void         (*destroyBuffer)(BufferHandle)                                  = nullptr;
        void         (*uploadBuffer)(BufferHandle, const void* data, size_t size,
                                     size_t offset)                                  = nullptr;
        void*        (*mapBuffer)(BufferHandle)                                      = nullptr;
        void         (*unmapBuffer)(BufferHandle)                                    = nullptr;

        // ── Texture ────────────────────────────────────────────────────────
        TextureHandle (*createTexture)(const TextureDesc&)                           = nullptr;
        void          (*destroyTexture)(TextureHandle)                               = nullptr;
        void          (*uploadTexture)(TextureHandle, const void* data, size_t size) = nullptr;

        // ── Shader ─────────────────────────────────────────────────────────
        ShaderHandle (*createShader)(const ShaderDesc&)                              = nullptr;
        void         (*destroyShader)(ShaderHandle)                                  = nullptr;

        // ── Pipelines ──────────────────────────────────────────────────────
        GraphicsPipelineHandle (*createGraphicsPipeline)(const GraphicsPipelineDesc&) = nullptr;
        void                   (*destroyGraphicsPipeline)(GraphicsPipelineHandle)      = nullptr;
        ComputePipelineHandle  (*createComputePipeline)(const ComputePipelineDesc&)    = nullptr;
        void                   (*destroyComputePipeline)(ComputePipelineHandle)         = nullptr;

        // ── Render Pass / Framebuffer ──────────────────────────────────────
        RenderPassHandle  (*createRenderPass)(const RenderPassDesc&)                 = nullptr;
        void              (*destroyRenderPass)(RenderPassHandle)                     = nullptr;
        FramebufferHandle (*createFramebuffer)(const FramebufferDesc&)               = nullptr;
        void              (*destroyFramebuffer)(FramebufferHandle)                   = nullptr;

        // ── Command Buffer ─────────────────────────────────────────────────
        CommandBufferHandle (*allocCommandBuffer)(QueueType)                         = nullptr;
        void                (*freeCommandBuffer)(CommandBufferHandle)                = nullptr;
        void                (*beginCommandBuffer)(CommandBufferHandle)               = nullptr;
        void                (*endCommandBuffer)(CommandBufferHandle)                 = nullptr;
        void                (*submitCommandBuffer)(CommandBufferHandle, QueueType)   = nullptr;

        // ── Draw / Dispatch Commands ───────────────────────────────────────
        void (*cmdBeginRenderPass)(CommandBufferHandle, FramebufferHandle,
                                   const ClearValue*, uint32_t clearCount)           = nullptr;
        void (*cmdEndRenderPass)(CommandBufferHandle)                                = nullptr;
        void (*cmdBindGraphicsPipeline)(CommandBufferHandle,
                                        GraphicsPipelineHandle)                      = nullptr;
        void (*cmdBindComputePipeline)(CommandBufferHandle,
                                       ComputePipelineHandle)                        = nullptr;
        void (*cmdBindVertexBuffer)(CommandBufferHandle, BufferHandle,
                                    uint64_t offset)                                 = nullptr;
        void (*cmdBindIndexBuffer)(CommandBufferHandle, BufferHandle,
                                   uint64_t offset)                                  = nullptr;
        void (*cmdDrawIndexed)(CommandBufferHandle, uint32_t indexCount,
                               uint32_t baseIndex, uint32_t baseVertex)              = nullptr;
        void (*cmdDraw)(CommandBufferHandle, uint32_t vertexCount,
                        uint32_t firstVertex)                                        = nullptr;
        void (*cmdDispatch)(CommandBufferHandle,
                            uint32_t x, uint32_t y, uint32_t z)                     = nullptr;
        void (*cmdSetViewport)(CommandBufferHandle, const Viewport&)                 = nullptr;
        void (*cmdSetScissor)(CommandBufferHandle, const Rect2D&)                    = nullptr;
        void (*cmdPushConstants)(CommandBufferHandle, ShaderStage,
                                 const void* data, uint32_t size, uint32_t offset)  = nullptr;
        void (*cmdCopyBuffer)(CommandBufferHandle, BufferHandle src, BufferHandle dst,
                              size_t size, size_t srcOffset, size_t dstOffset)       = nullptr;

        // ── Swapchain ──────────────────────────────────────────────────────
        SwapchainHandle (*createSwapchain)(const SwapchainDesc&)                    = nullptr;
        void            (*destroySwapchain)(SwapchainHandle)                        = nullptr;
        void            (*swapchainBeginFrame)(SwapchainHandle)                     = nullptr;
        void            (*swapchainEndFrame)(SwapchainHandle, TextureHandle blitSrc) = nullptr;
        void            (*swapchainResize)(SwapchainHandle, uint32_t w, uint32_t h) = nullptr;
        uint32_t        (*swapchainGetFrameIndex)(SwapchainHandle)                  = nullptr;
        uint32_t        (*swapchainGetImageCount)(SwapchainHandle)                  = nullptr;
    };

    // ── Global device accessor ──────────────────────────────────────────────
    // Set once by the active backend (e.g. VulkanRHIDevice::Initialize calls
    // SetRHIDevice(&gVulkanRHIDevice.GetDispatchTable())).
    SURGE_API void      SetRHIDevice(RHIDevice* device);
    SURGE_API RHIDevice* GetRHIDevice();

} // namespace Surge::RHI
