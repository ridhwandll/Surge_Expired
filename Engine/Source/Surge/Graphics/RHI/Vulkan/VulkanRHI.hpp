// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Window/Window.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include "Surge/Graphics/RHI/RHIDescriptors.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanDevice.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanDebugger.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanResourcePools.hpp"

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

		void CmdBindVertexBuffer(VkCommandBuffer cmdBuffer, BufferHandle h, Uint offset = 0);
		void CmdBindIndexBuffer(VkCommandBuffer cmdBuffer, BufferHandle h, Uint offset = 0);

		BufferHandle CreateBuffer(const BufferDesc& desc);		
		void UploadBuffer(BufferHandle h, const void* data, Uint size, Uint offset);
		void DestroyBuffer(BufferHandle buffer);

		TextureHandle CreateTexture(const TextureDesc& desc, const void* initialData = nullptr);
		void DestroyTexture(TextureHandle texture);

		FramebufferHandle CreateFramebuffer(const FramebufferDesc& desc, const void* initialData = nullptr);
		void DestroyFramebuffer(FramebufferHandle framebuffer);

		void SetDebugName(const VkDebugUtilsObjectNameInfoEXT& nameInfo) const { mDebugger.SetDebugName(*this, nameInfo); }

		VkInstance GetInstance() const { return mInstance; }
		VkSurfaceKHR GetSurface() const { return mSurface; }
		VkDevice GetDevice() const { return mDevice.GetDevice(); }
		VkPhysicalDevice GetGPU() const { return mDevice.GetGPU(); }
		VkQueue GetQueue() const { return mDevice.GetQueue(); }
		VmaAllocator GetAllocator() const { return mDevice.GetAllocator(); }
		int32_t GetQueueIndex() const { return mDevice.GetQueueIndex(); }

	private:
		void CreateInstance();
		void CreateSurface(Window* window);
	
		Vector<const char*> GetRequiredInstanceExtensions();
		Vector<const char*> GetRequiredInstanceLayers();

	private:
		VulkanDevice mDevice;
		VulkanDebugger mDebugger;

		VkInstance mInstance { VK_NULL_HANDLE };
		VkSurfaceKHR mSurface { VK_NULL_HANDLE };

		//Pools
		HandlePool<TextureHandle, TextureEntry> mTexturePool;
		HandlePool<BufferHandle, BufferEntry> mBufferPool;
		HandlePool<FramebufferHandle, FramebufferEntry> mFramebufferPool;
	};

}