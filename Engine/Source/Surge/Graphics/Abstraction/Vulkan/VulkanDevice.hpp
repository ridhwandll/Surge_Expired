// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/Abstraction/Vulkan/VulkanDiagnostics.hpp"
#include <unordered_set>
#include <volk.h>
#include "Surge/Core/String.hpp"

namespace Surge
{
    struct VulkanQueueFamilyIndices
    {
        int32_t GraphicsQueue = -1;
        int32_t ComputeQueue = -1;
        int32_t TransferQueue = -1;
    };

    enum class SURGE_API VulkanQueueType
    {
        Graphics = 0,
        Compute,
        Transfer
    };

    class SURGE_API VulkanDevice
    {
    public:
        VulkanDevice() = default;
        ~VulkanDevice() = default;

        void Initialize(VkInstance instance);
        void Destroy();

        VkPhysicalDevice GetPhysicalDevice() const { return mPhysicalDevice; }
        int32_t GetDeviceScore() const { return mDeviceScore; }
        VkDevice GetLogicalDevice() const { return mLogicalDevice; }
        VulkanQueueFamilyIndices GetQueueFamilyIndices() const { return mQueueFamilyIndices; }
        auto GetProperties() const { return mProperties; }
        VkQueue GetGraphicsQueue() const { return mGraphicsQueue; }
        VkQueue GetComputeQueue() const { return mComputeQueue; }
        VkQueue GetTransferQueue() const { return mTransferQueue; }

        VkCommandPool GetGraphicsCommandPool() const { return mGraphicsCommandPool; }
        VkCommandPool GetComputeCommandPool() const { return mComputeCommandPool; }
        VkCommandPool GetTransferCommandPool() const { return mTransferCommandPool; }
        VkPhysicalDeviceProperties GetPhysicalDeviceProperties() const
        {
            VkPhysicalDeviceProperties properties {};
            vkGetPhysicalDeviceProperties(mPhysicalDevice, &properties);
            return properties;
        }

        void InstantSubmit(VulkanQueueType type, std::function<void(VkCommandBuffer&)> function);
        bool IsExtensionSupported(const String& extensionName) { return mSupportedExtensions.find(extensionName) != mSupportedExtensions.end(); };

    private:
        void QueryDeviceExtensions();
        void QueryPhysicalDeviceFeatures();
        void QueryPhysicalDeviceProperties();
        void FillQueueFamilyIndicesAndStructures(int flags, VulkanQueueFamilyIndices& outQueueFamilyIndices, Vector<VkDeviceQueueCreateInfo>& outQueueInfo);
        void CreateCommandPools();
        int32_t RatePhysicalDevice(VkPhysicalDevice physicalDevice);

    private:
        VulkanQueueFamilyIndices mQueueFamilyIndices;
        Vector<VkQueueFamilyProperties> mQueueFamilyProperties;

        // PhysicalDevice
        VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
        int32_t mDeviceScore;

        // LogicalDevice
        VkDevice mLogicalDevice;
        std::unordered_set<String> mSupportedExtensions;

        VkQueue mGraphicsQueue;
        VkQueue mComputeQueue;
        VkQueue mTransferQueue;

        VkCommandPool mGraphicsCommandPool;
        VkCommandPool mComputeCommandPool;
        VkCommandPool mTransferCommandPool;

    public:
        struct VkFeatures
        {
            VkPhysicalDeviceFeatures2 vk10Features {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
            VkPhysicalDeviceVulkan11Features vk11Features {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
            VkPhysicalDeviceVulkan12Features vk12Features {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
            VkPhysicalDeviceSynchronization2FeaturesKHR sync2Features {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR};
        } mFeatures {};

        struct VkProperties
        {
            VkPhysicalDeviceProperties2 vk10Properties {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
            VkPhysicalDeviceVulkan11Properties vk11Properties {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES};
            VkPhysicalDeviceVulkan12Properties vk12Properties {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES};
        } mProperties {};
    };
} // namespace Surge
