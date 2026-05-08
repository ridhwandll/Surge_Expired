// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RHI/Vulkan/VulkanRenderpassFactory.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHI.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanUtils.hpp"
#include "Surge/Core/Core.hpp"

namespace Surge
{
	VkRenderPass VulkanRenderpassFactory::GetOrCreate(const VulkanRHI& rhi, const RenderPassKey& key)
	{
		auto it = mCache.find(key);
		if (it != mCache.end())
			return it->second;

		VkRenderPass rp = Create(rhi, key);
		mCache[key] = rp;
		return rp;
	}


	void VulkanRenderpassFactory::Shutdown(const VulkanRHI& rhi)
	{
		for (auto& [key, rp] : mCache)
		{
			if (rp != VK_NULL_HANDLE)
				vkDestroyRenderPass(rhi.GetDevice(), rp, nullptr);
		}
		mCache.clear();
	}

	VkRenderPass VulkanRenderpassFactory::Create(const VulkanRHI& rhi, const RenderPassKey& key)
	{
		Vector<VkAttachmentDescription> attachments;
		Vector<VkAttachmentReference> colorRefs;
		bool hasDepth = key.DepthFormat != VK_FORMAT_UNDEFINED;

		// Color attachments
		for (Uint i = 0; i < key.ColorCount; i++)
		{
			VkAttachmentDescription desc = {};
			desc.format = key.ColorFormats[i];
			desc.samples = VK_SAMPLE_COUNT_1_BIT;
			desc.loadOp = VulkanUtils::ToVkLoadOp(key.ColorLoad[i]);
			desc.storeOp = VulkanUtils::ToVkStoreOp(key.ColorStore[i]);
			desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			desc.initialLayout = key.ColorLoad[i] == LoadOp::LOAD ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
			desc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			attachments.push_back(desc);

			VkAttachmentReference ref = {};
			ref.attachment = i;
			ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			colorRefs.push_back(ref);
		}

		// Depth attachment
		VkAttachmentReference depthRef = {};
		if (hasDepth)
		{
			VkAttachmentDescription desc = {};
			desc.format = key.DepthFormat;
			desc.samples = VK_SAMPLE_COUNT_1_BIT;
			desc.loadOp = VulkanUtils::ToVkLoadOp(key.DepthLoad);
			desc.storeOp = VulkanUtils::ToVkStoreOp(key.DepthStore);
			desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			desc.initialLayout = key.DepthLoad == LoadOp::LOAD ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
			desc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			attachments.push_back(desc);

			depthRef.attachment = key.ColorCount;
			depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		// What is a Subpass?
		// A Subpass is a logical phase of rendering within a single Render Pass. Instead of finishing one render pass and starting another
		// (which forces the GPU to write data back to main system memory), subpasses let you perform multiple operations on the same set of
		// pixels while keeping that data in fast, on-chip tile memory (Mobile? Maybe we should utilize this in future)
		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = (Uint)colorRefs.size();
		subpass.pColorAttachments = colorRefs.data();
		subpass.pDepthStencilAttachment = hasDepth ? &depthRef : nullptr;

		// What is a Subpass Dependency?
		// Vulkan does not assume that one subpass must wait for the previous one to finish. A Subpass Dependency is a synchronization command
		// (like a memory barrier) built into the Render Pass that tells the GPU exactly when it is safe for a destination subpass to read data written by a source subpass 
		VkSubpassDependency dependency = {};

		// Which subpasses are being synchronized (e.g. 0 to 1)
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;

		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

		if (hasDepth)
		{
			dependency.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			dependency.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		}

		VkRenderPassCreateInfo rpInfo = {};
		rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		rpInfo.attachmentCount = (Uint)attachments.size();
		rpInfo.pAttachments = attachments.data();
		rpInfo.subpassCount = 1;
		rpInfo.pSubpasses = &subpass;
		rpInfo.dependencyCount = 1;
		rpInfo.pDependencies = &dependency;

		VkRenderPass rp = VK_NULL_HANDLE;
		VK_CALL(vkCreateRenderPass(rhi.GetDevice(), &rpInfo, nullptr, &rp));

		return rp;
	}

}
