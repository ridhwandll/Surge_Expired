// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RHI/Vulkan/VulkanResources.hpp"

// ---------------------------------------------------------------------------
// VulkanRHICommandBuffer.hpp
//   Vulkan implementations of the RHI command-buffer free functions.
//   These are installed into the RHIDevice dispatch table by VulkanRHIDevice.
// ---------------------------------------------------------------------------

namespace Surge::RHI
{
    // ── Lifetime ─────────────────────────────────────────────────────────────
    CommandBufferHandle vkRHIAllocCommandBuffer(QueueType queue);
    void                vkRHIFreeCommandBuffer(CommandBufferHandle handle);
    void                vkRHIBeginCommandBuffer(CommandBufferHandle handle);
    void                vkRHIEndCommandBuffer(CommandBufferHandle handle);
    void                vkRHISubmitCommandBuffer(CommandBufferHandle handle, QueueType queue);

    // ── Render pass ───────────────────────────────────────────────────────────
    void vkRHICmdBeginRenderPass(CommandBufferHandle cmd, FramebufferHandle fb,
                                  const ClearValue* clearValues, uint32_t clearCount);
    void vkRHICmdEndRenderPass(CommandBufferHandle cmd);

    // ── Pipeline binding ──────────────────────────────────────────────────────
    void vkRHICmdBindGraphicsPipeline(CommandBufferHandle cmd, GraphicsPipelineHandle pipeline);
    void vkRHICmdBindComputePipeline(CommandBufferHandle cmd, ComputePipelineHandle pipeline);

    // ── Geometry binding ──────────────────────────────────────────────────────
    void vkRHICmdBindVertexBuffer(CommandBufferHandle cmd, BufferHandle buf, uint64_t offset);
    void vkRHICmdBindIndexBuffer(CommandBufferHandle cmd, BufferHandle buf, uint64_t offset);

    // ── Draw / Dispatch ───────────────────────────────────────────────────────
    void vkRHICmdDrawIndexed(CommandBufferHandle cmd, uint32_t indexCount,
                              uint32_t baseIndex, uint32_t baseVertex);
    void vkRHICmdDraw(CommandBufferHandle cmd, uint32_t vertexCount, uint32_t firstVertex);
    void vkRHICmdDispatch(CommandBufferHandle cmd, uint32_t x, uint32_t y, uint32_t z);

    // ── State ─────────────────────────────────────────────────────────────────
    void vkRHICmdSetViewport(CommandBufferHandle cmd, const Viewport& vp);
    void vkRHICmdSetScissor(CommandBufferHandle cmd, const Rect2D& scissor);
    void vkRHICmdPushConstants(CommandBufferHandle cmd, ShaderStage stages,
                                const void* data, uint32_t size, uint32_t offset);

    // ── Transfer ──────────────────────────────────────────────────────────────
    void vkRHICmdCopyBuffer(CommandBufferHandle cmd, BufferHandle src, BufferHandle dst,
                             size_t size, size_t srcOffset, size_t dstOffset);

} // namespace Surge::RHI
