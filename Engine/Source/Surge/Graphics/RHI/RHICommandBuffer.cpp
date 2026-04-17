// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RHI/RHICommandBuffer.hpp"

namespace Surge::RHI
{
    void rhiAllocCommandBuffer(CommandBufferHandle& outHandle, QueueType queue)
    {
        outHandle = GetRHIDevice()->allocCommandBuffer(queue);
    }

    void rhiFreeCommandBuffer(CommandBufferHandle handle)
    {
        GetRHIDevice()->freeCommandBuffer(handle);
    }

    void rhiBeginCommandBuffer(CommandBufferHandle handle)
    {
        GetRHIDevice()->beginCommandBuffer(handle);
    }

    void rhiEndCommandBuffer(CommandBufferHandle handle)
    {
        GetRHIDevice()->endCommandBuffer(handle);
    }

    void rhiSubmitCommandBuffer(CommandBufferHandle handle, QueueType queue)
    {
        GetRHIDevice()->submitCommandBuffer(handle, queue);
    }

    void rhiCmdBeginRenderPass(CommandBufferHandle cmd, FramebufferHandle fb,
                                const ClearValue* cv, uint32_t count)
    {
        GetRHIDevice()->cmdBeginRenderPass(cmd, fb, cv, count);
    }

    void rhiCmdEndRenderPass(CommandBufferHandle cmd)
    {
        GetRHIDevice()->cmdEndRenderPass(cmd);
    }

    void rhiCmdBindGraphicsPipeline(CommandBufferHandle cmd, GraphicsPipelineHandle pipeline)
    {
        GetRHIDevice()->cmdBindGraphicsPipeline(cmd, pipeline);
    }

    void rhiCmdBindComputePipeline(CommandBufferHandle cmd, ComputePipelineHandle pipeline)
    {
        GetRHIDevice()->cmdBindComputePipeline(cmd, pipeline);
    }

    void rhiCmdBindVertexBuffer(CommandBufferHandle cmd, BufferHandle buf, uint64_t offset)
    {
        GetRHIDevice()->cmdBindVertexBuffer(cmd, buf, offset);
    }

    void rhiCmdBindIndexBuffer(CommandBufferHandle cmd, BufferHandle buf, uint64_t offset)
    {
        GetRHIDevice()->cmdBindIndexBuffer(cmd, buf, offset);
    }

    void rhiCmdDrawIndexed(CommandBufferHandle cmd, uint32_t indexCount,
                            uint32_t baseIndex, uint32_t baseVertex)
    {
        GetRHIDevice()->cmdDrawIndexed(cmd, indexCount, baseIndex, baseVertex);
    }

    void rhiCmdDraw(CommandBufferHandle cmd, uint32_t vertexCount, uint32_t firstVertex)
    {
        GetRHIDevice()->cmdDraw(cmd, vertexCount, firstVertex);
    }

    void rhiCmdDispatch(CommandBufferHandle cmd, uint32_t x, uint32_t y, uint32_t z)
    {
        GetRHIDevice()->cmdDispatch(cmd, x, y, z);
    }

    void rhiCmdSetViewport(CommandBufferHandle cmd, const Viewport& vp)
    {
        GetRHIDevice()->cmdSetViewport(cmd, vp);
    }

    void rhiCmdSetScissor(CommandBufferHandle cmd, const Rect2D& scissor)
    {
        GetRHIDevice()->cmdSetScissor(cmd, scissor);
    }

    void rhiCmdPushConstants(CommandBufferHandle cmd, ShaderStage stages,
                              const void* data, uint32_t size, uint32_t offset)
    {
        GetRHIDevice()->cmdPushConstants(cmd, stages, data, size, offset);
    }

    void rhiCmdCopyBuffer(CommandBufferHandle cmd, BufferHandle src, BufferHandle dst,
                           size_t size, size_t srcOffset, size_t dstOffset)
    {
        GetRHIDevice()->cmdCopyBuffer(cmd, src, dst, size, srcOffset, dstOffset);
    }

} // namespace Surge::RHI
