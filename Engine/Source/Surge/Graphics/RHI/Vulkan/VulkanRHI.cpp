// Copyright (c) - SurgeTechnologies - All rights reserved
#include "VulkanRHI.hpp"
#include "VulkanDebugger.hpp"
#include "Surge/Core/Logger/Logger.hpp"


#ifdef SURGE_PLATFORM_ANDROID
#include <android/native_window.h>
#endif

namespace Surge
{
	void VulkanRHI::Initialize(Window* window)
	{
		CreateInstance();
		CreateSurface(window);
		CreateDevice();
	}

	void VulkanRHI::Shutdown()
	{		
		vkDeviceWaitIdle(mDevice); // When destroying the application, we need to make sure the GPU is no longer accessing any resources

		ENABLE_IF_VK_VALIDATION(mDebugger.EndDiagnostics(mInstance));
		vmaDestroyAllocator(mVmaAllocator);
		vkDestroyDevice(mDevice, nullptr);
		vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
		vkDestroyInstance(mInstance, nullptr);
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

	void VulkanRHI::CreateInstance()
	{
		Log<Severity::Info>("Initializing vulkan instance....");

		VK_CALL(volkInitialize());

		Vector<const char*> requiredInstanceExtensions = GetRequiredInstanceExtensions();
		Vector<const char*> requestedInstanceLayers = GetRequiredInstanceLayers();

		VkApplicationInfo app{
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pApplicationName = "SurgePlayer",
			.pEngineName = "SurgeEngine",
			.apiVersion = VK_API_VERSION_1_1 };

		VkInstanceCreateInfo instanceinfo{
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pApplicationInfo = &app,
			.enabledLayerCount = static_cast<Uint>(requestedInstanceLayers.size()),
			.ppEnabledLayerNames = requestedInstanceLayers.data(),
			.enabledExtensionCount = static_cast<Uint>(requiredInstanceExtensions.size()),
			.ppEnabledExtensionNames = requiredInstanceExtensions.data() };

		ENABLE_IF_VK_VALIDATION(mDebugger.Create(instanceinfo));

		// Create the Vulkan instance
		VK_CALL(vkCreateInstance(&instanceinfo, nullptr, &mInstance));
		volkLoadInstance(mInstance);

		ENABLE_IF_VK_VALIDATION(mDebugger.StartDiagnostics(mInstance));
	}

	Vector<const char*> VulkanRHI::GetRequiredInstanceExtensions()
	{
		Vector<const char*> requiredInstanceExtensions;
		requiredInstanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(SURGE_PLATFORM_ANDROID)
		requiredInstanceExtensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(SURGE_PLATFORM_WINDOWS)
		requiredInstanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(SURGE_PLATFORM_APPLE)
		requiredInstanceExtensions.push_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
#else
#pragma error Platform not supported
#endif
		ENABLE_IF_VK_VALIDATION(mDebugger.AddValidationExtensions(requiredInstanceExtensions));

		Uint instanceExtensionCount = 0;
		VK_CALL(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr));
		Vector<VkExtensionProperties> availableInstanceExtensions(instanceExtensionCount);
		VK_CALL(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, availableInstanceExtensions.data()));
		if (!ValidateExtensions(requiredInstanceExtensions, availableInstanceExtensions))
		{
			throw std::runtime_error("Required instance extensions are missing!");
		}

		return requiredInstanceExtensions;
	}

	Vector<const char*> VulkanRHI::GetRequiredInstanceLayers()
	{
		Vector<const char*> instanceLayers;
		ENABLE_IF_VK_VALIDATION(mDebugger.AddValidationLayers(instanceLayers));
		return instanceLayers;
	}

	void VulkanRHI::CreateSurface(Window* window)
	{
#ifdef SURGE_PLATFORM_WINDOWS
		PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR;

		// Getting the vkCreateWin32SurfaceKHR function pointer and assert if it doesnt exist
		vkCreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(mInstance, "vkCreateWin32SurfaceKHR");		
		SG_ASSERT(vkCreateWin32SurfaceKHR, "[Win32] Vulkan instance missing VK_KHR_win32_surface extension");

		VkWin32SurfaceCreateInfoKHR sci;
		memset(&sci, 0, sizeof(sci)); // Clear the info
		sci.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		sci.hinstance = GetModuleHandle(nullptr);
		sci.hwnd = static_cast<HWND>(window->GetNativeWindowHandle());

		vkCreateWin32SurfaceKHR(mInstance, &sci, nullptr, &mSurface);
#elif defined(SURGE_PLATFORM_ANDROID)
		PFN_vkCreateAndroidSurfaceKHR vkCreateAndroidSurfaceKHR = (PFN_vkCreateAndroidSurfaceKHR)vkGetInstanceProcAddr(mInstance, "vkCreateAndroidSurfaceKHR");
		if (!vkCreateAndroidSurfaceKHR)
			SG_ASSERT_INTERNAL("[Android] Vulkan instance missing VK_KHR_android_surface extension");

		VkAndroidSurfaceCreateInfoKHR sci{};
		memset(&sci, 0, sizeof(sci)); // Clear the info
		sci.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
		sci.window = static_cast<ANativeWindow*>(window->GetNativeWindowHandle());

		vkCreateAndroidSurfaceKHR(mInstance, &sci, nullptr, &mSurface);
#else
		SG_ASSERT_INTERNAL("Surge is currently Windows/Android Only! :(");
#endif
	}

	void VulkanRHI::CreateDevice()
	{
		Log<Severity::Info>("Initializing vulkan device...");

		Uint gpuCount = 0;
		VK_CALL(vkEnumeratePhysicalDevices(mInstance, &gpuCount, nullptr));		

		if (gpuCount < 1)
			throw std::runtime_error("No physical device found.");
		else
			Log<Severity::Info>("Found {0} GPU(s)", gpuCount);

		Vector<VkPhysicalDevice> gpus(gpuCount);
		VK_CALL(vkEnumeratePhysicalDevices(mInstance, &gpuCount, gpus.data()));


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
				vkGetPhysicalDeviceSurfaceSupportKHR(mGPU, i, mSurface, &supportsPresent);

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

		vkGetDeviceQueue(mDevice, mGraphicsQueueIndex, 0, &mQueue);

		//Vulkan Memory Alloctor (VMA)
		VmaVulkanFunctions vmaVulkanFunc{
			.vkGetInstanceProcAddr = vkGetInstanceProcAddr,
			.vkGetDeviceProcAddr = vkGetDeviceProcAddr };

		VmaAllocatorCreateInfo allocatorInfo{
			.physicalDevice = mGPU,
			.device = mDevice,
			.pVulkanFunctions = &vmaVulkanFunc,
			.instance = mInstance };

		VK_CALL(vmaCreateAllocator(&allocatorInfo, &mVmaAllocator));
	}
}
