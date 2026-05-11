// Copyright (c) - SurgeTechnologies - All rights reserved
#include "VulkanDevice.hpp"
#include "VulkanDebugger.hpp"
#include "Surge/Core/Logger/Logger.hpp"

namespace Surge
{

	static void OnVmaAllocate(VmaAllocator allocator, Uint memoryType,VkDeviceMemory memory,VkDeviceSize size,void* userData)
	{
		GPUMemoryStats* stats = static_cast<GPUMemoryStats*>(userData);
		stats->AllocatedBytes.fetch_add(size, std::memory_order_relaxed);
		stats->AllocationCount.fetch_add(1, std::memory_order_relaxed);
	}

	static void OnVmaFree(VmaAllocator allocator, Uint memoryType, VkDeviceMemory memory, VkDeviceSize size,void* userData)
	{
		GPUMemoryStats* stats = static_cast<GPUMemoryStats*>(userData);
		stats->AllocatedBytes.fetch_sub(size, std::memory_order_relaxed);
		stats->AllocationCount.fetch_sub(1, std::memory_order_relaxed);
	}

	static bool ValidateExtensions(const Vector<const char*>& required, const Vector<VkExtensionProperties>& available)
	{
		for (auto extension : required)
		{
			bool found = false;
			for (auto& availableExtension : available)
			{
				if (strcmp(availableExtension.extensionName, extension) == 0)
				{
					found = true;
					break;
				}
			}

			if (!found)
			{
				return false;
			}
		}

		return true;
	}

	void VulkanDevice::Initialize(VkInstance instance, VkSurfaceKHR surface)
	{
		Log<Severity::Info>("Initializing Vulkan Device");

		Uint gpuCount = 0;
		VK_CALL(vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr));

		if (gpuCount < 1)
			throw std::runtime_error("No physical device found.");
		else
			Log<Severity::Info>("Found {0} GPU(s)", gpuCount);

		Vector<VkPhysicalDevice> gpus(gpuCount);
		VK_CALL(vkEnumeratePhysicalDevices(instance, &gpuCount, gpus.data()));


		// Just grab the first GPU with a queue that supports graphics and presentation. A more robust implementation would rate each GPU and select the best one...
		// TODO: Implement a more robust GPU selection algorithm that rates each GPU and selects the best one based on features, performance, etc.
		for (size_t i = 0; i < gpuCount && (mGraphicsQueueIndex < 0); i++)
		{
			mGPU = gpus[i];

			Uint queueFamilyCount;
			vkGetPhysicalDeviceQueueFamilyProperties(mGPU, &queueFamilyCount, nullptr);

			if (queueFamilyCount < 1)
			{
				throw std::runtime_error("No queue family found.");
			}

			Vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
			vkGetPhysicalDeviceQueueFamilyProperties(mGPU, &queueFamilyCount, queueFamilyProperties.data());

			for (Uint i = 0; i < queueFamilyCount; i++)
			{
				VkBool32 supportsPresent;
				vkGetPhysicalDeviceSurfaceSupportKHR(mGPU, i, surface, &supportsPresent);

				// Find a queue family which supports graphics and presentation
				if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && supportsPresent)
				{
					mGraphicsQueueIndex = i;
					break;
				}
			}
		}

		if (mGraphicsQueueIndex < 0)
			throw std::runtime_error("Did not find suitable device with a queue that supports graphics and presentation.");


		Uint deviceExtensionCount;
		VK_CALL(vkEnumerateDeviceExtensionProperties(mGPU, nullptr, &deviceExtensionCount, nullptr));
		Vector<VkExtensionProperties> deviceExtensions(deviceExtensionCount);
		VK_CALL(vkEnumerateDeviceExtensionProperties(mGPU, nullptr, &deviceExtensionCount, deviceExtensions.data()));
			
		Vector<const char*> requiredDeviceExtensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };

		VkDeviceCreateInfo deviceInfo{ .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };

		// Query and enable bindless features if supported
		VkPhysicalDeviceDescriptorIndexingFeaturesEXT bindlessFeatures = {};
		if (SetupBindlessVulkan(requiredDeviceExtensions, bindlessFeatures))
			deviceInfo.pNext = &bindlessFeatures;		
		else
			Log<Severity::Error>("Bindless Vulkan is not supported in this device! Graphics features are compromised!");		

		if (!ValidateExtensions(requiredDeviceExtensions, deviceExtensions))
			throw std::runtime_error("Required device extensions are missing!");

		// Uses a single graphics queue for now, but a more robust implementation would use separate queues for graphics, compute, and transfer operations if available
		const float queuePriority = 0.5f;
		VkDeviceQueueCreateInfo queueInfo{
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = static_cast<uint32_t>(mGraphicsQueueIndex),
			.queueCount = 1,
			.pQueuePriorities = &queuePriority };

		deviceInfo.queueCreateInfoCount = 1;
	    deviceInfo.pQueueCreateInfos = &queueInfo;
	    deviceInfo.enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions.size());
	    deviceInfo.ppEnabledExtensionNames = requiredDeviceExtensions.data();
		VK_CALL(vkCreateDevice(mGPU, &deviceInfo, nullptr, &mDevice));
		volkLoadDevice(mDevice);

		// Get the graphics queue
		// TODO: Move queue to a separate file and support multiple queues (graphics, compute, transfer)
		vkGetDeviceQueue(mDevice, mGraphicsQueueIndex, 0, &mQueue);

		//Vulkan Memory Alloctor (VMA)
		VmaDeviceMemoryCallbacks memCallbacks = {};
		memCallbacks.pfnAllocate = OnVmaAllocate;
		memCallbacks.pfnFree = OnVmaFree;
		memCallbacks.pUserData = &mGPUMemoryStats;

		VmaVulkanFunctions vmaVulkanFunc = {}; // Zero initialize everything else!
		vmaVulkanFunc.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
		vmaVulkanFunc.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

		VmaAllocatorCreateInfo allocatorInfo = {};
		allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_1;
		allocatorInfo.physicalDevice = mGPU;
		allocatorInfo.device = mDevice;
		allocatorInfo.instance = instance;
		allocatorInfo.pVulkanFunctions = &vmaVulkanFunc;
		allocatorInfo.pDeviceMemoryCallbacks = &memCallbacks;

		VK_CALL(vmaCreateAllocator(&allocatorInfo, &mVmaAllocator));

		QueryBudget();
	}

	void VulkanDevice::Destroy()
	{
		vkDeviceWaitIdle(mDevice);
		vmaDestroyAllocator(mVmaAllocator);
		vkDestroyDevice(mDevice, nullptr);
	}
	void VulkanDevice::QueryBudget()
	{
		vkGetPhysicalDeviceMemoryProperties(mGPU, &mMemProps);
		std::array<VmaBudget, VK_MAX_MEMORY_HEAPS> budgets = {};
		vmaGetHeapBudgets(mVmaAllocator, budgets.data());

		const auto& props = mMemProps;
		for (Uint i = 0; i < props.memoryHeapCount; i++)
		{
			bool isDeviceLocal = props.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;

			// On UMA mobile memoryHeapCount may be 1 with no DEVICE_LOCAL
			if (isDeviceLocal || props.memoryHeapCount == 1)
			{
				mGPUMemoryStats.BudgetBytes += budgets[i].budget;
			}
		}
	}

	bool VulkanDevice::SetupBindlessVulkan(Vector<const char*>& outExtensions, VkPhysicalDeviceDescriptorIndexingFeaturesEXT& outIndexingFeatures)
	{
		outIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
		outIndexingFeatures.pNext = nullptr;

		VkPhysicalDeviceFeatures2 features2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
		features2.pNext = &outIndexingFeatures;
		vkGetPhysicalDeviceFeatures2(mGPU, &features2);

		bool bindlessSupported = outIndexingFeatures.descriptorBindingPartiallyBound &&
			outIndexingFeatures.runtimeDescriptorArray &&
			outIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind;

		
		if (bindlessSupported)
		{
			// Essential extensions for bindless in 1.1
			outExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
			outExtensions.push_back(VK_KHR_MAINTENANCE3_EXTENSION_NAME); // Required dependency for indexing

			outIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;

			VkPhysicalDeviceDescriptorIndexingProperties indexingProps{};
			indexingProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES;
			indexingProps.pNext = nullptr;

			VkPhysicalDeviceProperties2 deviceProps{};
			deviceProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
			deviceProps.pNext = &indexingProps;
			vkGetPhysicalDeviceProperties2(mGPU, &deviceProps);

			uint32_t maxBindlessTextures = indexingProps.maxDescriptorSetUpdateAfterBindSampledImages;
			Log<Severity::Info>("Max Bindless Textures: {}", maxBindlessTextures);
		}

		return bindlessSupported;
	}
}
