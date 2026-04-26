// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Abstraction/Vulkan/VulkanSwapChain.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Defines.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanDevice.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanDiagnostics.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanRenderContext.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanUtils.hpp"

#include <array>

namespace Surge
{
    void VulkanSwapChain::Initialize(Window* window)
    {
        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        VkInstance instance = renderContext->GetInstance();
        VulkanUtils::CreateWindowSurface(instance, window, &mSurface);
        PickPresentQueue();
        CreateSwapChain();
        CreateRenderPass();
        CreateFramebuffer();
        CreateFrameObjects();
    }

    void VulkanSwapChain::CreateSwapChain()
    {
        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        VulkanDevice* vulkanDevice = renderContext->GetDevice();
        VkPhysicalDevice physicalDevice = vulkanDevice->GetPhysicalDevice();
        VkDevice device = vulkanDevice->GetLogicalDevice();

        VkSurfaceCapabilitiesKHR surfaceCapabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, mSurface, &surfaceCapabilities);

        Uint imageCount = 0;
        if (FRAMES_IN_FLIGHT < surfaceCapabilities.minImageCount)
            imageCount = surfaceCapabilities.minImageCount;
        else
            imageCount = FRAMES_IN_FLIGHT;

        VkExtent2D swapChainExtent = {};
        if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
            swapChainExtent = surfaceCapabilities.currentExtent;

        // Getting all swapchain formats
        Uint availableFormatCount = 0;
        Vector<VkSurfaceFormatKHR> availableFormats;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, mSurface, &availableFormatCount, nullptr);
        availableFormats.resize(availableFormatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, mSurface, &availableFormatCount, availableFormats.data());

        // Selecting the best swapchain format
        VkSurfaceFormatKHR pickedFormat = availableFormats[0]; // Default one is `availableFormats[0]`
        /*
        for (const VkSurfaceFormatKHR& availableFormat : availableFormats)
        {
            if (availableFormat.format == VK_FORMAT_R8G8B8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                pickedFormat = availableFormat;
        }
        */

        // Getting all swapchain presentModes
        Uint availablePresentModeCount = 0;
        Vector<VkPresentModeKHR> avaialbePresentModes;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, mSurface, &availablePresentModeCount, nullptr);
        avaialbePresentModes.resize(availablePresentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, mSurface, &availablePresentModeCount, avaialbePresentModes.data());

        // Selecting the best swapchain present mode
        VkPresentModeKHR pickedPresentMode = VK_PRESENT_MODE_FIFO_KHR; // Default one
        for (const auto& availablePresentMode : avaialbePresentModes)
        {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR && !mVsync)
                pickedPresentMode = availablePresentMode;
        }

        VkSwapchainCreateInfoKHR createInfo {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        createInfo.surface = mSurface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = pickedFormat.format;
        createInfo.imageColorSpace = pickedFormat.colorSpace;
        createInfo.imageExtent = swapChainExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; // VK_IMAGE_USAGE_TRANSFER_DST_BIT for blitting the final rendered image to the swapchain image
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // TODO(AC3R): Should add support for `VK_SHARING_MODE_CONCURRENT` later
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
        createInfo.preTransform = surfaceCapabilities.currentTransform;

        // Pick a composite alpha mode supported by the surface (Android often lacks OPAQUE)
        VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        const VkCompositeAlphaFlagBitsKHR compositeAlphaCandidates[] = {
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
            VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR};
        for (auto candidate : compositeAlphaCandidates)
        {
            if (surfaceCapabilities.supportedCompositeAlpha & candidate)
            {
                compositeAlpha = candidate;
                break;
            }
        }
        createInfo.compositeAlpha = compositeAlpha;
        createInfo.presentMode = pickedPresentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = mSwapChain ? mSwapChain : VK_NULL_HANDLE;

        VK_CALL(vkCreateSwapchainKHR(device, &createInfo, nullptr, &mSwapChain));

        vkGetSwapchainImagesKHR(device, mSwapChain, &imageCount, nullptr);
        mSwapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, mSwapChain, &imageCount, mSwapChainImages.data());

        // Setting up the members
        mColorFormat = pickedFormat;
        mSwapChainExtent = swapChainExtent;
        mImageCount = imageCount;

        Log<Severity::Info>("Swapchain Image count {0}, Frames in flight {1}", mImageCount, FRAMES_IN_FLIGHT);        
    }

    void VulkanSwapChain::CreateRenderPass()
    {
        // Creating here the renderPass because we need it for the framebuffer, for imgui and other stuff.
        // The rendered image will be passed to this renderpass by rendering a quad with the final texture or viewport
        // and then it will pe presented
        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        VkDevice device = renderContext->GetDevice()->GetLogicalDevice();

        VkAttachmentDescription colorAttachment {};
        colorAttachment.format = mColorFormat.format;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef {};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkSubpassDependency dependency = {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        VK_CALL(vkCreateRenderPass(device, &renderPassInfo, nullptr, &mRenderPass));
    }

    void VulkanSwapChain::CreateFramebuffer()
    {
        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        VkDevice device = renderContext->GetDevice()->GetLogicalDevice();

        // Creating the VkImageViews
        mSwapChainImageViews.resize(mSwapChainImages.size());
        for (Uint i = 0; i < mSwapChainImages.size(); i++)
        {
            VkImage swapChainImage = mSwapChainImages[i];

            // Specifying that we are gonna use the imageviews as color attachments
            VkImageViewUsageCreateInfo usageInfo {VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO};
            usageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

            VkImageViewCreateInfo createInfo {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            createInfo.pNext = &usageInfo; // using pNext chain for letting vulkan know that this imageView is gonna be
                                           // used for the imageless framebuffer
            createInfo.image = swapChainImage;
            createInfo.format = mColorFormat.format;
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

            VK_CALL(vkCreateImageView(device, &createInfo, nullptr, &mSwapChainImageViews[i]));
        }

        // Imageless framebuffer (Vulkan Core feature 1.2)
        // Firstly we tell vulkan how we are gonna use the image views
        VkFramebufferAttachmentImageInfo attachmentImageInfo {VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO};
        attachmentImageInfo.pNext = nullptr;
        attachmentImageInfo.width = mSwapChainExtent.width;
        attachmentImageInfo.height = mSwapChainExtent.height;
        attachmentImageInfo.flags = 0;
        attachmentImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        attachmentImageInfo.layerCount = 1;
        attachmentImageInfo.viewFormatCount = 1;
        attachmentImageInfo.pViewFormats = &mColorFormat.format;

        // Reference the struct above here and specify how many attachments we have
        VkFramebufferAttachmentsCreateInfo attachmentCreateInfo {VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO};
        attachmentCreateInfo.attachmentImageInfoCount = 1;
        attachmentCreateInfo.pAttachmentImageInfos = &attachmentImageInfo;

        // Creation of the framebuffer is done using the pNext chain and with the flag
        // `VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT`
        VkFramebufferCreateInfo framebufferInfo {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        framebufferInfo.pNext = &attachmentCreateInfo;
        framebufferInfo.flags = VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT;
        framebufferInfo.renderPass = mRenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = nullptr; // Yes, nullptr
        framebufferInfo.width = mSwapChainExtent.width;
        framebufferInfo.height = mSwapChainExtent.height;
        framebufferInfo.layers = 1;

        VK_CALL(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &mFramebuffer));
    }

    void VulkanSwapChain::CreateFrameObjects()
    {
        VulkanRenderContext* vkContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(vkContext);
        VulkanDevice* device = vkContext->GetDevice();
        VkDevice vulkanDevice = device->GetLogicalDevice();

        // Command Pool Creation
        VkCommandPoolCreateInfo cmdPoolInfo {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        cmdPoolInfo.queueFamilyIndex = device->GetQueueFamilyIndices().GraphicsQueue;
        cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        VK_CALL(vkCreateCommandPool(device->GetLogicalDevice(), &cmdPoolInfo, nullptr, &mCommandPool));

        for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
        {
            VkCommandBufferAllocateInfo cbai {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            cbai.commandPool = mCommandPool;
            cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            cbai.commandBufferCount = 1;
            VK_CALL(vkAllocateCommandBuffers(vulkanDevice, &cbai, &mFrames[i].CmdBuf));

            VkSemaphoreCreateInfo sci {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
            VK_CALL(vkCreateSemaphore(vulkanDevice, &sci, nullptr, &mFrames[i].PresentSemaphore));

            VkFenceCreateInfo fci {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            VK_CALL(vkCreateFence(vulkanDevice, &fci, nullptr, &mFrames[i].Fence));
        }

        mRenderSemaphores.resize(mSwapChainImages.size());
        VkSemaphoreCreateInfo si {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        for (Uint i = 0; i < (Uint)mSwapChainImages.size(); i++)
            VK_CALL(vkCreateSemaphore(vulkanDevice, &si, nullptr, &mRenderSemaphores[i]));
    }

    void VulkanSwapChain::Resize()
    {
        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        VkDevice device = renderContext->GetDevice()->GetLogicalDevice();
        mCurrentFrameIndex = 0;

        // Wait till everything has finished rendering before deleting it
        vkDeviceWaitIdle(device);

        vkDestroyFramebuffer(device, mFramebuffer, nullptr);
        for (auto& imageView : mSwapChainImageViews)
            vkDestroyImageView(device, imageView, nullptr);

        CreateSwapChain();
        CreateFramebuffer();
    }

    void VulkanSwapChain::DestroyFrameObjects()
    {
        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        VkDevice device = renderContext->GetDevice()->GetLogicalDevice();

        vkDestroyCommandPool(device, mCommandPool, nullptr);

        for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
        {
            vkDestroySemaphore(device, mFrames[i].PresentSemaphore, nullptr);
            vkDestroyFence(device, mFrames[i].Fence, nullptr);
        }

        for (Uint i = 0; i < (Uint)mSwapChainImages.size(); i++)
            vkDestroySemaphore(device, mRenderSemaphores[i], nullptr);
    }

    void VulkanSwapChain::Destroy()
    {
        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        VkDevice device = renderContext->GetDevice()->GetLogicalDevice();
        VkInstance instance = renderContext->GetInstance();

        vkDeviceWaitIdle(device);

        vkDestroyRenderPass(device, mRenderPass, nullptr);
        vkDestroyFramebuffer(device, mFramebuffer, nullptr);

        for (auto& imageView : mSwapChainImageViews)
            vkDestroyImageView(device, imageView, nullptr);

        vkDestroySwapchainKHR(device, mSwapChain, nullptr);
        vkDestroySurfaceKHR(instance, mSurface, nullptr);

        DestroyFrameObjects();
    }

    VkResult VulkanSwapChain::AcquireNextImage(VkSemaphore imageAvailableSemaphore, Uint* imageIndex)
    {
        // By setting timeout to UINT64_MAX we will always wait until the next image has been acquired
        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        VkDevice device = renderContext->GetDevice()->GetLogicalDevice();
        return vkAcquireNextImageKHR(device, mSwapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, imageIndex);
    }

    void VulkanSwapChain::BeginFrame()
    {
        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        VkDevice device = renderContext->GetDevice()->GetLogicalDevice();
        Frame currentFrameObj = GetCurrentFrameObjects();
        VK_CALL(vkWaitForFences(device, 1, &currentFrameObj.Fence, VK_TRUE, UINT64_MAX));
        VK_CALL(vkResetFences(device, 1, &currentFrameObj.Fence));
        VK_CALL(AcquireNextImage(currentFrameObj.PresentSemaphore, &mCurrentImageIndex));
    }

    void VulkanSwapChain::Present()
    {
        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        VulkanDevice* device = renderContext->GetDevice();
        Frame currentFrameObj = GetCurrentFrameObjects();

        VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submitInfo.pWaitDstStageMask = &waitStageMask;
        submitInfo.pWaitSemaphores = &currentFrameObj.PresentSemaphore;
        submitInfo.pSignalSemaphores = &mRenderSemaphores[mCurrentImageIndex];
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pCommandBuffers = &currentFrameObj.CmdBuf;
        submitInfo.commandBufferCount = 1;

        VK_CALL(vkQueueSubmit(device->GetGraphicsQueue(), 1, &submitInfo, currentFrameObj.Fence));

        // Present
        VkPresentInfoKHR presentInfo {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &mRenderSemaphores[mCurrentImageIndex];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &mSwapChain;
        presentInfo.pImageIndices = &mCurrentImageIndex;
        VK_CHECK_WITHOUT_OUT_OF_DATE(vkQueuePresentKHR(mPresentQueue, &presentInfo));

        mCurrentFrameIndex = (mCurrentFrameIndex + 1) % FRAMES_IN_FLIGHT;
    }

    void VulkanSwapChain::EndFrame(VkImage blitSrcImage, VkExtent2D blitSrcExtent)
    {
        VulkanRenderContext* vkContext = static_cast<VulkanRenderContext*>(Core::GetRenderContext());
        Uint frameIndex = vkContext->GetFrameIndex();
        Frame currentFrameObj = GetCurrentFrameObjects();

        vkResetCommandBuffer(currentFrameObj.CmdBuf, 0);
        VkCommandBufferBeginInfo cmdBufInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        cmdBufInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        
        VK_CALL(vkBeginCommandBuffer(currentFrameObj.CmdBuf, &cmdBufInfo));

        if (blitSrcImage != VK_NULL_HANDLE)
        {
            VkImageSubresourceRange subresourceRange {};
            subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            subresourceRange.baseMipLevel = 0;
            subresourceRange.levelCount = 1;
            subresourceRange.baseArrayLayer = 0;
            subresourceRange.layerCount = 1;

            // Transition geometry output: SHADER_READ_ONLY_OPTIMAL -> TRANSFER_SRC_OPTIMAL
            VulkanUtils::InsertImageMemoryBarrier(currentFrameObj.CmdBuf, blitSrcImage,
                                                  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                                  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                  subresourceRange);

            // Transition swapchain image: UNDEFINED -> TRANSFER_DST_OPTIMAL
            VulkanUtils::InsertImageMemoryBarrier(currentFrameObj.CmdBuf, mSwapChainImages[mCurrentImageIndex],
                                                  0, VK_ACCESS_TRANSFER_WRITE_BIT,
                                                  VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                  subresourceRange);

            // Blit from geometry output to swapchain image
            VkImageBlit blitRegion {};
            blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blitRegion.srcSubresource.layerCount = 1;
            blitRegion.srcOffsets[0] = {0, 0, 0};
            blitRegion.srcOffsets[1] = {(int32_t)blitSrcExtent.width, (int32_t)blitSrcExtent.height, 1};
            blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blitRegion.dstSubresource.layerCount = 1;
            blitRegion.dstOffsets[0] = {0, 0, 0};
            blitRegion.dstOffsets[1] = {(int32_t)mSwapChainExtent.width, (int32_t)mSwapChainExtent.height, 1};

            vkCmdBlitImage(currentFrameObj.CmdBuf,
                           blitSrcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           mSwapChainImages[mCurrentImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &blitRegion, VK_FILTER_LINEAR);

            // Transition geometry output back: TRANSFER_SRC_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL
            VulkanUtils::InsertImageMemoryBarrier(currentFrameObj.CmdBuf, blitSrcImage,
                                                  VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
                                                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                                  subresourceRange);

            // Transition swapchain image: TRANSFER_DST_OPTIMAL -> PRESENT_SRC_KHR
            VulkanUtils::InsertImageMemoryBarrier(currentFrameObj.CmdBuf, mSwapChainImages[mCurrentImageIndex],
                                                  VK_ACCESS_TRANSFER_WRITE_BIT, 0,
                                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                                  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                                  subresourceRange);
        }
        else
        {
            // Render the ImGui
            BeginRenderPass();
            vkContext->RenderImGui();
            EndRenderPass();
        }
        VK_CALL(vkEndCommandBuffer(currentFrameObj.CmdBuf));

        Present();
    }

    void VulkanSwapChain::BeginRenderPass()
    {
        VkRenderPassAttachmentBeginInfo attachmentInfo {VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO};
        attachmentInfo.attachmentCount = 1;
        attachmentInfo.pAttachments = &mSwapChainImageViews[mCurrentImageIndex];

        std::array<VkClearValue, 2> clearValues;
        clearValues[0].color = {0.1f, 0.1f, 0.1f, 1.0f};
        clearValues[1].depthStencil = {1.0f, 0};

        VkRenderPassBeginInfo renderPassInfo {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        renderPassInfo.renderPass = mRenderPass;
        renderPassInfo.framebuffer = mFramebuffer;
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = mSwapChainExtent;
        renderPassInfo.pNext = &attachmentInfo; // Image less framebuffer
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(mFrames[mCurrentFrameIndex].CmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    void VulkanSwapChain::EndRenderPass()
    {
        vkCmdEndRenderPass(mFrames[mCurrentFrameIndex].CmdBuf);
    }

    Vector<VkCommandBuffer> VulkanSwapChain::GetVulkanCommandBuffers() const
    {
        Vector<VkCommandBuffer> cmdBfrs;        
        for (Uint i = 0; i < FRAMES_IN_FLIGHT; i++)        
            cmdBfrs.push_back(mFrames[i].CmdBuf);
           
        return cmdBfrs;
    }

    Vector<VkFence> VulkanSwapChain::GetVulkanFences() const
    {
        Vector<VkFence> fences;
        for (Uint i = 0; i < FRAMES_IN_FLIGHT; i++)
            fences.push_back(mFrames[i].Fence);

        return fences;
    }

    void VulkanSwapChain::PickPresentQueue()
    {
        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        VkDevice device = renderContext->GetDevice()->GetLogicalDevice();
        VkPhysicalDevice physicalDevice = renderContext->GetDevice()->GetPhysicalDevice();

        // Getting all the queueFamilies
        Vector<VkQueueFamilyProperties> queueFamilyProps;
        Uint queueFamilyCount;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        queueFamilyProps.resize(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProps.data());

        // Picking the one that has presentQueue
        for (Uint i = 0; i < queueFamilyProps.size(); i++)
        {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, mSurface, &presentSupport);

            if (presentSupport)
            {
                mPresentQueueIndex = i;
                break;
            }
        }

        // Getting the VkQueue using the presentQueueIndex
        vkGetDeviceQueue(device, mPresentQueueIndex, 0, &mPresentQueue);
    }
} // namespace Surge