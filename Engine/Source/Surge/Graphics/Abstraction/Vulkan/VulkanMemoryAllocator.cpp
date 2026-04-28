// Copyright (c) - SurgeTechnologies - All rights reserved
#define VMA_IMPLEMENTATION
#include "Surge/Graphics/Abstraction/Vulkan/VulkanMemoryAllocator.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanRenderContext.hpp"
#include "Surge/Graphics/RenderContext.hpp"
#include "VulkanDiagnostics.hpp"

namespace Surge
{
    void VulkanMemoryAllocator::Initialize(VkInstance instance, VulkanDevice& device)
    {
        VmaVulkanFunctions vulkanFunctions = {};
        vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;
        allocatorInfo.physicalDevice = device.GetPhysicalDevice();
        allocatorInfo.device = device.GetLogicalDevice();
        allocatorInfo.instance = instance;
        allocatorInfo.pVulkanFunctions = &vulkanFunctions;

        VK_CALL(vmaCreateAllocator(&allocatorInfo, &mAllocator));
    }

    void VulkanMemoryAllocator::Destroy() { vmaDestroyAllocator(mAllocator); }

    VmaAllocation VulkanMemoryAllocator::AllocateBuffer(VkBufferCreateInfo bufferCreateInfo, VmaMemoryUsage usage, VkBuffer& outBuffer, VmaAllocationInfo* allocationInfo)
    {
        VmaAllocationCreateInfo allocCreateInfo = {};
        allocCreateInfo.usage = usage;
        switch (usage)
        {
            case VMA_MEMORY_USAGE_GPU_ONLY:
                allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
                allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
                allocCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                break;

            case VMA_MEMORY_USAGE_CPU_ONLY:
            case VMA_MEMORY_USAGE_CPU_TO_GPU:
                allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
                allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
                break;

            case VMA_MEMORY_USAGE_GPU_TO_CPU:
                allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
                allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
                break;

            default:
                allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
                break;
        }

        if (allocationInfo)
            allocCreateInfo.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;


        VmaAllocation allocation;
        vmaCreateBuffer(mAllocator, &bufferCreateInfo, &allocCreateInfo, &outBuffer, &allocation, allocationInfo);

        return allocation;
    }

    void VulkanMemoryAllocator::DestroyBuffer(VkBuffer buffer, VmaAllocation allocation)
    {
        SG_ASSERT_NOMSG(buffer);
        SG_ASSERT_NOMSG(allocation);
        vmaDestroyBuffer(mAllocator, buffer, allocation);
    }

    VmaAllocation VulkanMemoryAllocator::AllocateImage(VkImageCreateInfo imageCreateInfo, VmaMemoryUsage usage, VkImage& outImage, VmaAllocationInfo* allocationInfo)
    {
        VmaAllocationCreateInfo allocCreateInfo = {};
        allocCreateInfo.usage = usage;
        allocCreateInfo.flags = allocationInfo ? VMA_ALLOCATION_CREATE_MAPPED_BIT : 0;

        VmaAllocation allocation;
        vmaCreateImage(mAllocator, &imageCreateInfo, &allocCreateInfo, &outImage, &allocation, allocationInfo);

        return allocation;
    }

    void VulkanMemoryAllocator::DestroyImage(VkImage image, VmaAllocation allocation)
    {
        SG_ASSERT_NOMSG(image);
        SG_ASSERT_NOMSG(allocation);
        vmaDestroyImage(mAllocator, image, allocation);
    }

    void VulkanMemoryAllocator::Free(VmaAllocation allocation) { vmaFreeMemory(mAllocator, allocation); }

    void* VulkanMemoryAllocator::MapMemory(VmaAllocation allocation)
    {
        void* mappedMemory;
        vmaMapMemory(mAllocator, allocation, (void**)&mappedMemory);
        return mappedMemory;
    }

    void VulkanMemoryAllocator::UnmapMemory(VmaAllocation allocation) { vmaUnmapMemory(mAllocator, allocation); }

    GPUMemoryStats VulkanMemoryAllocator::GetStats() const
    {
        VmaTotalStatistics stats;
        vmaCalculateStatistics(mAllocator, &stats);

        uint64_t usedMemory = stats.total.statistics.allocationBytes;
        uint64_t freeMemory = stats.total.statistics.blockBytes - stats.total.statistics.allocationBytes;

        return GPUMemoryStats(usedMemory, freeMemory);
    }

    VmaMemoryUsage SurgeMemoryUsageToVmaMemoryUsage(GPUMemoryUsage usage)
    {
    switch (usage)
    {
        case GPUMemoryUsage::Unknown:
            return VMA_MEMORY_USAGE_UNKNOWN;
        case GPUMemoryUsage::GPUOnly:
            return VMA_MEMORY_USAGE_GPU_ONLY;
        case GPUMemoryUsage::CPUOnly:
            return VMA_MEMORY_USAGE_CPU_ONLY;
        case GPUMemoryUsage::CPUToGPU:
            return VMA_MEMORY_USAGE_CPU_TO_GPU;
        case GPUMemoryUsage::GPUToCPU:
            return VMA_MEMORY_USAGE_GPU_TO_CPU;
        case GPUMemoryUsage::CPUCopy:
            return VMA_MEMORY_USAGE_CPU_COPY;
        case GPUMemoryUsage::GPULazilyAllocated:
            return VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED;
    }
    SG_ASSERT_INTERNAL("Invalid GPUMemoryUsage!");
    return VMA_MEMORY_USAGE_UNKNOWN;
    }


} // namespace Surge
