// Copyright (c) - SurgeTechnologies - All rights reserved
#include "VulkanDevice.hpp"
#include "VulkanDebugger.hpp"
#include "Surge/Core/Logger/Logger.hpp"

namespace Surge
{
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

		if (!ValidateExtensions(requiredDeviceExtensions, deviceExtensions))
			throw std::runtime_error("Required device extensions are missing!");

		// Uses a single graphics queue for now, but a more robust implementation would use separate queues for graphics, compute, and transfer operations if available
		const float queuePriority = 0.5f;
		VkDeviceQueueCreateInfo queueInfo{
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = static_cast<uint32_t>(mGraphicsQueueIndex),
			.queueCount = 1,
			.pQueuePriorities = &queuePriority };

		VkDeviceCreateInfo deviceInfo{
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.queueCreateInfoCount = 1,
			.pQueueCreateInfos = &queueInfo,
			.enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions.size()),
			.ppEnabledExtensionNames = requiredDeviceExtensions.data() };

		VK_CALL(vkCreateDevice(mGPU, &deviceInfo, nullptr, &mDevice));
		volkLoadDevice(mDevice);

		// Get the graphics queue
		// TODO: Move queue to a separate file and support multiple queues (graphics, compute, transfer)
		vkGetDeviceQueue(mDevice, mGraphicsQueueIndex, 0, &mQueue);

		//Vulkan Memory Alloctor (VMA)
		VmaVulkanFunctions vmaVulkanFunc = {}; // Zero initialize everything else!
		vmaVulkanFunc.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
		vmaVulkanFunc.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

		VmaAllocatorCreateInfo allocatorInfo = {};
		allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_1;
		allocatorInfo.physicalDevice = mGPU;
		allocatorInfo.device = mDevice;
		allocatorInfo.instance = instance;
		allocatorInfo.pVulkanFunctions = &vmaVulkanFunc;

		VK_CALL(vmaCreateAllocator(&allocatorInfo, &mVmaAllocator));
	}
	
	void VulkanDevice::Destroy()
	{
		vkDeviceWaitIdle(mDevice);
		vmaDestroyAllocator(mVmaAllocator);
		vkDestroyDevice(mDevice, nullptr);
	}
}
