// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RHI/Vulkan/VulkanFramebuffer.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHI.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanUtils.hpp"

namespace Surge
{	
    FramebufferEntry VulkanFramebuffer::Create(const VulkanRHI& rhi, const FramebufferDesc& desc, VulkanRenderpassFactory& rpFactory, HandlePool<ImageHandle, ImageEntry>& imagePool)
    {
        SG_ASSERT(desc.Width > 0, "FramebufferDesc: Width is 0");
        SG_ASSERT(desc.Height > 0, "FramebufferDesc: Height is 0");
        SG_ASSERT(desc.ColorAttachmentCount > 0 || desc.DepthAttachment.Handle.IsNull() == false, "FramebufferDesc: must have at least one attachment");

        FramebufferEntry entry = {};
        entry.Desc = desc;

        // Build RenderPassKey to look up/create VkRenderPass
        RenderPassKey key = {};
        std::array<VkImageView, 9> views = {};

        Uint viewCount = 0;
        for (Uint i = 0; i < desc.ColorAttachmentCount; i++)
        {
            ImageEntry* tex = imagePool.Get(desc.ColorAttachments[i].Handle);
            SG_ASSERT(tex, "FramebufferDesc: invalid ColorAttachment handle!");

            key.ColorFormats[i] = VulkanUtils::ImageFormatToVkFormat(tex->Desc.Format);
            key.ColorLoad[i] = desc.ColorAttachments[i].Load;
            key.ColorStore[i] = desc.ColorAttachments[i].Store;
            views[viewCount++] = tex->View;

            // Does this do auto transition?
            //tex->Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; 
            // Default clear color per attachment (Foof pink)
            entry.ClearValues[i].color = { {1.0f, 0.0f, 1.0f, 1.0f} };
            entry.ClearCount++;
        }
        key.ColorCount = desc.ColorAttachmentCount;

        if (!desc.DepthAttachment.Handle.IsNull())
        {
            const ImageEntry* depth = imagePool.Get(desc.DepthAttachment.Handle);
            SG_ASSERT(depth, "FramebufferDesc: invalid DepthAttachment handle");

            key.DepthFormat = VulkanUtils::ImageFormatToVkFormat(depth->Desc.Format);
            key.DepthLoad = desc.DepthAttachment.Load;
            key.DepthStore = desc.DepthAttachment.Store;
            key.StencilLoad = desc.DepthAttachment.StencilLoad;
            key.StencilStore = desc.DepthAttachment.StencilStore;
            views[viewCount++] = depth->View;

            entry.ClearValues[entry.ClearCount].depthStencil = { 1.0f, 0 };
            entry.ClearCount++;
        }

        // Get or create compatible VkRenderPass from cache
        entry.RenderPass = rpFactory.GetOrCreate(rhi, key);
        SG_ASSERT(entry.RenderPass != VK_NULL_HANDLE, "VulkanFramebuffer: failed to get render pass from cache");

        // Create VkFramebuffer
        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = entry.RenderPass;
        fbInfo.attachmentCount = viewCount;
        fbInfo.pAttachments = views.data();
        fbInfo.width = desc.Width;
        fbInfo.height = desc.Height;
        fbInfo.layers = 1;
        VK_CALL(vkCreateFramebuffer(rhi.GetDevice(), &fbInfo, nullptr, &entry.Framebuffer));
        SET_VK_DEBUG_NAME(rhi, VK_OBJECT_TYPE_FRAMEBUFFER, (uint64_t)entry.Framebuffer, desc.DebugName);

        return entry;
    }

    void VulkanFramebuffer::Destroy(const VulkanRHI& rhi, FramebufferEntry& entry)
    {
        // Only destroy the VkFramebuffer RenderPass is owned by cache
        if (entry.Framebuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(rhi.GetDevice(), entry.Framebuffer, nullptr);
            entry.Framebuffer = VK_NULL_HANDLE;
        }

        // entry.RenderPass intentionally NOT destroyed here
        entry.RenderPass = VK_NULL_HANDLE;
        // entry.Desc = {}; //Don't clear desc since caller may want to reuse it for resizing
    }
}
