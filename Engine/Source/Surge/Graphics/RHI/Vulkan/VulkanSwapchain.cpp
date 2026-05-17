// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RHI/Vulkan/VulkanSwapchain.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHI.hpp"

namespace Surge
{
    void VulkanSwapchain::Initialize(const VulkanRHI& rhi, Uint width, Uint height)
    {
        Create(rhi, width, height);
    }

    void VulkanSwapchain::Resize(const VulkanRHI& rhi, Uint width, Uint height)
    {
        Destroy(rhi);
        Create(rhi, width, height);
    }

    void VulkanSwapchain::Shutdown(const VulkanRHI& rhi)
    {
        Destroy(rhi);
    }

    VkResult VulkanSwapchain::AcquireNextImage(const VulkanRHI& rhi, VkSemaphore acquireSemaphore, Uint& outImageIndex)
    {
        return vkAcquireNextImageKHR(rhi.GetDevice(), mSwapchain, UINT64_MAX, acquireSemaphore, VK_NULL_HANDLE, &outImageIndex);
    }

    VkResult VulkanSwapchain::Present(const VulkanRHI& rhi, VkSemaphore releaseSemaphore, Uint imageIndex)
    {
        VkQueue queue = rhi.GetQueue();
        VkPresentInfoKHR present{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &releaseSemaphore,
            .swapchainCount = 1,
            .pSwapchains = &mSwapchain,
            .pImageIndices = &imageIndex,
        };
        
        return vkQueuePresentKHR(queue, &present);
    }

    void VulkanSwapchain::Create(const VulkanRHI& rhi, Uint width, Uint height)
    {
        // Proudly stolen from Sascha Willems Vulkan Examples

        VkDevice device = rhi.GetDevice();
        VkPhysicalDevice gpu = rhi.GetGPU();
        VkSurfaceKHR surface = rhi.GetSurface();

        VkSurfaceCapabilitiesKHR surfaceProperties;
        VK_CALL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &surfaceProperties));

        VkSurfaceFormatKHR format = SelectSurfaceFormat(gpu, surface);

        VkExtent2D swapchainSize{};
        if (surfaceProperties.currentExtent.width == 0xFFFFFFFF)
        {
            swapchainSize.width = width;
            swapchainSize.height = height;
            Log<Severity::Debug>("VulkanSwapchain: Using custom swapchain size Width:{} Height:{}", swapchainSize.width, swapchainSize.height);
        }
        else		
            swapchainSize = surfaceProperties.currentExtent;
        
        
        VkPresentModeKHR swapchainPresentMode = SelectPresentMode(gpu, surface);

        // Determine the number of VkImage's to use in the swapchain.
        // Ideally, we desire to own 1 image at a time, the rest of the images can
        // either be rendered to and/or being queued up for display.
        uint32_t desiredSwapchainImages = surfaceProperties.minImageCount + 1;
        if ((surfaceProperties.maxImageCount > 0) && (desiredSwapchainImages > surfaceProperties.maxImageCount))
        {
            // Application must settle for fewer images than desired.
            desiredSwapchainImages = surfaceProperties.maxImageCount;
        }

        // Figure out a suitable surface transform
        VkSurfaceTransformFlagBitsKHR preTransform;
        if (surfaceProperties.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)		
            preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        
        else		
            preTransform = surfaceProperties.currentTransform;		

        VkSwapchainKHR oldSwapchain = mSwapchain;

        // Find a supported composite type
        VkCompositeAlphaFlagBitsKHR composite = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        if (surfaceProperties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)		
            composite = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;		
        else if (surfaceProperties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)		
            composite = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;		
        else if (surfaceProperties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)		
            composite = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;		
        else if (surfaceProperties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)		
            composite = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
        
        VkSwapchainCreateInfoKHR info{};
        info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        info.surface = surface;
        info.minImageCount = desiredSwapchainImages;
        info.imageFormat = format.format;
        info.imageColorSpace = format.colorSpace;
        info.imageExtent = swapchainSize;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        info.imageArrayLayers = 1;
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.preTransform = preTransform;
        info.compositeAlpha = composite;
        info.presentMode = swapchainPresentMode;
        info.clipped = true;
        info.oldSwapchain = oldSwapchain;

        if (RHISettings::BLIT_TO_SWAPCHAIN)
            info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        VK_CALL(vkCreateSwapchainKHR(device, &info, nullptr, &mSwapchain));
        Log<Severity::Debug>("VulkanSwapchain created with Width:{} Height:{}", swapchainSize.width, swapchainSize.height);

        if (oldSwapchain != VK_NULL_HANDLE)
        {
            for (SwapchainFrame frame : mFrames)
            {
                vkDestroyImageView(device, frame.View, nullptr);
            }

            mFrames.clear();
            vkDestroySwapchainKHR(device, oldSwapchain, nullptr);
        }

        mDimensions = { swapchainSize.width, swapchainSize.height, format.format };

        /// Retrieve swapchain images
        uint32_t imageCount;
        VK_CALL(vkGetSwapchainImagesKHR(device, mSwapchain, &imageCount, nullptr));
        std::vector<VkImage> swapchainImages(imageCount); 
        VK_CALL(vkGetSwapchainImagesKHR(device, mSwapchain, &imageCount, swapchainImages.data()));

        for (size_t i = 0; i < imageCount; i++)
        {
            // Create an image view which we can render into
            VkImageViewCreateInfo view_info{
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext = nullptr,
                .image = swapchainImages[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = mDimensions.Format,
                .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1} };

            VkImageView imageView;
            VK_CALL(vkCreateImageView(device, &view_info, nullptr, &imageView));
            
            VkSemaphore releaseSemaphore;
            VkSemaphoreCreateInfo semInfo = {};
            semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            VK_CALL(vkCreateSemaphore(device, &semInfo, nullptr, &releaseSemaphore));

            mFrames.push_back({ swapchainImages[i], imageView, releaseSemaphore });
        }

    }

    void VulkanSwapchain::Destroy(const VulkanRHI& rhi)
    {
        VkDevice device = rhi.GetDevice();
        for (auto& frame : mFrames)
        {
            if (frame.View != VK_NULL_HANDLE)
                vkDestroyImageView(device, frame.View, nullptr);

            if (frame.ReleaseSemaphore != VK_NULL_HANDLE)
                vkDestroySemaphore(device, frame.ReleaseSemaphore, nullptr);
        }
        mFrames.clear();

        if (mSwapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(device, mSwapchain, nullptr);
            mSwapchain = VK_NULL_HANDLE;
        }
    }

    VkSurfaceFormatKHR VulkanSwapchain::SelectSurfaceFormat(VkPhysicalDevice gpu, VkSurfaceKHR surface)
    {
        Uint count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &count, nullptr);
        Vector<VkSurfaceFormatKHR> formats(count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &count, formats.data());

        for (auto& f : formats)
        {
            if ((f.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 || f.format == VK_FORMAT_B8G8R8A8_UNORM)
                &&
                f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                return f;
        }
        return formats[0]; // Fallback
    }

    VkPresentModeKHR VulkanSwapchain::SelectPresentMode(VkPhysicalDevice gpu, VkSurfaceKHR surface)
    {
        Uint count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &count, nullptr);
        Vector<VkPresentModeKHR> modes(count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &count, modes.data());

        // On mobile -> FIFO is preferred (vsync, battery friendly)
        // MAILBOX gives lower latency(ImGui feels smooth) but burns more power
#ifdef SURGE_PLATFORM_WINDOWS
        for (auto& m : modes)
        {
            if (m == VK_PRESENT_MODE_MAILBOX_KHR) //VK_PRESENT_MODE_MAILBOX_KHR to non Vsync
                return m;
        }
#endif
        // FIFO must be supported by all implementations
        return VK_PRESENT_MODE_FIFO_KHR;
    }

}
