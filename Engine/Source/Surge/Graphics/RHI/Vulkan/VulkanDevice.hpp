// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include <volk.h>
#include <vk_mem_alloc.h>

namespace Surge
{
	struct GPUMemoryStats
	{
		std::atomic<uint64_t> AllocatedBytes{ 0 };
		std::atomic<uint64_t> AllocationCount{ 0 };
		uint64_t BudgetBytes = 0;
	};

	class VulkanDevice
	{
	public:
		VulkanDevice() = default;
		~VulkanDevice() = default;
		void Initialize(VkInstance instance, VkSurfaceKHR surface);
		void Destroy();

		VkDevice GetDevice() const { return mDevice; }
		VkPhysicalDevice GetGPU() const { return mGPU; }
		const GPUMemoryStats& GetMemoryStats() const { return mGPUMemoryStats; }

		// TODO: This is temporary until we support multiple queues and move queues to a separate class/file
		VkQueue GetQueue() const { return mQueue; }
		VmaAllocator GetAllocator() const { return mVmaAllocator; }
		int32_t GetQueueIndex() const { return mGraphicsQueueIndex; }
		
		operator VkDevice() const { return mDevice; }
	private:
		void QueryBudget();
	private:

		VkDevice mDevice = VK_NULL_HANDLE ;
		VkPhysicalDevice mGPU = VK_NULL_HANDLE;
		VkQueue mQueue = VK_NULL_HANDLE;
		int32_t mGraphicsQueueIndex = -1;

		VmaAllocator mVmaAllocator = VK_NULL_HANDLE;
		GPUMemoryStats mGPUMemoryStats;
		VkPhysicalDeviceMemoryProperties mMemProps;
	};

}
