// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RHI/Vulkan/VulkanFramePacing.hpp"
#include "Surge/Core/Logger/Logger.hpp"

#ifdef SURGE_PLATFORM_ANDROID
#include "Surge/Platform/Android/AndroidApp.hpp"
#include <android/native_window.h>
#include <swappy/swappyVk.h>
#endif

namespace Surge::VulkanFramePacing
{
#ifdef SURGE_PLATFORM_ANDROID
    static bool sActive = false;
    static bool sEnabled = true;
    static float sTargetFPS = 60.0f;
    static uint64_t sRefreshPeriodNS = 0;
    static constexpr uint64_t MAX_AUTO_SWAP_INTERVAL_NS = 50'000'000; // 20 fps
    static JNIEnv* sEnv = nullptr; // The android_main thread is not attached to the JVM by default, Swappy needs it for Choreographer/display queries

    Vector<String> GetRequiredDeviceExtensions(VkPhysicalDevice gpu, const Vector<VkExtensionProperties>& available)
    {
        VkExtensionProperties* availableData = const_cast<VkExtensionProperties*>(available.data());
        const Uint availableCount = static_cast<Uint>(available.size());

        Uint requiredCount = 0;
        SwappyVk_determineDeviceExtensions(gpu, availableCount, availableData, &requiredCount, nullptr);
        if(requiredCount == 0)
            return {};

        Vector<std::array<char, VK_MAX_EXTENSION_NAME_SIZE + 1>> storage(requiredCount);
        Vector<char*> names(requiredCount);
        for(Uint i = 0; i < requiredCount; i++)
            names[i] = storage[i].data();

        SwappyVk_determineDeviceExtensions(gpu, availableCount, availableData, &requiredCount, names.data());

        Vector<String> result;
        result.reserve(requiredCount);
        for(Uint i = 0; i < requiredCount; i++)
            result.emplace_back(names[i]);
        return result;
    }

    void OnDeviceCreated(VkDevice device, VkQueue queue, Uint queueFamilyIndex)
    {
        SwappyVk_setQueueFamilyIndex(device, queue, queueFamilyIndex);
    }

    void OnDeviceDestroyed(VkDevice device)
    {
        SwappyVk_destroyDevice(device);
        sActive = false;

        android_app* app = Android::GAndroidApp;
        if(sEnv && app && app->activity)
            app->activity->vm->DetachCurrentThread();
        sEnv = nullptr;
    }

    bool OnSwapchainCreated(VkPhysicalDevice gpu, VkDevice device, VkSwapchainKHR swapchain, void* nativeWindow)
    {
        android_app* app = Android::GAndroidApp;
        if(!app || !app->activity || !nativeWindow)
        {
            sActive = false;
            return false;
        }

        if(!sEnv && app->activity->vm->AttachCurrentThread(&sEnv, nullptr) != JNI_OK)
        {
            Log<Severity::Warn>("[FramePacing] Could not attach the render thread to the JVM, Swappy disabled");
            sEnv = nullptr;
            sActive = false;
            return false;
        }

        uint64_t refreshPeriod = 0;
        sActive = SwappyVk_initAndGetRefreshCycleDuration(sEnv, app->activity->javaGameActivity, gpu, device, swapchain, &refreshPeriod);
        if(!sActive)
        {
            Log<Severity::Warn>("[FramePacing] Swappy is unavailable on this device, falling back to the CPU frame capper");
            return false;
        }

        sRefreshPeriodNS = refreshPeriod;
        SwappyVk_setWindow(device, swapchain, static_cast<ANativeWindow*>(nativeWindow));
        SetTargetFrameRate(device, swapchain, sTargetFPS);
        Log<Severity::Info>("[FramePacing] Swappy active, display refresh period: {:.2f} ms, target: {} fps", refreshPeriod / 1.0e6, sTargetFPS);

        // A frame that misses the target would alternate 1 and 2 vsyncs (60/30 judder): Swappy lengthens the interval to the next
        // even step it can hold (30 fps on 60 Hz, 40 on 120 Hz) and returns to the target once frames fit again. Below 20 fps it stops pacing
        SwappyVk_setMaxAutoSwapIntervalNS(MAX_AUTO_SWAP_INTERVAL_NS);
        SwappyVk_setAutoSwapInterval(true);
        SwappyVk_setAutoPipelineMode(false);
        return true;
    }

    void OnSwapchainDestroyed(VkDevice device, VkSwapchainKHR swapchain)
    {
        if(swapchain != VK_NULL_HANDLE)
            SwappyVk_destroySwapchain(device, swapchain);
    }

    void SetTargetFrameRate(VkDevice device, VkSwapchainKHR swapchain, float fps)
    {
        sTargetFPS = fps;
        if(!sActive || swapchain == VK_NULL_HANDLE)
            return;

        const uint64_t intervalNS = fps > 0.0f ? static_cast<uint64_t>(1.0e9 / static_cast<double>(fps)) : sRefreshPeriodNS;
        SwappyVk_setSwapIntervalNS(device, swapchain, intervalNS);
    }

    bool IsActive()
    {
        return sActive;
    }

    void SetEnabled(bool enabled)
    {
        sEnabled = enabled;
    }

    VkResult QueuePresent(VkQueue queue, const VkPresentInfoKHR& presentInfo)
    {
        return sActive && sEnabled ? SwappyVk_queuePresent(queue, &presentInfo) : vkQueuePresentKHR(queue, &presentInfo);
    }

#else

    Vector<String> GetRequiredDeviceExtensions(VkPhysicalDevice, const Vector<VkExtensionProperties>&) { return {}; }
    void OnDeviceCreated(VkDevice, VkQueue, Uint) {}
    void OnDeviceDestroyed(VkDevice) {}
    bool OnSwapchainCreated(VkPhysicalDevice, VkDevice, VkSwapchainKHR, void*) { return false; }
    void OnSwapchainDestroyed(VkDevice, VkSwapchainKHR) {}
    void SetTargetFrameRate(VkDevice, VkSwapchainKHR, float) {}
    bool IsActive() { return false; }
    void SetEnabled(bool) {}
    VkResult QueuePresent(VkQueue queue, const VkPresentInfoKHR& presentInfo) { return vkQueuePresentKHR(queue, &presentInfo); }

#endif
}
