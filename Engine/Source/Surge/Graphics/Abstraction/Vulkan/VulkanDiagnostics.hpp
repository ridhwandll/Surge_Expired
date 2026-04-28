// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include <volk.h>
#include "Surge/Core/Defines.hpp"

#ifdef SURGE_PLATFORM_ANDROID
#include <android/log.h>
#endif

namespace Surge
{
    inline const char* VKResultToString(VkResult result)
    {
        switch (result)
        {
            case VK_SUCCESS: return "VK_SUCCESS";
            case VK_NOT_READY: return "VK_NOT_READY";
            case VK_TIMEOUT: return "VK_TIMEOUT";
            case VK_EVENT_SET: return "VK_EVENT_SET";
            case VK_EVENT_RESET: return "VK_EVENT_RESET";
            case VK_INCOMPLETE: return "VK_INCOMPLETE";
            case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
            case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
            case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
            case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
            case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
            case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
            case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
            case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
            case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
            case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
            case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
            case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
            case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
            case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
            case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
            case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
            case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
            case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
            case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
            case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
            case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
            case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
            case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
            case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
            case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
            case VK_ERROR_NOT_PERMITTED_EXT: return "VK_ERROR_NOT_PERMITTED_EXT";
            case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
            case VK_THREAD_IDLE_KHR: return "VK_THREAD_IDLE_KHR";
            case VK_THREAD_DONE_KHR: return "VK_THREAD_DONE_KHR";
            case VK_OPERATION_DEFERRED_KHR: return "VK_OPERATION_DEFERRED_KHR";
            case VK_OPERATION_NOT_DEFERRED_KHR: return "VK_OPERATION_NOT_DEFERRED_KHR";
            case VK_PIPELINE_COMPILE_REQUIRED_EXT: return "VK_PIPELINE_COMPILE_REQUIRED_EXT";
        default:
                return "Your mom";
        }
        return nullptr;
    }

    // NOTE(Rid):
    // Owned by Vulkan Context
    // Represents the "Debug Layer" of Vulkan
    class SURGE_API VulkanDiagnostics
    {
    public:
        void Create(VkInstanceCreateInfo& vkInstanceCreateInfo);
        void AddValidationLayers(Vector<const char*>& outInstanceLayers);
        FORCEINLINE void AddValidationExtensions(Vector<const char*>& outInstanceExtensions)
        {
            outInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        void StartDiagnostics(VkInstance& instance);
        void EndDiagnostics(VkInstance& instance);

    private:
        void PopulateDebugCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& debugCreateInfo);

    private:
        VkDebugUtilsMessengerEXT mDebugMessenger;
        VkDebugUtilsMessengerCreateInfoEXT mDebugCreateInfo {};
    };

} // namespace Surge

#ifdef SURGE_DEBUG
#define VK_CALL(res)                                                                      \
    if (res != VK_SUCCESS)                                                                \
    {                                                                                     \
        SG_ASSERT_INTERNAL("[Vulkan] Returned value: {0}", Surge::VKResultToString(res)); \
    }
#define VK_CHECK_WITHOUT_OUT_OF_DATE(res)                                                                                        \
    if (res != VK_SUCCESS && res != VK_ERROR_OUT_OF_DATE_KHR)                                                                    \
    {                                                                                                                            \
        SG_ASSERT_INTERNAL("[Vulkan] (VK_ERROR_OUT_OF_DATE_KHR not checked) Returned value: {0}", Surge::VKResultToString(res)); \
    }

#else
//#define VK_CALL(res) res
//#define VK_CHECK_WITHOUT_OUT_OF_DATE(res) res
#define VK_CALL(res)                                                                      \
    if (res != VK_SUCCESS)                                                                \
    {                                                                                     \
        ::Surge::Log<Surge::Severity::Fatal>(("[Vulkan] Returned value: {0}", Surge::VKResultToString(res))); \
        __android_log_assert("Vulkan Error", "SurgeEngine", "Returned value: %s", Surge::VKResultToString(res));\
    }
#define VK_CHECK_WITHOUT_OUT_OF_DATE(res)                                                                                        \
    if (res != VK_SUCCESS && res != VK_ERROR_OUT_OF_DATE_KHR)                                                                    \
    {                                                                                                                            \
        ::Surge::Log<Surge::Severity::Fatal>(("[Vulkan] (VK_ERROR_OUT_OF_DATE_KHR not checked) Returned value: {0}", Surge::VKResultToString(res))); \
        __android_log_assert("Vulkan Error", "SurgeEngine", "(VK_ERROR_OUT_OF_DATE_KHR not checked) Returned value: %s", Surge::VKResultToString(res));\
    }
#endif