// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RHI/RHIDevice.hpp"

// ---------------------------------------------------------------------------
// RHICommandBuffer.hpp
//   Free functions that delegate to the global RHIDevice dispatch table.
//   Higher-level systems (RenderGraph, ForwardRenderer, etc.) call these
//   instead of touching the dispatch table directly.
// ---------------------------------------------------------------------------

namespace Surge::RHI
{
    // ── Command buffer lifetime ─────────────────────────────────────────────
    SURGE_API void rhiAllocCommandBuffer(CommandBufferHandle& outHandle,
                                          QueueType queue = QueueType::Graphics);
    SURGE_API void rhiFreeCommandBuffer(CommandBufferHandle handle);
    SURGE_API void rhiBeginCommandBuffer(CommandBufferHandle handle);
    SURGE_API void rhiEndCommandBuffer(CommandBufferHandle handle);
    SURGE_API void rhiSubmitCommandBuffer(CommandBufferHandle handle,
                                           QueueType queue = QueueType::Graphics);

    // ── Render pass ─────────────────────────────────────────────────────────
    SURGE_API void rhiCmdBeginRenderPass(CommandBufferHandle cmd,
                                          FramebufferHandle fb,
                                          const ClearValue* clearValues,
                                          uint32_t clearCount);
    SURGE_API void rhiCmdEndRenderPass(CommandBufferHandle cmd);

    // ── Pipeline binding ────────────────────────────────────────────────────
    SURGE_API void rhiCmdBindGraphicsPipeline(CommandBufferHandle cmd,
                                               GraphicsPipelineHandle pipeline);
    SURGE_API void rhiCmdBindComputePipeline(CommandBufferHandle cmd,
                                              ComputePipelineHandle pipeline);

    // ── Geometry binding ────────────────────────────────────────────────────
    SURGE_API void rhiCmdBindVertexBuffer(CommandBufferHandle cmd,
                                           BufferHandle buf,
                                           uint64_t offset = 0);
    SURGE_API void rhiCmdBindIndexBuffer(CommandBufferHandle cmd,
                                          BufferHandle buf,
                                          uint64_t offset = 0);

    // ── Draw / Dispatch ─────────────────────────────────────────────────────
    SURGE_API void rhiCmdDrawIndexed(CommandBufferHandle cmd,
                                      uint32_t indexCount,
                                      uint32_t baseIndex  = 0,
                                      uint32_t baseVertex = 0);
    SURGE_API void rhiCmdDraw(CommandBufferHandle cmd,
                               uint32_t vertexCount,
                               uint32_t firstVertex = 0);
    SURGE_API void rhiCmdDispatch(CommandBufferHandle cmd,
                                   uint32_t x, uint32_t y, uint32_t z);

    // ── State ───────────────────────────────────────────────────────────────
    SURGE_API void rhiCmdSetViewport(CommandBufferHandle cmd, const Viewport& vp);
    SURGE_API void rhiCmdSetScissor(CommandBufferHandle cmd, const Rect2D& scissor);
    SURGE_API void rhiCmdPushConstants(CommandBufferHandle cmd,
                                        ShaderStage stages,
                                        const void* data,
                                        uint32_t size,
                                        uint32_t offset = 0);

    // ── Transfer ────────────────────────────────────────────────────────────
    SURGE_API void rhiCmdCopyBuffer(CommandBufferHandle cmd,
                                     BufferHandle src, BufferHandle dst,
                                     size_t size,
                                     size_t srcOffset = 0,
                                     size_t dstOffset = 0);

} // namespace Surge::RHI
