// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RHI/RHIDevice.hpp"

// ---------------------------------------------------------------------------
// RHISwapchain.hpp
//   Free functions for swapchain management, delegating to the global
//   RHIDevice dispatch table.
// ---------------------------------------------------------------------------

namespace Surge::RHI
{
    SURGE_API SwapchainHandle rhiCreateSwapchain(const SwapchainDesc& desc);
    SURGE_API void            rhiDestroySwapchain(SwapchainHandle handle);

    // BeginFrame acquires the next back-buffer image.
    SURGE_API void     rhiSwapchainBeginFrame(SwapchainHandle handle);

    // EndFrame presents. blitSrc (optional) is blit-copied onto the swapchain
    // image before present, matching the existing VulkanRenderContext pattern.
    SURGE_API void     rhiSwapchainEndFrame(SwapchainHandle handle,
                                             TextureHandle blitSrc = {});

    SURGE_API void     rhiSwapchainResize(SwapchainHandle handle,
                                           uint32_t width, uint32_t height);
    SURGE_API uint32_t rhiSwapchainGetFrameIndex(SwapchainHandle handle);
    SURGE_API uint32_t rhiSwapchainGetImageCount(SwapchainHandle handle);

} // namespace Surge::RHI
