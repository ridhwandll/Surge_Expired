// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RHI/Vulkan/VulkanFramebuffer.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHI.hpp"

namespace Surge
{	
	FramebufferEntry VulkanFramebuffer::Create(const VulkanRHI& rhi, const FramebufferDesc& desc, VulkanRenderpassCache& cache, const HandlePool<TextureHandle, TextureEntry>& texPool)
	{
		SG_ASSERT(desc.Width > 0, "FramebufferDesc: Width is 0");
		SG_ASSERT(desc.Height > 0, "FramebufferDesc: Height is 0");
		SG_ASSERT(desc.ColorAttachmentCount > 0 || desc.DepthAttachment.Handle.IsNull() == false, "FramebufferDesc: must have at least one attachment");

		FramebufferEntry entry = {};
		entry.Width = desc.Width;
		entry.Height = desc.Height;
		entry.Desc = desc;

		// Build RenderPassKey to look up / create VkRenderPass
		RenderPassKey key = {};
		VkImageView views[9] = {};
		Uint viewCount = 0;

		for (Uint i = 0; i < desc.ColorAttachmentCount; i++)
		{
			const TextureEntry* tex = texPool.Get(desc.ColorAttachments[i].Handle);
			SG_ASSERT(tex, "FramebufferDesc: invalid ColorAttachment handle!");

			key.ColorFormats[i] = tex->Format;
			key.ColorLoad[i] = desc.ColorAttachments[i].Load;
			key.ColorStore[i] = desc.ColorAttachments[i].Store;
			views[viewCount++] = tex->View;

			// Default clear color per attachment
			entry.ClearValues[i].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
			entry.ClearCount++;
		}
		key.ColorCount = desc.ColorAttachmentCount;

		if (!desc.DepthAttachment.Handle.IsNull())
		{
			const TextureEntry* depth = texPool.Get(desc.DepthAttachment.Handle);
			SG_ASSERT(depth, "FramebufferDesc: invalid DepthAttachment handle");

			key.DepthFormat = depth->Format;
			key.DepthLoad = desc.DepthAttachment.Load;
			key.DepthStore = desc.DepthAttachment.Store;
			views[viewCount++] = depth->View;

			entry.ClearValues[entry.ClearCount].depthStencil = { 1.0f, 0 };
			entry.ClearCount++;
		}

		// Get or create compatible VkRenderPass from cache
		entry.RenderPass = cache.GetOrCreate(rhi, key);
		SG_ASSERT(entry.RenderPass != VK_NULL_HANDLE, "VulkanFramebuffer: failed to get render pass from cache");

		// Create VkFramebuffer
		VkFramebufferCreateInfo fbInfo = {};
		fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbInfo.renderPass = entry.RenderPass;
		fbInfo.attachmentCount = viewCount;
		fbInfo.pAttachments = views;
		fbInfo.width = desc.Width;
		fbInfo.height = desc.Height;
		fbInfo.layers = 1;
		VK_CALL(vkCreateFramebuffer(rhi.GetDevice(), &fbInfo, nullptr, &entry.Framebuffer));

#if defined(SURGE_DEBUG)
		if (desc.DebugName)
		{
			VkDebugUtilsObjectNameInfoEXT nameInfo = {};
			nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
			nameInfo.objectType = VK_OBJECT_TYPE_FRAMEBUFFER;
			nameInfo.objectHandle = (uint64_t)entry.Framebuffer;
			nameInfo.pObjectName = desc.DebugName;
			rhi.SetDebugName(nameInfo);
		}
#endif

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
	}
}
