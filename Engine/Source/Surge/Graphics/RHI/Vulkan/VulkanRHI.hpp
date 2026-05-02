// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Window/Window.hpp"
#include "VulkanDebugger.hpp"
#include <volk.h>
#include <vk_mem_alloc.h>

#define FORCE_VALIDATION 0
#if FORCE_VALIDATION == 1
	#define ENABLE_IF_VK_VALIDATION(x) { x; }
#else
	#ifdef SURGE_DEBUG
		#define ENABLE_IF_VK_VALIDATION(x) { x; }
	#else
		#define ENABLE_IF_VK_VALIDATION(x)
	#endif // SURGE_DEBUG
#endif

namespace Surge
{
	class VulkanRHI
	{
	public:
		VulkanRHI() = default;
		~VulkanRHI() = default;
		void Initialize(Window* window);
		void Shutdown();

		VkInstance GetInstance() const { return mInstance; }
		VkSurfaceKHR GetSurface() const { return mSurface; }
		VkDevice GetDevice() const { return mDevice; }
		VkPhysicalDevice GetGPU() const { return mGPU; }
		VkQueue GetQueue() const { return mQueue; }
		VmaAllocator GetAllocator() const { return mVmaAllocator; }
		int32_t GetQueueIndex() const { return mGraphicsQueueIndex; }

	private:
		void CreateInstance();
		void CreateSurface(Window* window);
		void CreateDevice();
	
		Vector<const char*> GetRequiredInstanceExtensions();
		Vector<const char*> GetRequiredInstanceLayers();

	private:
		VulkanDebugger mDebugger;
		VkInstance mInstance { VK_NULL_HANDLE };
		VkSurfaceKHR mSurface { VK_NULL_HANDLE };
		VkDevice mDevice { VK_NULL_HANDLE };
		VkPhysicalDevice mGPU = VK_NULL_HANDLE;
		VkQueue mQueue = VK_NULL_HANDLE;
		VmaAllocator mVmaAllocator = VK_NULL_HANDLE;

		int32_t mGraphicsQueueIndex = -1;
	};

}