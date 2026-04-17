// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RHI/RHISwapchain.hpp"

namespace Surge::RHI
{
    SwapchainHandle rhiCreateSwapchain(const SwapchainDesc& desc)
    {
        return GetRHIDevice()->createSwapchain(desc);
    }

    void rhiDestroySwapchain(SwapchainHandle handle)
    {
        GetRHIDevice()->destroySwapchain(handle);
    }

    void rhiSwapchainBeginFrame(SwapchainHandle handle)
    {
        GetRHIDevice()->swapchainBeginFrame(handle);
    }

    void rhiSwapchainEndFrame(SwapchainHandle handle, TextureHandle blitSrc)
    {
        GetRHIDevice()->swapchainEndFrame(handle, blitSrc);
    }

    void rhiSwapchainResize(SwapchainHandle handle, uint32_t width, uint32_t height)
    {
        GetRHIDevice()->swapchainResize(handle, width, height);
    }

    uint32_t rhiSwapchainGetFrameIndex(SwapchainHandle handle)
    {
        return GetRHIDevice()->swapchainGetFrameIndex(handle);
    }

    uint32_t rhiSwapchainGetImageCount(SwapchainHandle handle)
    {
        return GetRHIDevice()->swapchainGetImageCount(handle);
    }

} // namespace Surge::RHI
