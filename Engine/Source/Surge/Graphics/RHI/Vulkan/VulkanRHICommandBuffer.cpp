// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RHI/Vulkan/VulkanRHICommandBuffer.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHIDevice.hpp"
#include <volk.h>

namespace Surge::RHI
{
    static VkQueue GetVkQueue(QueueType queue)
    {
        switch (queue)
        {
            case QueueType::Compute:  return gVulkanRHIDevice.GetComputeQueue();
            case QueueType::Transfer: return gVulkanRHIDevice.GetTransferQueue();
            default:                  return gVulkanRHIDevice.GetGraphicsQueue();
        }
    }

    static uint32_t GetQueueFamily(QueueType queue)
    {
        switch (queue)
        {
            case QueueType::Compute:  return gVulkanRHIDevice.GetComputeFamily();
            case QueueType::Transfer: return gVulkanRHIDevice.GetTransferFamily();
            default:                  return gVulkanRHIDevice.GetGraphicsFamily();
        }
    }

    CommandBufferHandle vkRHIAllocCommandBuffer(QueueType queue)
    {
        VkDevice device = gVulkanRHIDevice.GetLogicalDevice();

        VkCommandPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        poolInfo.queueFamilyIndex = GetQueueFamily(queue);
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        VkCommandPool pool = VK_NULL_HANDLE;
        vkCreateCommandPool(device, &poolInfo, nullptr, &pool);

        VkCommandBufferAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocInfo.commandPool        = pool;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer cmdBuf = VK_NULL_HANDLE;
        vkAllocateCommandBuffers(device, &allocInfo, &cmdBuf);

        VkFenceCreateInfo fenceInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VkFence fence = VK_NULL_HANDLE;
        vkCreateFence(device, &fenceInfo, nullptr, &fence);

        VulkanCommandBuffer vcb;
        vcb.CmdBuf    = cmdBuf;
        vcb.Pool      = pool;
        vcb.Fence     = fence;
        vcb.QueueType = queue;

        return GetVulkanRegistry().CommandBuffers.Create(std::move(vcb));
    }

    void vkRHIFreeCommandBuffer(CommandBufferHandle handle)
    {
        auto& reg  = GetVulkanRegistry();
        auto& vcb  = reg.GetCommandBuffer(handle);
        VkDevice dev = gVulkanRHIDevice.GetLogicalDevice();

        vkDestroyFence(dev, vcb.Fence, nullptr);
        vkDestroyCommandPool(dev, vcb.Pool, nullptr);
        reg.CommandBuffers.Destroy(handle);
    }

    void vkRHIBeginCommandBuffer(CommandBufferHandle handle)
    {
        auto& vcb  = GetVulkanRegistry().GetCommandBuffer(handle);
        VkDevice dev = gVulkanRHIDevice.GetLogicalDevice();
        vkWaitForFences(dev, 1, &vcb.Fence, VK_TRUE, UINT64_MAX);
        vkResetFences(dev, 1, &vcb.Fence);

        VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(vcb.CmdBuf, &beginInfo);
    }

    void vkRHIEndCommandBuffer(CommandBufferHandle handle)
    {
        auto& vcb = GetVulkanRegistry().GetCommandBuffer(handle);
        vkEndCommandBuffer(vcb.CmdBuf);
    }

    void vkRHISubmitCommandBuffer(CommandBufferHandle handle, QueueType queue)
    {
        auto& vcb = GetVulkanRegistry().GetCommandBuffer(handle);
        VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1;
        si.pCommandBuffers    = &vcb.CmdBuf;
        vkQueueSubmit(GetVkQueue(queue), 1, &si, vcb.Fence);
    }

    void vkRHICmdBeginRenderPass(CommandBufferHandle cmd, FramebufferHandle fb,
                                  const ClearValue* clearValues, uint32_t clearCount)
    {
        auto& reg  = GetVulkanRegistry();
        auto& vcb  = reg.GetCommandBuffer(cmd);
        auto& vfb  = reg.GetFramebuffer(fb);

        Vector<VkClearValue> vkClears(clearCount);
        for (uint32_t i = 0; i < clearCount; ++i)
        {
            if (clearValues[i].IsDepth)
            {
                vkClears[i].depthStencil.depth   = clearValues[i].DepthStencil.Depth;
                vkClears[i].depthStencil.stencil = clearValues[i].DepthStencil.Stencil;
            }
            else
            {
                memcpy(vkClears[i].color.float32, clearValues[i].Color.Float32, sizeof(float) * 4);
            }
        }

        VkRenderPassBeginInfo rpBI = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rpBI.renderPass        = vfb.RenderPass;
        rpBI.framebuffer       = vfb.Framebuffer;
        rpBI.renderArea.offset = {0, 0};
        rpBI.renderArea.extent = {vfb.Width, vfb.Height};
        rpBI.clearValueCount   = clearCount;
        rpBI.pClearValues      = vkClears.empty() ? nullptr : vkClears.data();

        vkCmdBeginRenderPass(vcb.CmdBuf, &rpBI, VK_SUBPASS_CONTENTS_INLINE);
    }

    void vkRHICmdEndRenderPass(CommandBufferHandle cmd)
    {
        vkCmdEndRenderPass(GetVulkanRegistry().GetCommandBuffer(cmd).CmdBuf);
    }

    void vkRHICmdBindGraphicsPipeline(CommandBufferHandle cmd, GraphicsPipelineHandle pipeline)
    {
        auto& reg = GetVulkanRegistry();
        vkCmdBindPipeline(reg.GetCommandBuffer(cmd).CmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          reg.GetGraphicsPipeline(pipeline).Pipeline);
    }

    void vkRHICmdBindComputePipeline(CommandBufferHandle cmd, ComputePipelineHandle pipeline)
    {
        auto& reg = GetVulkanRegistry();
        vkCmdBindPipeline(reg.GetCommandBuffer(cmd).CmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE,
                          reg.GetComputePipeline(pipeline).Pipeline);
    }

    void vkRHICmdBindVertexBuffer(CommandBufferHandle cmd, BufferHandle buf, uint64_t offset)
    {
        auto& reg       = GetVulkanRegistry();
        VkBuffer vkBuf  = reg.GetBuffer(buf).Buffer;
        VkDeviceSize off = static_cast<VkDeviceSize>(offset);
        vkCmdBindVertexBuffers(reg.GetCommandBuffer(cmd).CmdBuf, 0, 1, &vkBuf, &off);
    }

    void vkRHICmdBindIndexBuffer(CommandBufferHandle cmd, BufferHandle buf, uint64_t offset)
    {
        auto& reg = GetVulkanRegistry();
        vkCmdBindIndexBuffer(reg.GetCommandBuffer(cmd).CmdBuf,
                             reg.GetBuffer(buf).Buffer,
                             static_cast<VkDeviceSize>(offset),
                             VK_INDEX_TYPE_UINT32);
    }

    void vkRHICmdDrawIndexed(CommandBufferHandle cmd, uint32_t indexCount,
                              uint32_t baseIndex, uint32_t baseVertex)
    {
        vkCmdDrawIndexed(GetVulkanRegistry().GetCommandBuffer(cmd).CmdBuf,
                         indexCount, 1, baseIndex, static_cast<int32_t>(baseVertex), 0);
    }

    void vkRHICmdDraw(CommandBufferHandle cmd, uint32_t vertexCount, uint32_t firstVertex)
    {
        vkCmdDraw(GetVulkanRegistry().GetCommandBuffer(cmd).CmdBuf, vertexCount, 1, firstVertex, 0);
    }

    void vkRHICmdDispatch(CommandBufferHandle cmd, uint32_t x, uint32_t y, uint32_t z)
    {
        vkCmdDispatch(GetVulkanRegistry().GetCommandBuffer(cmd).CmdBuf, x, y, z);
    }

    void vkRHICmdSetViewport(CommandBufferHandle cmd, const Viewport& vp)
    {
        VkViewport vkVP;
        vkVP.x        = vp.x;
        vkVP.y        = vp.y;
        vkVP.width    = vp.Width;
        vkVP.height   = vp.Height;
        vkVP.minDepth = vp.MinDepth;
        vkVP.maxDepth = vp.MaxDepth;
        vkCmdSetViewport(GetVulkanRegistry().GetCommandBuffer(cmd).CmdBuf, 0, 1, &vkVP);
    }

    void vkRHICmdSetScissor(CommandBufferHandle cmd, const Rect2D& scissor)
    {
        VkRect2D vkR;
        vkR.offset = {scissor.X, scissor.Y};
        vkR.extent = {scissor.Width, scissor.Height};
        vkCmdSetScissor(GetVulkanRegistry().GetCommandBuffer(cmd).CmdBuf, 0, 1, &vkR);
    }

    void vkRHICmdPushConstants(CommandBufferHandle cmd, ShaderStage stages,
                                const void* data, uint32_t size, uint32_t offset)
    {
        VkShaderStageFlags vkStages = 0;
        if (stages & ShaderStage::Vertex)  vkStages |= VK_SHADER_STAGE_VERTEX_BIT;
        if (stages & ShaderStage::Pixel)   vkStages |= VK_SHADER_STAGE_FRAGMENT_BIT;
        if (stages & ShaderStage::Compute) vkStages |= VK_SHADER_STAGE_COMPUTE_BIT;

        // Pipeline layout must be known at this point; for simplicity we use
        // the graphics pipeline layout that was last bound.
        // TODO: track the bound pipeline layout per command buffer.
        auto& vcb = GetVulkanRegistry().GetCommandBuffer(cmd);
        (void)vcb; (void)vkStages; (void)data; (void)size; (void)offset;
        // vkCmdPushConstants(vcb.CmdBuf, boundLayout, vkStages, offset, size, data);
    }

    void vkRHICmdCopyBuffer(CommandBufferHandle cmd, BufferHandle src, BufferHandle dst,
                             size_t size, size_t srcOffset, size_t dstOffset)
    {
        auto& reg = GetVulkanRegistry();
        VkBufferCopy region;
        region.srcOffset = static_cast<VkDeviceSize>(srcOffset);
        region.dstOffset = static_cast<VkDeviceSize>(dstOffset);
        region.size      = static_cast<VkDeviceSize>(size);
        vkCmdCopyBuffer(reg.GetCommandBuffer(cmd).CmdBuf,
                        reg.GetBuffer(src).Buffer,
                        reg.GetBuffer(dst).Buffer,
                        1, &region);
    }

} // namespace Surge::RHI
