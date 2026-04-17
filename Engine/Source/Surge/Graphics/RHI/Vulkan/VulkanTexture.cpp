// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RHI/Vulkan/VulkanTexture.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanImage.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHIDevice.hpp"
#include <stb_image.h>
#include <vk_mem_alloc.h>

namespace Surge
{
    VulkanTexture2D::VulkanTexture2D(const String& filepath, TextureSpecification specification) : mFilePath(filepath), mSpecification(specification)
    {
        int width, height, channels;
        ImageFormat imageFormat;
        bool defaultFormat = (specification.Format == ImageFormat::None);

        if (stbi_is_hdr(filepath.c_str()))
        {
            mPixelData = (void*)stbi_loadf(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
            if (defaultFormat)
                imageFormat = ImageFormat::RGBA32F;
            else
                imageFormat = specification.Format;
        }
        else
        {
            mPixelData = (void*)stbi_load(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
            if (defaultFormat)
                imageFormat = ImageFormat::RGBA8;
            else
                imageFormat = specification.Format;
        }
        SG_ASSERT(mPixelData, "Failed to load image!");
        mWidth = width;
        mHeight = height;
        mPixelDataSize = VulkanUtils::GetMemorySize(imageFormat, width, height);
        Uint mipChainLevels = CalculateMipChainLevels(width, height);

        ImageSpecification imageSpec {};
        imageSpec.Format = imageFormat;
        imageSpec.Width = mWidth;
        imageSpec.Height = mHeight;
        imageSpec.Mips = specification.UseMips ? mipChainLevels : 1;
        imageSpec.Usage = specification.Usage;
        imageSpec.SamplerProps = specification.Sampler;
        mImage = Image2D::Create(imageSpec);
        mSpecification.Format = imageFormat;

        Invalidate();
        stbi_image_free(mPixelData);
    }

    VulkanTexture2D::VulkanTexture2D(ImageFormat format, Uint width, Uint height, void* data, TextureSpecification specification)
    {
        mWidth = width;
        mHeight = height;
        mPixelData = data;
        mPixelDataSize = VulkanUtils::GetMemorySize(format, width, height);

        ImageSpecification imageSpec {};
        imageSpec.Format = format;
        imageSpec.Width = mWidth;
        imageSpec.Height = mHeight;
        imageSpec.Mips = 1;
        imageSpec.Usage = specification.Usage;
        imageSpec.SamplerProps = specification.Sampler;
        mImage = Image2D::Create(imageSpec);
        mSpecification.Format = format;

        Invalidate();
    }

    VulkanTexture2D::~VulkanTexture2D()
    {
        if (mImage)
            mImage->Release();
    }

    void VulkanTexture2D::Invalidate()
    {
        mImage->Release();
        VmaAllocator allocator = RHI::gVulkanRHIDevice.GetVMAAllocator();

        ImageSpecification& imageSpec = mImage->GetSpecification();
        imageSpec.Format = mSpecification.Format;
        Ref<VulkanImage2D> image = mImage.As<VulkanImage2D>();
        image->Invalidate();

        VkDeviceSize size = mPixelDataSize;

        // Create staging buffer
        VkBufferCreateInfo bufferCreateInfo {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferCreateInfo.size = size;
        bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo stagingAllocInfo = {};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        VmaAllocation stagingBufferAllocation = nullptr;
        VmaAllocationInfo stagingInfo = {};
        vmaCreateBuffer(allocator, &bufferCreateInfo, &stagingAllocInfo, &stagingBuffer, &stagingBufferAllocation, &stagingInfo);

        // Copy data to staging buffer
        SG_ASSERT(mPixelData, "Invalid pixel data!");
        memcpy(stagingInfo.pMappedData, mPixelData, mPixelDataSize);

        RHI::gVulkanRHIDevice.InstantSubmit(RHI::QueueType::Graphics, [&](VkCommandBuffer cmd) {
            VkImageSubresourceRange subresourceRange {};
            subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            subresourceRange.baseMipLevel = 0;
            subresourceRange.levelCount = imageSpec.Mips;
            subresourceRange.layerCount = 1;

            VkImageMemoryBarrier imageMemoryBarrier {};
            imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarrier.image = image->GetVulkanImage();
            imageMemoryBarrier.subresourceRange = subresourceRange;
            imageMemoryBarrier.srcAccessMask = 0;
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

            VkBufferImageCopy bufferCopyRegion = {};
            bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.mipLevel = 0;
            bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
            bufferCopyRegion.imageSubresource.layerCount = 1;
            bufferCopyRegion.imageExtent.width = mWidth;
            bufferCopyRegion.imageExtent.height = mHeight;
            bufferCopyRegion.imageExtent.depth = 1;
            bufferCopyRegion.bufferOffset = 0;

            vkCmdCopyBufferToImage(cmd, stagingBuffer, image->GetVulkanImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);

            if (!mSpecification.UseMips)
            {
                VulkanUtils::InsertImageMemoryBarrier(cmd, image->GetVulkanImage(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                                      subresourceRange);
            }
            else
            {
                VulkanUtils::InsertImageMemoryBarrier(cmd, image->GetVulkanImage(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, subresourceRange);
            }
        });
        vmaDestroyBuffer(allocator, stagingBuffer, stagingBufferAllocation);

        if (mSpecification.UseMips)
            GenerateMips();
    }

    void VulkanTexture2D::GenerateMips()
    {
        Ref<VulkanImage2D> image = mImage.As<VulkanImage2D>();
        ImageSpecification& imageSpec = mImage->GetSpecification();

        RHI::gVulkanRHIDevice.InstantSubmit(RHI::QueueType::Graphics, [&](VkCommandBuffer cmd) {
            VkImage& vulkanImage = image->GetVulkanImage();

            for (Uint i = 1; i < imageSpec.Mips; i++)
            {
                VkImageBlit imageBlit {};

                imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                imageBlit.srcSubresource.layerCount = 1;
                imageBlit.srcSubresource.mipLevel = i - 1;
                imageBlit.srcOffsets[1].x = int32_t(mWidth >> (i - 1));
                imageBlit.srcOffsets[1].y = int32_t(mHeight >> (i - 1));
                imageBlit.srcOffsets[1].z = 1;

                imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                imageBlit.dstSubresource.layerCount = 1;
                imageBlit.dstSubresource.mipLevel = i;
                imageBlit.dstOffsets[1].x = int32_t(mWidth >> i);
                imageBlit.dstOffsets[1].y = int32_t(mHeight >> i);
                imageBlit.dstOffsets[1].z = 1;

                VkImageSubresourceRange mipSubRange {};
                mipSubRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                mipSubRange.baseMipLevel = i;
                mipSubRange.levelCount = 1;
                mipSubRange.layerCount = 1;

                VulkanUtils::InsertImageMemoryBarrier(cmd, vulkanImage,
                                                      0, VK_ACCESS_TRANSFER_WRITE_BIT,
                                                      VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                      VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, mipSubRange);

                vkCmdBlitImage(cmd, vulkanImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vulkanImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_LINEAR);

                VulkanUtils::InsertImageMemoryBarrier(cmd, vulkanImage,
                                                      VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                                      VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, mipSubRange);
            }

            VkImageSubresourceRange subresourceRange = {};
            subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            subresourceRange.layerCount = 1;
            subresourceRange.levelCount = imageSpec.Mips;

            VulkanUtils::InsertImageMemoryBarrier(cmd, vulkanImage,
                                                  VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
                                                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, subresourceRange);
        });
    }
} // namespace Surge
