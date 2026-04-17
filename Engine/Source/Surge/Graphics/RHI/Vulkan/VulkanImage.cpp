// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RHI/Vulkan/VulkanImage.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHIDevice.hpp"
#include <vk_mem_alloc.h>

namespace Surge
{
    VulkanImage2D::VulkanImage2D(const ImageSpecification& specification)
        : mSpecification(specification)
    {
        Invalidate();
    }

    void VulkanImage2D::Release()
    {
        if (mImage == VK_NULL_HANDLE)
            return;

        VkDevice device = RHI::gVulkanRHIDevice.GetLogicalDevice();
        vkDeviceWaitIdle(device);
        vkDestroyImageView(device, mImageView, nullptr);
        vkDestroySampler(device, mImageSampler, nullptr);
        vmaDestroyImage(RHI::gVulkanRHIDevice.GetVMAAllocator(), mImage, mImageMemory);

        mImage = VK_NULL_HANDLE;
        mImageView = VK_NULL_HANDLE;
        mImageSampler = VK_NULL_HANDLE;
        mImageMemory = nullptr;
    }

    void VulkanImage2D::Invalidate()
    {
        VkDevice logicalDevice = RHI::gVulkanRHIDevice.GetLogicalDevice();
        VmaAllocator allocator = RHI::gVulkanRHIDevice.GetVMAAllocator();

        VkFormat textureFormat = VulkanUtils::GetImageFormat(mSpecification.Format);
        VkImageUsageFlags usageFlags = VulkanUtils::GetImageUsageFlags(mSpecification.Usage, mSpecification.Format);

        VkImageAspectFlags aspectMask = VulkanUtils::IsDepthFormat(mSpecification.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        if (mSpecification.Format == ImageFormat::Depth24Stencil8)
            aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

        VkImageCreateInfo imageInfo {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = mSpecification.Width;
        imageInfo.extent.height = mSpecification.Height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = mSpecification.Mips;
        imageInfo.arrayLayers = 1;
        imageInfo.format = textureFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usageFlags;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        vmaCreateImage(allocator, &imageInfo, &allocInfo, &mImage, &mImageMemory, nullptr);
        SET_VK_OBJECT_DEBUGNAME(mImage, VK_OBJECT_TYPE_IMAGE, "Image");

        // Create the image view
        VkImageViewCreateInfo imageViewCreateInfo {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = imageInfo.format;
        imageViewCreateInfo.subresourceRange = {};
        imageViewCreateInfo.subresourceRange.aspectMask = aspectMask;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = mSpecification.Mips;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;
        imageViewCreateInfo.image = mImage;
        VK_CALL(vkCreateImageView(logicalDevice, &imageViewCreateInfo, nullptr, &mImageView));
        SET_VK_OBJECT_DEBUGNAME(mImageView, VK_OBJECT_TYPE_IMAGE_VIEW, "Image View");

        // Sampler
        VkSamplerCreateInfo samplerCreateInfo {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        samplerCreateInfo.anisotropyEnable = mSpecification.SamplerProps.EnableAnisotropy;
        samplerCreateInfo.magFilter = VulkanUtils::GetImageFiltering(mSpecification.SamplerProps.SamplerFilter);
        samplerCreateInfo.minFilter = samplerCreateInfo.magFilter;
        samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCreateInfo.addressModeU = VulkanUtils::GetImageAddressMode(mSpecification.SamplerProps.SamplerAddressMode);
        samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
        samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
        samplerCreateInfo.mipLodBias = 0.0f;
        samplerCreateInfo.minLod = 0.0f;
        samplerCreateInfo.maxLod = float(mSpecification.Mips);
        samplerCreateInfo.compareEnable = mSpecification.SamplerProps.EnableComparison;
        samplerCreateInfo.compareOp = VulkanUtils::CompareOpToVkCompareOp(mSpecification.SamplerProps.SamplerCompareOp);

        VK_CALL(vkCreateSampler(logicalDevice, &samplerCreateInfo, nullptr, &mImageSampler));
        SET_VK_OBJECT_DEBUGNAME(mImageSampler, VK_OBJECT_TYPE_SAMPLER, "Sampler");

        // Transition Storage images to VK_IMAGE_LAYOUT_GENERAL
        if (mSpecification.Usage == ImageUsage::Storage)
        {
            RHI::gVulkanRHIDevice.InstantSubmit(RHI::QueueType::Graphics, [&](VkCommandBuffer cmd) {
                VkImageSubresourceRange subresourceRange {};
                subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                subresourceRange.baseMipLevel = 0;
                subresourceRange.levelCount = mSpecification.Mips;
                subresourceRange.layerCount = 1;

                VulkanUtils::InsertImageMemoryBarrier(cmd, mImage, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                                      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, subresourceRange);
            });
        }

        UpdateDescriptor();
    }

    void VulkanImage2D::UpdateDescriptor()
    {
        if (mSpecification.Format == ImageFormat::Depth32 || mSpecification.Format == ImageFormat::Depth24Stencil8)
            mDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        else if (mSpecification.Usage == ImageUsage::Storage)
            mDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        else
            mDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        mDescriptorInfo.imageView = mImageView;
        mDescriptorInfo.sampler = mImageSampler;
    }

    VulkanImage2D::~VulkanImage2D()
    {
        VkDevice device = RHI::gVulkanRHIDevice.GetLogicalDevice();
        vkDeviceWaitIdle(device);

        if (mImageSampler)
        {
            vkDestroySampler(device, mImageSampler, nullptr);
            mImageSampler = VK_NULL_HANDLE;
        }
        if (mImageView)
        {
            vkDestroyImageView(device, mImageView, nullptr);
            mImageView = VK_NULL_HANDLE;
        }
        if (mImage)
        {
            vmaDestroyImage(RHI::gVulkanRHIDevice.GetVMAAllocator(), mImage, mImageMemory);
            mImage = VK_NULL_HANDLE;
            mImageMemory = nullptr;
        }
    }
} // namespace Surge
