// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RHI/Vulkan/VulkanResources.hpp"

// ---------------------------------------------------------------------------
// VulkanRHISwapchain.hpp
//   Vulkan implementation of the RHI swapchain free functions.
//   Installed into the RHIDevice dispatch table by VulkanRHIDevice.
// ---------------------------------------------------------------------------

namespace Surge::RHI
{
    SwapchainHandle vkRHICreateSwapchain(const SwapchainDesc& desc);
    void            vkRHIDestroySwapchain(SwapchainHandle handle);
    void            vkRHISwapchainBeginFrame(SwapchainHandle handle);
    void            vkRHISwapchainEndFrame(SwapchainHandle handle, TextureHandle blitSrc);
    void            vkRHISwapchainResize(SwapchainHandle handle, uint32_t width, uint32_t height);
    uint32_t        vkRHISwapchainGetFrameIndex(SwapchainHandle handle);
    uint32_t        vkRHISwapchainGetImageCount(SwapchainHandle handle);

} // namespace Surge::RHI
