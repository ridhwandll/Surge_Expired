// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RHI/Vulkan/VulkanTexture.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHI.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanUtils.hpp"

namespace Surge
{
	VkImageAspectFlags VulkanTexture::GetAspectFlags(TextureFormat format)
	{
		if (format == TextureFormat::D24_UNORM_S8_UINT)
			return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

		if (VulkanUtils::IsDepthFormat(format))
			return VK_IMAGE_ASPECT_DEPTH_BIT;

		return VK_IMAGE_ASPECT_COLOR_BIT;
	}

	TextureEntry VulkanTexture::Create(const VulkanRHI& rhi, const TextureDesc& desc)
	{
		SG_ASSERT(desc.Width > 0, "TextureDesc::Width must be > 0");
		SG_ASSERT(desc.Height > 0, "TextureDesc::Height must be > 0");
		SG_ASSERT(desc.Mips > 0, "TextureDesc::Mips must be > 0");
		SG_ASSERT(desc.Layers > 0, "TextureDesc::Layers must be > 0");

		// Transient attachments Tf are these?
		// Transient attachments are images used for intermediate data that only exists during a single render pass.
		// These textures are never meant to be stored in the device's main memory or read back by the CPU; they are created, used, and discarded entirely
		// within the GPU's high-speed local cache (on-chip memory)
		// Transient textures must not be sampled or transferred
		if (desc.Transient)
		{
			SG_ASSERT(!(desc.Usage & TextureUsage::SAMPLED), "TextureDesc: Transient textures cannot have SAMPLED usage they live on tile memory and are never written to DRAM");
			SG_ASSERT(!(desc.Usage & TextureUsage::TRANSFER_SRC), "TextureDesc: Transient textures cannot have TRANSFER_SRC usage");
		}

		TextureEntry entry = {};
		entry.Layout = VK_IMAGE_LAYOUT_UNDEFINED;
		entry.Desc = desc;

		// VkImage
		VkImageCreateInfo imageInfo = {};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.format = VulkanUtils::TextureFormatToVkFormat(desc.Format);
		imageInfo.extent = { desc.Width, desc.Height, 1 };
		imageInfo.mipLevels = desc.Mips;
		imageInfo.arrayLayers = desc.Layers;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = VulkanUtils::ToVkImageUsage(desc.Usage, desc.Transient);

		// VK_SHARING_MODE_EXCLUSIVE specifies that access to any range or image subresource of the object will be exclusive to a single queue family at a time	
		// VK_SHARING_MODE_CONCURRENT may result in lower performance access to the buffer or image than VK_SHARING_MODE_EXCLUSIVE (Vulkan Docs)
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; 		
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		// VMA allocation
		VmaAllocationCreateInfo allocCreateInfo = {};

		if (desc.Transient)
		{
			// Mobile TBDR key optimization:
			// GPU_LAZILY_ALLOCATED = physical memory only allocated if the GPU actually needs to flush the tile on a perfect TBDR frame, zero bytes
			// of real DRAM are used for transient attachments
			allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED;
			allocCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
		}
		else		
			allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE; // Load on GPU
		// VMA Docs:
		// If you have a preference for putting the resource in GPU (device) memory or CPU (host) memory on systems with discrete graphics card that have
		// the memories separate, you can use VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE or VMA_MEMORY_USAGE_AUTO_PREFER_HOST.

		VmaAllocationInfo allocInfo;
		VK_CALL(vmaCreateImage(rhi.GetAllocator(), &imageInfo, &allocCreateInfo, &entry.Image, &entry.Allocation, &allocInfo));
		entry.Size = allocInfo.size;

		// VkImageView
		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = entry.Image;
		viewInfo.viewType = desc.Layers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = VulkanUtils::TextureFormatToVkFormat(desc.Format);
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.subresourceRange.aspectMask = GetAspectFlags(desc.Format);
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = desc.Mips;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = desc.Layers;
		VK_CALL(vkCreateImageView(rhi.GetDevice(), &viewInfo, nullptr, &entry.View));

		// Debug name
#if defined(SURGE_DEBUG)
		if (!desc.DebugName.empty())
		{
			VkDebugUtilsObjectNameInfoEXT nameInfo = {};
			nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
			nameInfo.objectType = VK_OBJECT_TYPE_IMAGE;
			nameInfo.objectHandle = (uint64_t)entry.Image;
			nameInfo.pObjectName = desc.DebugName.c_str();
			rhi.SetDebugName(nameInfo);

			// Name the view too
			nameInfo.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
			nameInfo.objectHandle = (uint64_t)entry.View;
			rhi.SetDebugName(nameInfo);
		}
#endif

		return entry;
	}

	void VulkanTexture::Destroy(const VulkanRHI& rhi, TextureEntry& entry)
	{
		if (entry.View != VK_NULL_HANDLE)
		{
			vkDestroyImageView(rhi.GetDevice(), entry.View, nullptr);
			entry.View = VK_NULL_HANDLE;
		}

		if (entry.Image != VK_NULL_HANDLE)
		{
			// vmaDestroyImage frees both VkImage and VmaAllocation together
			vmaDestroyImage(rhi.GetAllocator(), entry.Image, entry.Allocation);
			entry.Image = VK_NULL_HANDLE;
			entry.Allocation = VK_NULL_HANDLE;
		}

		entry.Layout = VK_IMAGE_LAYOUT_UNDEFINED;
		//entry.Desc = {}; //Don't clear the desc since it may be needed for resizing or other purposes after destruction
	}

	void VulkanTexture::UploadData(VulkanRHI& rhi, TextureHandle h, const void* data, Uint size)
	{
		TextureEntry* entry = rhi.mTexturePool.Get(h);
		SG_ASSERT(entry, "UploadTextureData: invalid TextureHandle");
		SG_ASSERT(data && size > 0, "UploadTextureData: data is null or size is 0");

		// Staging buffer
		BufferDesc stagingDesc = {};
		stagingDesc.Size = size;
		stagingDesc.Usage = BufferUsage::STAGING;
		stagingDesc.HostVisible = true;
		stagingDesc.InitialData = data;
		stagingDesc.DebugName = "TextureUploadStaging";
		BufferHandle stagingHandle = rhi.CreateBuffer(stagingDesc);
		BufferEntry* staging = rhi.mBufferPool.Get(stagingHandle);
		Log<Severity::Debug>("Created staging buffer generation: {}", stagingHandle.Generation);

		// One-time command buffer
		VkCommandBuffer cmd = rhi.BeginOneTimeCommands();

		// Transition: UNDEFINED to TRANSFER_DST
		TransitionLayout(cmd, *entry, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		// Copy staging buffer to image, one region per mip 0
		VkBufferImageCopy region = {};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;// tightly packed
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VulkanTexture::GetAspectFlags(entry->Desc.Format);
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { entry->Desc.Width, entry->Desc.Height, 1 };

		vkCmdCopyBufferToImage(cmd, staging->Buffer, entry->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		// Transition: TRANSFER_DST to SHADER_READ_ONLY
		TransitionLayout(cmd, *entry, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		rhi.EndOneTimeCommands(cmd);

		// Cleanup staging buffer
		vkQueueWaitIdle(rhi.GetQueue());
		rhi.DestroyBuffer(stagingHandle);
	}

	void VulkanTexture::TransitionLayout(VkCommandBuffer cmd, TextureEntry& entry, VkImageLayout newLayout)
	{
		if (entry.Layout == newLayout)
			return;

		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = entry.Layout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = entry.Image;
		barrier.subresourceRange.aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
			newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
			? VK_IMAGE_ASPECT_DEPTH_BIT
			: VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = entry.Desc.Mips;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

		// Source access + stage
		switch (entry.Layout)
		{
		case VK_IMAGE_LAYOUT_UNDEFINED:
			barrier.srcAccessMask = 0;
			srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			break;

		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			break;

		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			srcStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
				| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			break;

		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			break;

		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;

		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;

		case VK_IMAGE_LAYOUT_GENERAL:
			barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			srcStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			break;

		default:
			SG_ASSERT_INTERNAL("VulkanTexture::TransitionLayout: unhandled source layout!");
			break;
		}

		// Destination access + stage
		switch (newLayout)
		{
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
				| VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
			dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			break;

		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
				| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
			dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
				| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			break;

		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
				| VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
			break;

		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;

		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;

		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT
				| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
			dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
				| VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			break;

		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			barrier.dstAccessMask = 0;
			dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			break;

		case VK_IMAGE_LAYOUT_GENERAL:
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT
				| VK_ACCESS_SHADER_WRITE_BIT;
			dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			break;

		default:
			SG_ASSERT(false, "VulkanTexture::TransitionLayout: unhandled destination layout!");
			break;
		}

		vkCmdPipelineBarrier(cmd,
			srcStage, dstStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		entry.Layout = newLayout;
	}
}
