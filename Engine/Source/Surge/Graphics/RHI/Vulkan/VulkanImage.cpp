// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RHI/Vulkan/VulkanImage.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHI.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanUtils.hpp"

namespace Surge
{
    // (Rid) For if depth image is sampled in shader then it can have only VK_IMAGE_ASPECT_DEPTH_BIT OR VK_IMAGE_ASPECT_STENCIL_BIT, not both
    // Thats why I had to create GetAspectFlagsForImageView a separate function which returns only VK_IMAGE_ASPECT_DEPTH_BIT on DepthStencil/DepthOnly format
    // Thats why this fucntion is created for image view separately
    static VkImageAspectFlags GetAspectFlagsForImageView(ImageFormat format)
    {
        if(format == ImageFormat::D24_UNORM_S8_UINT || format == ImageFormat::D32_SFLOAT || format == ImageFormat::D16_UNORM)
            return VK_IMAGE_ASPECT_DEPTH_BIT; // No stencil(hack, we might require stencil buffer reads in future?)

        if(VulkanUtils::IsDepthFormat(format))
            return VK_IMAGE_ASPECT_DEPTH_BIT;

        return VK_IMAGE_ASPECT_COLOR_BIT;
    }

    ImageEntry VulkanImage::Create(const VulkanRHI& rhi, const ImageDesc& desc)
    {
        SG_ASSERT(desc.Width > 0, "TextureDesc::Width must be > 0");
        SG_ASSERT(desc.Height > 0, "TextureDesc::Height must be > 0");
        SG_ASSERT(desc.MipLevel > 0, "TextureDesc::Mips must be > 0");
        SG_ASSERT(desc.Layers > 0, "TextureDesc::Layers must be > 0");

        // Transient attachments Tf are these?
        // Transient attachments are images used for intermediate data that only exists during a single render pass.
        // These textures are never meant to be stored in the device's main memory or read back by the CPU; they are created, used, and discarded entirely
        // within the GPU's high-speed local cache (on-chip memory)
        // Transient textures must not be sampled or transferred
        if (desc.Transient)
        {
            SG_ASSERT(!(desc.Usage & ImageUsage::SAMPLED), "TextureDesc: Transient textures cannot have SAMPLED usage they live on tile memory and are never written to DRAM");
            SG_ASSERT(!(desc.Usage & ImageUsage::TRANSFER_SRC), "TextureDesc: Transient textures cannot have TRANSFER_SRC usage");
        }

        ImageEntry entry = {};
        entry.Layout = VK_IMAGE_LAYOUT_UNDEFINED;
        entry.Desc = desc;

        // VkImage
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VulkanUtils::ImageFormatToVkFormat(desc.Format);
        imageInfo.extent = { desc.Width, desc.Height, 1 };
        imageInfo.mipLevels = desc.MipLevel;
        imageInfo.arrayLayers = desc.Layers;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VulkanUtils::ToVkImageUsage(desc.Usage, desc.Transient);

        if(desc.MipLevel > 1)
            imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

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

        VmaAllocator allocator = rhi.GetAllocator();
        VmaAllocationInfo allocInfo;
        VK_CALL(vmaCreateImage(allocator, &imageInfo, &allocCreateInfo, &entry.Image, &entry.Allocation, &allocInfo));
        entry.Size = allocInfo.size;

        // VkImageView
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = entry.Image;
        viewInfo.viewType = desc.Layers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VulkanUtils::ImageFormatToVkFormat(desc.Format);
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        // (Rid) For if depth image is sampled in shader then it can have only VK_IMAGE_ASPECT_DEPTH_BIT OR VK_IMAGE_ASPECT_STENCIL_BIT, not both
        // Thats why I had to create GetAspectFlagsForImageView a separate function which returns only VK_IMAGE_ASPECT_DEPTH_BIT on DepthStencil/DepthOnly format
        viewInfo.subresourceRange.aspectMask = GetAspectFlagsForImageView(desc.Format);
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = desc.MipLevel;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = desc.Layers;
        VK_CALL(vkCreateImageView(rhi.GetDevice(), &viewInfo, nullptr, &entry.View));

        SET_VK_DEBUG_NAME(rhi, VK_OBJECT_TYPE_IMAGE, (uint64_t)entry.Image, desc.DebugName);
        SET_VK_DEBUG_NAME(rhi, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)entry.View, desc.DebugName);
        SET_VMA_DEBUG_NAME(rhi, entry.Allocation, desc.DebugName.c_str());

        return entry;
    }

    void VulkanImage::Destroy(const VulkanRHI& rhi, ImageEntry& entry)
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

    void VulkanImage::UploadData(VulkanRHI& rhi, ImageHandle h, const void* data, Uint size)
    {
        ImageEntry* entry = rhi.mTexturePool.Get(h);
        SG_ASSERT(entry, "UploadTextureData: invalid TextureHandle");
        SG_ASSERT(data && size > 0, "UploadTextureData: data is null or size is 0");
        VmaAllocator allocator = rhi.GetAllocator();

        VkBufferCreateInfo stagingInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        stagingInfo.size = size;

        stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        VmaAllocationCreateInfo stagingAllocInfo = {};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        stagingAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT; // TODO(Rid) Do we need this requiredFlags?

        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        VmaAllocation stagingAllocation {};
        VmaAllocationInfo stagingResultInfo;

        VK_CALL(vmaCreateBuffer(allocator, &stagingInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, &stagingResultInfo));
        SG_ASSERT(stagingResultInfo.pMappedData != nullptr, "Staging buffer failed to map");
        memcpy(stagingResultInfo.pMappedData, data, size);

        const VkCommandBuffer cb = rhi.BeginOneTimeCommands();

        // Transition all mip levels to TRANSFER_DST
        TransitionLayout(cb, *entry, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { entry->Desc.Width, entry->Desc.Height, 1 };
        vkCmdCopyBufferToImage(cb, stagingBuffer, entry->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        if(entry->Desc.MipLevel > 1)
            GenerateMipmaps(rhi, cb, *entry); // leaves all levels at SHADER_READ_ONLY_OPTIMAL
        else
            TransitionLayout(cb, *entry, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        rhi.EndOneTimeCommands(cb);
        vmaDestroyBuffer(rhi.GetAllocator(), stagingBuffer, stagingAllocation);
        vkQueueWaitIdle(rhi.GetQueue());
    }

    void VulkanImage::TransitionLayout(VkCommandBuffer cmd, ImageEntry& entry, VkImageLayout newLayout)
    {
        if(entry.Layout == newLayout)
        {
            //Log<Severity::Warn>("Trying to call VulkanImage::TransitionLayout while entry.Layout = newLayout!");
            return;
        }

        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = entry.Layout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = entry.Image;

        if(entry.Layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        else if(entry.Layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        else
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = entry.Desc.MipLevel;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = entry.Desc.Layers;

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
            srcStage              = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            srcStage              = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStage              = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            srcStage              = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            srcStage              = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;

        case VK_IMAGE_LAYOUT_GENERAL:
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            srcStage              = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            break;

        default:
            SG_ASSERT_INTERNAL("VulkanTexture::TransitionLayout: unhandled source layout!");
            break;
        }

        // Destination access + stage
        switch (newLayout)
        {
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
            dstStage              = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            dstStage              = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT   | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
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
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT  | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            break;

        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            barrier.dstAccessMask = 0;
            dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            break;

        case VK_IMAGE_LAYOUT_GENERAL:
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            dstStage              = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            break;

        default:
            SG_ASSERT(false, "VulkanTexture::TransitionLayout: unhandled destination layout!");
            break;
        }

        vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        entry.Layout = newLayout;
    }

    void VulkanImage::GenerateMipmaps(VulkanRHI& rhi, VkCommandBuffer cmd, ImageEntry& entry)
    {
        const Uint mipCount = entry.Desc.MipLevel;
        SG_ASSERT(mipCount > 1, "GenerateMipmaps called on image with Mips <= 1");

        // Validate that the format supports linear filtering required for vkCmdBlitImage
        VkFormatProperties formatProps;
        vkGetPhysicalDeviceFormatProperties(rhi.GetGPU(), VulkanUtils::ImageFormatToVkFormat(entry.Desc.Format), &formatProps);
        SG_ASSERT(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT, "GenerateMipmaps: image format does not support linear filtering, cannot blit mips!");

        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image = entry.Image;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.subresourceRange.levelCount = 1; // one mip at a time

        int32_t mipWidth = static_cast<int32_t>(entry.Desc.Width);
        int32_t mipHeight = static_cast<int32_t>(entry.Desc.Height);

        for(Uint i = 1; i < mipCount; i++)
        {
            // Promote mip i-1: TRANSFER_DST to TRANSFER_SRC so we can read from it
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            vkCmdPipelineBarrier(cmd,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0, 0, nullptr, 0, nullptr, 1, &barrier);

            // Blit mip i-1 to mip i (mip i is already TRANSFER_DST from the upfront transition)
            const int32_t nextWidth = mipWidth > 1 ? mipWidth / 2 : 1;
            const int32_t nextHeight = mipHeight > 1 ? mipHeight / 2 : 1;

            VkImageBlit blit = {};
            blit.srcOffsets[0] = { 0, 0, 0 };
            blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;

            blit.dstOffsets[0] = { 0, 0, 0 };
            blit.dstOffsets[1] = { nextWidth, nextHeight, 1 };
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;

            vkCmdBlitImage(cmd,
                           entry.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           entry.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &blit, VK_FILTER_LINEAR);

            // Mip i-1 is done, send it to its final layout
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmd,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 0, 0, nullptr, 0, nullptr, 1, &barrier);

            mipWidth = nextWidth;
            mipHeight = nextHeight;
        }

        // The last mip level was never a blit source, it's still TRANSFER_DST
        barrier.subresourceRange.baseMipLevel = mipCount - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        // All levels are now SHADER_READ_ONLY_OPTIMAL
        entry.Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
}
