// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Abstraction/Vulkan/VulkanDevice.hpp"
#include <map>

namespace Surge
{
    void VulkanDevice::Initialize(VkInstance instance)
    {
        Uint deviceCount = 0;
        VK_CALL(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr));
        Vector<VkPhysicalDevice> physicalDevices(deviceCount);
        VK_CALL(vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data()));

        std::multimap<int, VkPhysicalDevice> candidates;
        for (const auto& device : physicalDevices)
        {
            int32_t score = RatePhysicalDevice(device);
            candidates.insert(std::make_pair(score, device));
        }

        if (candidates.rbegin()->first > 0)
        {
            mPhysicalDevice = candidates.rbegin()->second;
            QueryPhysicalDeviceProperties();
            QueryPhysicalDeviceFeatures();
            mDeviceScore = candidates.rbegin()->first;
        }
        else
            SG_ASSERT_INTERNAL("No discrete Graphics Processing Unit(GPU) found!");

        QueryDeviceExtensions();

        /// Queue families ///
        Uint queueFamilyCount;
        vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueFamilyCount, nullptr);
        mQueueFamilyProperties.resize(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueFamilyCount, mQueueFamilyProperties.data());

        Vector<VkDeviceQueueCreateInfo> queueCreateInfos {};
        int requiredQueueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
        FillQueueFamilyIndicesAndStructures(requiredQueueFlags, mQueueFamilyIndices, queueCreateInfos);

        /// Logical Device ///
        Vector<const char*> deviceExtensions;
        deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

        VkPhysicalDeviceFeatures enabledFeatures {};

        VkDeviceCreateInfo deviceCreateInfo {};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pEnabledFeatures = nullptr;
        deviceCreateInfo.queueCreateInfoCount = static_cast<Uint>(queueCreateInfos.size());
        ;
        deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
        deviceCreateInfo.pNext = &mFeatures;

        if (!deviceExtensions.empty())
        {
            deviceCreateInfo.enabledExtensionCount = static_cast<Uint>(deviceExtensions.size());
            deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
        }
        VK_CALL(vkCreateDevice(mPhysicalDevice, &deviceCreateInfo, nullptr, &mLogicalDevice));
        volkLoadDevice(mLogicalDevice);

        CreateCommandPools();

        vkGetDeviceQueue(mLogicalDevice, mQueueFamilyIndices.GraphicsQueue, 0, &mGraphicsQueue);
        vkGetDeviceQueue(mLogicalDevice, mQueueFamilyIndices.ComputeQueue, 0, &mComputeQueue);
        vkGetDeviceQueue(mLogicalDevice, mQueueFamilyIndices.TransferQueue, 0, &mTransferQueue);
    }

    void VulkanDevice::Destroy()
    {
        vkDestroyCommandPool(mLogicalDevice, mGraphicsCommandPool, nullptr);
        vkDestroyCommandPool(mLogicalDevice, mComputeCommandPool, nullptr);
        vkDestroyCommandPool(mLogicalDevice, mTransferCommandPool, nullptr);

        vkDeviceWaitIdle(mLogicalDevice);
        vkDestroyDevice(mLogicalDevice, nullptr);
    }

    void VulkanDevice::InstantSubmit(VulkanQueueType type, std::function<void(VkCommandBuffer&)> function)
    {
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo cmdBufAllocateInfo = {};
        cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

        switch (type)
        {
            case VulkanQueueType::Graphics: cmdBufAllocateInfo.commandPool = mGraphicsCommandPool; break;
            case VulkanQueueType::Compute: cmdBufAllocateInfo.commandPool = mComputeCommandPool; break;
            case VulkanQueueType::Transfer: cmdBufAllocateInfo.commandPool = mTransferCommandPool; break;
        }

        cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdBufAllocateInfo.commandBufferCount = 1;

        VK_CALL(vkAllocateCommandBuffers(mLogicalDevice, &cmdBufAllocateInfo, &commandBuffer));

        VkCommandBufferBeginInfo cmdBufferBeginInfo {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        VK_CALL(vkBeginCommandBuffer(commandBuffer, &cmdBufferBeginInfo));

        function(commandBuffer); // Execute

        const uint64_t fenceTimeout = 100000000000;

        SG_ASSERT_NOMSG(commandBuffer != VK_NULL_HANDLE);
        VK_CALL(vkEndCommandBuffer(commandBuffer));

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        // Create fence to ensure that the command buffer has finished executing
        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = 0;
        VkFence fence = VK_NULL_HANDLE;
        VK_CALL(vkCreateFence(mLogicalDevice, &fenceCreateInfo, nullptr, &fence));

        // Submit to the queue
        switch (type)
        {
            case VulkanQueueType::Graphics:
            {
                VK_CALL(vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, fence));
                break;
            }
            case VulkanQueueType::Compute:
            {
                VK_CALL(vkQueueSubmit(mComputeQueue, 1, &submitInfo, fence));
                break;
            }
            case VulkanQueueType::Transfer:
            {
                VK_CALL(vkQueueSubmit(mTransferQueue, 1, &submitInfo, fence));
                break;
            }
        }

        // Wait for the fence to signal that command buffer has finished executing
        VK_CALL(vkWaitForFences(mLogicalDevice, 1, &fence, VK_TRUE, fenceTimeout));

        switch (type)
        {
            case VulkanQueueType::Graphics:
            {
                vkFreeCommandBuffers(mLogicalDevice, mGraphicsCommandPool, 1, &commandBuffer);
                break;
            }
            case VulkanQueueType::Compute:
            {
                vkFreeCommandBuffers(mLogicalDevice, mComputeCommandPool, 1, &commandBuffer);
                break;
            }
            case VulkanQueueType::Transfer:
            {
                vkFreeCommandBuffers(mLogicalDevice, mTransferCommandPool, 1, &commandBuffer);
                break;
            }
        }

        vkDestroyFence(mLogicalDevice, fence, nullptr);
    }

    void VulkanDevice::QueryDeviceExtensions()
    {
        Uint extCount = 0;
        vkEnumerateDeviceExtensionProperties(mPhysicalDevice, nullptr, &extCount, nullptr);
        if (extCount > 0)
        {
            Vector<VkExtensionProperties> extensions(extCount);
            if (vkEnumerateDeviceExtensionProperties(mPhysicalDevice, nullptr, &extCount, &extensions.front()) == VK_SUCCESS)
            {
                Log<Severity::Trace>("Found {1} extensions on {0}", mProperties.vk10Properties.properties.deviceName, extensions.size());
                for (const VkExtensionProperties& ext : extensions)
                    mSupportedExtensions.emplace(ext.extensionName);
            }
        }
    }

    void VulkanDevice::QueryPhysicalDeviceProperties()
    {
        mProperties.vk10Properties.pNext = &mProperties.vk11Properties;
        mProperties.vk11Properties.pNext = &mProperties.vk12Properties;
        mProperties.vk12Properties.pNext = nullptr;
        vkGetPhysicalDeviceProperties2(mPhysicalDevice, &mProperties.vk10Properties);
    }

    void VulkanDevice::QueryPhysicalDeviceFeatures()
    {
        // Credit to: https://github.com/rtryan98/Yggdrasil
        mFeatures.vk10Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        mFeatures.vk10Features.pNext = &mFeatures.vk11Features;
        mFeatures.vk11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        mFeatures.vk11Features.pNext = &mFeatures.vk12Features;
        mFeatures.vk12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        mFeatures.vk12Features.pNext = &mFeatures.sync2Features;
        mFeatures.sync2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;
        mFeatures.sync2Features.pNext = nullptr;

        VkPhysicalDeviceFeatures requestedVulkan10Features {};
        requestedVulkan10Features.samplerAnisotropy = VK_TRUE;
        requestedVulkan10Features.multiDrawIndirect = VK_TRUE;
        requestedVulkan10Features.imageCubeArray = VK_TRUE;
        requestedVulkan10Features.pipelineStatisticsQuery = VK_TRUE;
        requestedVulkan10Features.wideLines = VK_TRUE;
        requestedVulkan10Features.fillModeNonSolid = VK_TRUE;

        VkPhysicalDeviceVulkan11Features requestedVulkan11Features {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
        requestedVulkan11Features.shaderDrawParameters = VK_TRUE;

        VkPhysicalDeviceVulkan12Features requestedVulkan12Features {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        requestedVulkan12Features.drawIndirectCount = VK_TRUE;
        requestedVulkan12Features.imagelessFramebuffer = VK_TRUE;
        requestedVulkan12Features.shaderInt8 = VK_TRUE;

        VkPhysicalDeviceSynchronization2FeaturesKHR requestedSync2Features {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR};
        requestedSync2Features.synchronization2 = VK_TRUE;

        VkPhysicalDeviceFeatures2 availableVulkan10Features {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        VkPhysicalDeviceVulkan11Features availableVulkan11Features {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
        VkPhysicalDeviceVulkan12Features availableVulkan12Features {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        VkPhysicalDeviceSynchronization2FeaturesKHR availableSync2Features {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR};

        availableVulkan10Features.pNext = &availableVulkan11Features;
        availableVulkan11Features.pNext = &availableVulkan12Features;
        availableVulkan12Features.pNext = &availableSync2Features;

        vkGetPhysicalDeviceFeatures2(mPhysicalDevice, &availableVulkan10Features);

        VkBool32* requested = nullptr;
        VkBool32* available = nullptr;

        requested = &requestedVulkan10Features.robustBufferAccess;
        available = &availableVulkan10Features.features.robustBufferAccess;
        for (Uint i = 0; i < (sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32)); i++)
        {
            if (requested[i] && available[i])
            {
                VkBool32* feature {(&mFeatures.vk10Features.features.robustBufferAccess) + i};
                *feature = VK_TRUE;
            }
        }

        requested = &requestedVulkan11Features.storageBuffer16BitAccess;
        available = &availableVulkan11Features.storageBuffer16BitAccess;
        for (Uint i = 0; i < ((sizeof(VkPhysicalDeviceVulkan11Features) - sizeof(void*) - sizeof(VkStructureType)) / sizeof(VkBool32)) - 1; i++)
        {
            if (requested[i] && available[i])
            {
                VkBool32* feature {(&mFeatures.vk11Features.storageBuffer16BitAccess) + i};
                *feature = VK_TRUE;
            }
        }

        requested = &requestedVulkan12Features.samplerMirrorClampToEdge;
        available = &availableVulkan12Features.samplerMirrorClampToEdge;
        for (Uint i = 0; i < ((sizeof(VkPhysicalDeviceVulkan12Features) - sizeof(void*) - sizeof(VkStructureType)) / sizeof(VkBool32)) - 1; i++)
        {
            if (requested[i] && available[i])
            {
                VkBool32* feature {(&mFeatures.vk12Features.samplerMirrorClampToEdge) + i};
                *feature = VK_TRUE;
            }
        }
        if (requestedSync2Features.synchronization2 && availableSync2Features.synchronization2)
        {
            mFeatures.sync2Features.synchronization2 = VK_TRUE;
        }
    }

    void VulkanDevice::FillQueueFamilyIndicesAndStructures(int flags, VulkanQueueFamilyIndices& outQueueFamilyIndices, Vector<VkDeviceQueueCreateInfo>& outQueueInfo)
    {
        // Find a dedicated queue for compute queue, which doesn't have graphics in it
        if (flags & VK_QUEUE_COMPUTE_BIT)
        {
            for (Uint i = 0; i < mQueueFamilyProperties.size(); i++)
            {
                VkQueueFamilyProperties& queueFamilyProperties = mQueueFamilyProperties[i];
                if ((queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT) && ((queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0))
                {
                    outQueueFamilyIndices.ComputeQueue = i;
                    break;
                }
            }
        }

        // Find a dedicated queue for transfer queue, which doesn't have graphics/compute in it
        if (flags & VK_QUEUE_TRANSFER_BIT)
        {
            for (Uint i = 0; i < mQueueFamilyProperties.size(); i++)
            {
                auto& queueFamilyProperties = mQueueFamilyProperties[i];
                if ((queueFamilyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT) && ((queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) &&
                    ((queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
                {
                    outQueueFamilyIndices.TransferQueue = i;
                    break;
                }
            }
        }

        // For other queue types or if no separate compute queue is present, return the first one to support the
        // requested flags
        for (Uint i = 0; i < mQueueFamilyProperties.size(); i++)
        {
            if ((flags & VK_QUEUE_TRANSFER_BIT) && outQueueFamilyIndices.TransferQueue == -1)
            {
                if (mQueueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
                    outQueueFamilyIndices.TransferQueue = i;
            }

            if ((flags & VK_QUEUE_COMPUTE_BIT) && outQueueFamilyIndices.ComputeQueue == -1)
            {
                if (mQueueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
                    outQueueFamilyIndices.ComputeQueue = i;
            }

            // Fill the Graphics queue
            if (flags & VK_QUEUE_GRAPHICS_BIT)
            {
                if (mQueueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                    outQueueFamilyIndices.GraphicsQueue = i;
            }
        }

        const float defaultQueuePriority = 1.0f;

        // Graphics queue
        if (flags & VK_QUEUE_GRAPHICS_BIT)
        {
            VkDeviceQueueCreateInfo queueInfo {};
            queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueInfo.queueFamilyIndex = mQueueFamilyIndices.GraphicsQueue;
            queueInfo.queueCount = 1;
            queueInfo.pQueuePriorities = &defaultQueuePriority;
            outQueueInfo.push_back(queueInfo);
        }

        // Dedicated compute queue
        if (flags & VK_QUEUE_COMPUTE_BIT)
        {
            if (mQueueFamilyIndices.ComputeQueue != mQueueFamilyIndices.GraphicsQueue)
            {
                VkDeviceQueueCreateInfo queueInfo {};
                queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueInfo.queueFamilyIndex = mQueueFamilyIndices.ComputeQueue;
                queueInfo.queueCount = 1;
                queueInfo.pQueuePriorities = &defaultQueuePriority;
                outQueueInfo.push_back(queueInfo);
            }
        }

        // Dedicated transfer queue
        if (flags & VK_QUEUE_TRANSFER_BIT)
        {
            if ((mQueueFamilyIndices.TransferQueue != mQueueFamilyIndices.GraphicsQueue) && (mQueueFamilyIndices.TransferQueue != mQueueFamilyIndices.ComputeQueue))
            {
                VkDeviceQueueCreateInfo queueInfo {};
                queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueInfo.queueFamilyIndex = mQueueFamilyIndices.TransferQueue;
                queueInfo.queueCount = 1;
                queueInfo.pQueuePriorities = &defaultQueuePriority;
                outQueueInfo.push_back(queueInfo);
            }
        }
    }

    void VulkanDevice::CreateCommandPools()
    {
        VkCommandPoolCreateInfo cmdPoolInfo = {};
        cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmdPoolInfo.queueFamilyIndex = mQueueFamilyIndices.GraphicsQueue;
        cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        VK_CALL(vkCreateCommandPool(mLogicalDevice, &cmdPoolInfo, nullptr, &mGraphicsCommandPool));

        cmdPoolInfo.queueFamilyIndex = mQueueFamilyIndices.ComputeQueue;
        VK_CALL(vkCreateCommandPool(mLogicalDevice, &cmdPoolInfo, nullptr, &mComputeCommandPool));

        cmdPoolInfo.queueFamilyIndex = mQueueFamilyIndices.TransferQueue;
        VK_CALL(vkCreateCommandPool(mLogicalDevice, &cmdPoolInfo, nullptr, &mTransferCommandPool));
    }

    int32_t VulkanDevice::RatePhysicalDevice(VkPhysicalDevice physicalDevice)
    {
        VkPhysicalDeviceFeatures features {};
        VkPhysicalDeviceProperties properties {};
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        vkGetPhysicalDeviceFeatures(physicalDevice, &features);
        int32_t score = 0;

        if (!features.geometryShader)
        {
#ifndef SURGE_ANDROID
            // Mobile GPUs commonly lack geometry shaders; skip this requirement on Android
            return 0;
#endif
        }

        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            score += 250;
        else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
            score += 100;
        else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)
            score += 50;

        Uint extCount = 0;
        Uint layerCount = 0;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
        vkEnumerateDeviceLayerProperties(physicalDevice, &layerCount, nullptr);

        // Bonus
        score += properties.limits.maxColorAttachments;
        score += properties.limits.framebufferColorSampleCounts;
        score += properties.limits.framebufferDepthSampleCounts;
        score += properties.limits.maxClipDistances;
        score += properties.limits.maxBoundDescriptorSets;
        score += properties.limits.maxPushConstantsSize;
        score += extCount;
        score += layerCount;

        return score;
    }

} // namespace Surge
