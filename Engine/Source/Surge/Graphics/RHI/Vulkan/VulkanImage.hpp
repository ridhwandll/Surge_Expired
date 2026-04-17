// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RHI/Vulkan/VulkanUtils.hpp"
#include "Surge/Graphics/Interface/Image.hpp"

// VmaAllocation is an opaque pointer type from vk_mem_alloc.h
typedef struct VmaAllocation_T* VmaAllocation;

namespace Surge
{
    class SURGE_API VulkanImage2D : public Image2D
    {
    public:
        VulkanImage2D(const ImageSpecification& specification);
        virtual ~VulkanImage2D();

        virtual Uint GetWidth() const override { return mSpecification.Width; }
        virtual Uint GetHeight() const override { return mSpecification.Height; }
        virtual void Release() override;
        virtual ImageSpecification& GetSpecification() override { return mSpecification; }
        virtual const ImageSpecification& GetSpecification() const override { return mSpecification; }

        // Vulkan Specific
        VkImage& GetVulkanImage() { return mImage; }
        VkImageView& GetVulkanImageView() { return mImageView; }
        VkSampler& GetVulkanSampler() { return mImageSampler; }
        VkImageLayout GetVulkanImageLayout() { return mDescriptorInfo.imageLayout; }
        const VkDescriptorImageInfo& GetVulkanDescriptorImageInfo() const { return mDescriptorInfo; }

    private:
        void Invalidate();
        void UpdateDescriptor();

    private:
        ImageSpecification mSpecification;

        VkImage mImage = VK_NULL_HANDLE;
        VkImageView mImageView = VK_NULL_HANDLE;
        VkSampler mImageSampler = VK_NULL_HANDLE;
        VmaAllocation mImageMemory = nullptr;

        VkDescriptorImageInfo mDescriptorInfo;
        friend class SURGE_API VulkanTexture2D;
    };
} // namespace Surge
