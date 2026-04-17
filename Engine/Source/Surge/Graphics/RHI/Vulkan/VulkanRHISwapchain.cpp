// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RHI/Vulkan/VulkanRHISwapchain.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHIDevice.hpp"
#include <volk.h>

namespace Surge::RHI
{
    static void CreateSwapchainImages(VulkanSwapchain& sc)
    {
        VkDevice device = gVulkanRHIDevice.GetLogicalDevice();

        vkGetSwapchainImagesKHR(device, sc.Swapchain, &sc.ImageCount, nullptr);
        sc.Images.resize(sc.ImageCount);
        vkGetSwapchainImagesKHR(device, sc.Swapchain, &sc.ImageCount, sc.Images.data());

        sc.ImageViews.resize(sc.ImageCount);
        for (uint32_t i = 0; i < sc.ImageCount; ++i)
        {
            VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            viewInfo.image                 = sc.Images[i];
            viewInfo.viewType              = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format                = sc.ColorFormat;
            viewInfo.components            = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                              VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
            viewInfo.subresourceRange      = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            vkCreateImageView(device, &viewInfo, nullptr, &sc.ImageViews[i]);
        }
    }

    static void CreateSwapchainRenderPass(VulkanSwapchain& sc)
    {
        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format         = sc.ColorFormat;
        colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorRef = {};
        colorRef.attachment = 0;
        colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments    = &colorRef;

        VkSubpassDependency dep = {};
        dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass    = 0;
        dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo rpInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        rpInfo.attachmentCount = 1;
        rpInfo.pAttachments    = &colorAttachment;
        rpInfo.subpassCount    = 1;
        rpInfo.pSubpasses      = &subpass;
        rpInfo.dependencyCount = 1;
        rpInfo.pDependencies   = &dep;

        vkCreateRenderPass(gVulkanRHIDevice.GetLogicalDevice(), &rpInfo, nullptr, &sc.RenderPass);
    }

    static void CreateSwapchainFramebuffers(VulkanSwapchain& sc)
    {
        VkDevice device = gVulkanRHIDevice.GetLogicalDevice();
        sc.Framebuffers.resize(sc.ImageCount);
        for (uint32_t i = 0; i < sc.ImageCount; ++i)
        {
            VkFramebufferCreateInfo fbInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            fbInfo.renderPass      = sc.RenderPass;
            fbInfo.attachmentCount = 1;
            fbInfo.pAttachments    = &sc.ImageViews[i];
            fbInfo.width           = sc.Extent.width;
            fbInfo.height          = sc.Extent.height;
            fbInfo.layers          = 1;
            vkCreateFramebuffer(device, &fbInfo, nullptr, &sc.Framebuffers[i]);
        }
    }

    static void CreateSwapchainSyncObjects(VulkanSwapchain& sc, uint32_t framesInFlight)
    {
        VkDevice device = gVulkanRHIDevice.GetLogicalDevice();

        // Command pool
        VkCommandPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        poolInfo.queueFamilyIndex = gVulkanRHIDevice.GetGraphicsFamily();
        poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        vkCreateCommandPool(device, &poolInfo, nullptr, &sc.CommandPool);

        sc.RenderSemaphores.resize(sc.ImageCount);
        for (auto& sem : sc.RenderSemaphores)
        {
            VkSemaphoreCreateInfo si = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
            vkCreateSemaphore(device, &si, nullptr, &sem);
        }

        for (uint32_t i = 0; i < framesInFlight; ++i)
        {
            auto& frame = sc.Frames[i];

            VkCommandBufferAllocateInfo ai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            ai.commandPool        = sc.CommandPool;
            ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            ai.commandBufferCount = 1;
            vkAllocateCommandBuffers(device, &ai, &frame.CmdBuf);

            VkSemaphoreCreateInfo si = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
            vkCreateSemaphore(device, &si, nullptr, &frame.PresentSemaphore);

            VkFenceCreateInfo fi = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            vkCreateFence(device, &fi, nullptr, &frame.Fence);
        }
    }

    SwapchainHandle vkRHICreateSwapchain(const SwapchainDesc& desc)
    {
        VkDevice         device    = gVulkanRHIDevice.GetLogicalDevice();
        VkPhysicalDevice physDev   = gVulkanRHIDevice.GetPhysicalDevice();
        VkInstance       instance  = gVulkanRHIDevice.GetInstance();

        VulkanSwapchain sc;
        sc.Vsync = desc.Vsync;

        // Surface
        VkWin32SurfaceCreateInfoKHR surfCI = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
        surfCI.hwnd      = static_cast<HWND>(desc.WindowHandle);
        surfCI.hinstance = GetModuleHandleA(nullptr);
        vkCreateWin32SurfaceKHR(instance, &surfCI, nullptr, &sc.Surface);

        // Surface capabilities
        VkSurfaceCapabilitiesKHR caps;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDev, sc.Surface, &caps);

        // Format
        uint32_t fmtCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physDev, sc.Surface, &fmtCount, nullptr);
        Vector<VkSurfaceFormatKHR> formats(fmtCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physDev, sc.Surface, &fmtCount, formats.data());
        sc.ColorFormat = formats[0].format;
        VkColorSpaceKHR colorSpace = formats[0].colorSpace;
        for (const auto& f : formats)
        {
            if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                sc.ColorFormat = f.format;
                colorSpace     = f.colorSpace;
                break;
            }
        }

        // Present mode
        uint32_t pmCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physDev, sc.Surface, &pmCount, nullptr);
        Vector<VkPresentModeKHR> presentModes(pmCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physDev, sc.Surface, &pmCount, presentModes.data());
        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
        if (!desc.Vsync)
        {
            for (VkPresentModeKHR pm : presentModes)
                if (pm == VK_PRESENT_MODE_MAILBOX_KHR) { presentMode = pm; break; }
        }

        // Extent
        sc.Extent = {desc.Width, desc.Height};
        if (caps.currentExtent.width != UINT32_MAX)
            sc.Extent = caps.currentExtent;

        uint32_t imageCount = std::max(caps.minImageCount + 1, desc.FramesInFlight);
        if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
            imageCount = caps.maxImageCount;

        VkSwapchainCreateInfoKHR swapCI = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        swapCI.surface          = sc.Surface;
        swapCI.minImageCount    = imageCount;
        swapCI.imageFormat      = sc.ColorFormat;
        swapCI.imageColorSpace  = colorSpace;
        swapCI.imageExtent      = sc.Extent;
        swapCI.imageArrayLayers = 1;
        swapCI.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        swapCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapCI.preTransform     = caps.currentTransform;
        swapCI.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapCI.presentMode      = presentMode;
        swapCI.clipped          = VK_TRUE;

        vkCreateSwapchainKHR(device, &swapCI, nullptr, &sc.Swapchain);

        CreateSwapchainImages(sc);
        CreateSwapchainRenderPass(sc);
        CreateSwapchainFramebuffers(sc);
        CreateSwapchainSyncObjects(sc, desc.FramesInFlight);

        return GetVulkanRegistry().Swapchains.Create(std::move(sc));
    }

    void vkRHIDestroySwapchain(SwapchainHandle handle)
    {
        auto& reg    = GetVulkanRegistry();
        auto& sc     = reg.GetSwapchain(handle);
        VkDevice dev = gVulkanRHIDevice.GetLogicalDevice();

        vkDeviceWaitIdle(dev);

        for (auto& frame : sc.Frames)
        {
            if (frame.Fence)         vkDestroyFence(dev, frame.Fence, nullptr);
            if (frame.PresentSemaphore) vkDestroySemaphore(dev, frame.PresentSemaphore, nullptr);
        }
        for (auto sem : sc.RenderSemaphores) vkDestroySemaphore(dev, sem, nullptr);
        if (sc.CommandPool) vkDestroyCommandPool(dev, sc.CommandPool, nullptr);
        for (auto fb : sc.Framebuffers)   vkDestroyFramebuffer(dev, fb, nullptr);
        if (sc.RenderPass)  vkDestroyRenderPass(dev, sc.RenderPass, nullptr);
        for (auto iv : sc.ImageViews)     vkDestroyImageView(dev, iv, nullptr);
        if (sc.Swapchain)   vkDestroySwapchainKHR(dev, sc.Swapchain, nullptr);
        if (sc.Surface)     vkDestroySurfaceKHR(gVulkanRHIDevice.GetInstance(), sc.Surface, nullptr);

        reg.Swapchains.Destroy(handle);
    }

    void vkRHISwapchainBeginFrame(SwapchainHandle handle)
    {
        auto& reg = GetVulkanRegistry();
        auto& sc  = reg.GetSwapchain(handle);
        VkDevice dev = gVulkanRHIDevice.GetLogicalDevice();

        auto& frame = sc.Frames[sc.CurrentFrameIdx];
        vkWaitForFences(dev, 1, &frame.Fence, VK_TRUE, UINT64_MAX);
        vkResetFences(dev, 1, &frame.Fence);

        vkAcquireNextImageKHR(dev, sc.Swapchain, UINT64_MAX,
                              frame.PresentSemaphore, VK_NULL_HANDLE, &sc.CurrentImageIdx);

        VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(frame.CmdBuf, &beginInfo);
    }

    void vkRHISwapchainEndFrame(SwapchainHandle handle, TextureHandle /*blitSrc*/)
    {
        auto& reg = GetVulkanRegistry();
        auto& sc  = reg.GetSwapchain(handle);
        VkDevice dev = gVulkanRHIDevice.GetLogicalDevice();

        auto& frame = sc.Frames[sc.CurrentFrameIdx];
        vkEndCommandBuffer(frame.CmdBuf);

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.waitSemaphoreCount   = 1;
        si.pWaitSemaphores      = &frame.PresentSemaphore;
        si.pWaitDstStageMask    = &waitStage;
        si.commandBufferCount   = 1;
        si.pCommandBuffers      = &frame.CmdBuf;
        si.signalSemaphoreCount = 1;
        si.pSignalSemaphores    = &sc.RenderSemaphores[sc.CurrentImageIdx];
        vkQueueSubmit(gVulkanRHIDevice.GetGraphicsQueue(), 1, &si, frame.Fence);

        VkPresentInfoKHR pi = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        pi.waitSemaphoreCount = 1;
        pi.pWaitSemaphores    = &sc.RenderSemaphores[sc.CurrentImageIdx];
        pi.swapchainCount     = 1;
        pi.pSwapchains        = &sc.Swapchain;
        pi.pImageIndices      = &sc.CurrentImageIdx;
        vkQueuePresentKHR(gVulkanRHIDevice.GetGraphicsQueue(), &pi);

        sc.CurrentFrameIdx = (sc.CurrentFrameIdx + 1) % 3;
    }

    void vkRHISwapchainResize(SwapchainHandle handle, uint32_t width, uint32_t height)
    {
        // Destroy and recreate the swapchain with new dimensions.
        auto& reg = GetVulkanRegistry();
        auto& sc  = reg.GetSwapchain(handle);
        VkDevice dev = gVulkanRHIDevice.GetLogicalDevice();
        vkDeviceWaitIdle(dev);

        // Destroy old swapchain objects (keep Surface)
        for (auto fb : sc.Framebuffers) vkDestroyFramebuffer(dev, fb, nullptr);
        if (sc.RenderPass) vkDestroyRenderPass(dev, sc.RenderPass, nullptr);
        for (auto iv : sc.ImageViews)   vkDestroyImageView(dev, iv, nullptr);

        VkSwapchainKHR old = sc.Swapchain;
        sc.Extent = {width, height};
        sc.Images.clear();
        sc.ImageViews.clear();
        sc.Framebuffers.clear();

        VkSurfaceCapabilitiesKHR caps;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gVulkanRHIDevice.GetPhysicalDevice(), sc.Surface, &caps);

        VkSwapchainCreateInfoKHR swapCI = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        swapCI.surface          = sc.Surface;
        swapCI.minImageCount    = sc.ImageCount;
        swapCI.imageFormat      = sc.ColorFormat;
        swapCI.imageColorSpace  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        swapCI.imageExtent      = sc.Extent;
        swapCI.imageArrayLayers = 1;
        swapCI.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        swapCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapCI.preTransform     = caps.currentTransform;
        swapCI.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapCI.presentMode      = sc.Vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR;
        swapCI.clipped          = VK_TRUE;
        swapCI.oldSwapchain     = old;

        vkCreateSwapchainKHR(dev, &swapCI, nullptr, &sc.Swapchain);
        vkDestroySwapchainKHR(dev, old, nullptr);

        CreateSwapchainImages(sc);
        CreateSwapchainRenderPass(sc);
        CreateSwapchainFramebuffers(sc);
    }

    uint32_t vkRHISwapchainGetFrameIndex(SwapchainHandle handle)
    {
        return GetVulkanRegistry().GetSwapchain(handle).CurrentFrameIdx;
    }

    uint32_t vkRHISwapchainGetImageCount(SwapchainHandle handle)
    {
        return GetVulkanRegistry().GetSwapchain(handle).ImageCount;
    }

} // namespace Surge::RHI
