// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RHI/Vulkan/VulkanDiagnostics.hpp"

namespace Surge
{
    static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                              const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
    {
        switch (messageSeverity)
        {
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
                Log("[VulkanDiagnostics]");
                Log<Severity::Warn>("{0}", pCallbackData->pMessage);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
                Log("[VulkanDiagnostics]");
                Log<Severity::Error>("{0}", pCallbackData->pMessage);
                break;
        }
        return VK_FALSE;
    }

    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator,
                                          VkDebugUtilsMessengerEXT* pDebugMessenger)
    {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr)
            return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
        else
            return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
    {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr)
            func(instance, debugMessenger, pAllocator);
    }

    void VulkanDiagnostics::Create(VkInstanceCreateInfo& vkInstanceCreateInfo)
    {
        PopulateDebugCreateInfo(mDebugCreateInfo);
        vkInstanceCreateInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&mDebugCreateInfo;
    }

    void VulkanDiagnostics::AddValidationLayers(Vector<const char*>& outInstanceLayers)
    {
        const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
        uint32_t instanceLayerCount;
        VK_CALL(vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr));
        Vector<VkLayerProperties> instanceLayerProperties(instanceLayerCount);
        VK_CALL(vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties.data()));

        bool validationLayerPresent = false;
        for (const VkLayerProperties& layer : instanceLayerProperties)
        {
            if (strcmp(layer.layerName, validationLayerName) == 0)
            {
                validationLayerPresent = true;
                outInstanceLayers.push_back(validationLayerName);
                break;
            }
        }

        if (!validationLayerPresent)
            Log<Severity::Error>("Validation layer requested, but it is not present ({0}), validation is disabled", validationLayerName);
        else
            Log<Severity::Info>("Vulkan Validation layer enabled");
    }

    void VulkanDiagnostics::StartDiagnostics(VkInstance& instance)
    {
        VK_CALL(CreateDebugUtilsMessengerEXT(instance, &mDebugCreateInfo, nullptr, &mDebugMessenger));
    }

    void VulkanDiagnostics::EndDiagnostics(VkInstance& instance) { DestroyDebugUtilsMessengerEXT(instance, mDebugMessenger, nullptr); }

    void VulkanDiagnostics::PopulateDebugCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& debugCreateInfo)
    {
        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = VulkanDebugCallback;
        debugCreateInfo.pUserData = nullptr;
    }
} // namespace Surge
