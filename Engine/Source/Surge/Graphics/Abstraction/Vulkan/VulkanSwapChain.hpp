// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Window/Window.hpp"
#include <volk.h>

namespace Surge
{
    class SURGE_API VulkanSwapChain
    {
        struct Frame
        {
            VkCommandBuffer CmdBuf = VK_NULL_HANDLE;
            VkSemaphore PresentSemaphore = VK_NULL_HANDLE;
            VkFence Fence = VK_NULL_HANDLE;
        };
    public:
        VulkanSwapChain() = default;
        ~VulkanSwapChain() = default;

        void Initialize(Window* window);
        void BeginFrame();
        void EndFrame(VkImage blitSrcImage = VK_NULL_HANDLE, VkExtent2D blitSrcExtent = {});
        void Resize();
        void Destroy();
        void BeginRenderPass();
        void EndRenderPass();

        Uint GetImageCount() const { return mImageCount; }
        Uint GetWidth() const { return mSwapChainExtent.width; }
        Uint GetHeight() const { return mSwapChainExtent.height; }

        VkCommandPool GetCommandPool() { return mCommandPool; }
        VkExtent2D GetVulkanExtent2D() const { return mSwapChainExtent; }
        VkFormat GetVulkanColorFormat() const { return mColorFormat.format; }
        VkSwapchainKHR GetVulkanSwapChain() const { return mSwapChain; }
        VkRenderPass GetVulkanRenderPass() const { return mRenderPass; }
        VkFramebuffer GetVulkanFramebuffer() const { return mFramebuffer; }
        VkCommandPool GetVulkanCommandPool() const { return mCommandPool; }
        Vector<VkImageView> GetVulkanImageViews() const { return mSwapChainImageViews; }
        Vector<VkCommandBuffer> GetVulkanCommandBuffers() const;
        Vector<VkFence> GetVulkanFences() const;
        Uint GetCurrentFrameIndex() const { return mCurrentFrameIndex; }
        Uint GetCurrentImageIndex() const { return mCurrentImageIndex; }

    private:
        void Present();
        VkResult AcquireNextImage(VkSemaphore imageAvailableSemaphore, Uint* imageIndex);
        void PickPresentQueue();
        void CreateSwapChain();
        void CreateRenderPass();
        void CreateFramebuffer();
        void CreateFrameObjects();
        void DestroyFrameObjects();
        inline Frame GetCurrentFrameObjects() { return mFrames[mCurrentFrameIndex]; }
    private:
        // Swapchain
        VkSwapchainKHR mSwapChain = VK_NULL_HANDLE;
        VkSurfaceKHR mSurface;
        VkExtent2D mSwapChainExtent;
        VkSurfaceFormatKHR mColorFormat;
        Uint mImageCount;
        Uint mCurrentImageIndex = 0;
        Uint mCurrentFrameIndex = 0;

        // Framebuffers + Renderpasses
        VkRenderPass mRenderPass;
        Vector<VkImage> mSwapChainImages;
        Vector<VkImageView> mSwapChainImageViews;
        VkFramebuffer mFramebuffer;

        // Presentation Queue stuff
        VkQueue mPresentQueue;
        Uint mPresentQueueIndex;

        // Frame objects
        VkCommandPool mCommandPool = VK_NULL_HANDLE;
        std::vector<VkSemaphore> mRenderSemaphores;

        Frame mFrames[FRAMES_IN_FLIGHT];
        bool mVsync = false;
    };

} // namespace Surge
