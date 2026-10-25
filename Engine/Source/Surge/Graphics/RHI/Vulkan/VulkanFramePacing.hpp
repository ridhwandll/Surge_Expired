// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/String.hpp"
#include "Surge/Core/Vector.hpp"
#include <volk.h>

namespace Surge
{
    // Display synchronised frame pacing
    // Android: Swappy (Android Frame Pacing library), presents on vsync boundaries at a target rate (60/30 fps) instead of the
    // panel refresh rate, which keeps 90/120 Hz phones from rendering (and heating up) at 120 fps. A device too slow for the target
    // drops to the next steady rate (60 -> 30 on 60 Hz) instead of alternating frame lengths
    // Windows: no-op, presentation goes straight to vkQueuePresentKHR and Core's FrameCapper limits the rate
    namespace VulkanFramePacing
    {
        // Optional device extensions the pacer wants (e.g. VK_GOOGLE_display_timing), only ones present in 'available' are returned
        Vector<String> GetRequiredDeviceExtensions(VkPhysicalDevice gpu, const Vector<VkExtensionProperties>& available);

        void OnDeviceCreated(VkDevice device, VkQueue queue, Uint queueFamilyIndex);
        void OnDeviceDestroyed(VkDevice device);

        // Returns true if pacing is active for this swapchain
        bool OnSwapchainCreated(VkPhysicalDevice gpu, VkDevice device, VkSwapchainKHR swapchain, void* nativeWindow);
        void OnSwapchainDestroyed(VkDevice device, VkSwapchainKHR swapchain);

        // fps <= 0 presents at the display refresh rate
        void SetTargetFrameRate(VkDevice device, VkSwapchainKHR swapchain, float fps);
        bool IsActive();

        // false: frames bypass the pacer (plain vkQueuePresentKHR), so the GPU stays fully loaded and its clock is not lowered. For profiling
        void SetEnabled(bool enabled);

        VkResult QueuePresent(VkQueue queue, const VkPresentInfoKHR& presentInfo);
    }
}
