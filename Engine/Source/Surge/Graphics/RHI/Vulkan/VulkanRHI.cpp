// Copyright (c) - SurgeTechnologies - All rights reserved
#include "VulkanRHI.hpp"
#include "VulkanDebugger.hpp"
#include "VulkanBuffer.hpp"
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
		mDevice.Initialize(mInstance, mSurface);
	}

	void VulkanRHI::Shutdown()
	{		
		vkDeviceWaitIdle(mDevice.GetDevice()); // When destroying the application, we need to make sure the GPU is no longer accessing any resources

		// Clean up buffers if forgotten to be destroyed manually
		mBufferPool.ForEachAlive([&](const BufferHandle& h, BufferEntry& entry)
		{
			VulkanBuffer::Destroy(*this, entry);
			SG_ASSERT_INTERNAL("You forgot to destroy a buffer manually!");
		});	
			
		ENABLE_IF_VK_VALIDATION(mDebugger.EndDiagnostics(mInstance));
		mDevice.Destroy();
		vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
		vkDestroyInstance(mInstance, nullptr);
	}

	void VulkanRHI::CmdBindVertexBuffer(VkCommandBuffer cmdBuffer, BufferHandle h, Uint offset /*= 0*/)
	{
		const BufferEntry* entry = mBufferPool.Get(h);
		SG_ASSERT(entry != nullptr, "CmdBindVertexBuffer: Invalid BufferHandle");
		SG_ASSERT(entry->Buffer != VK_NULL_HANDLE, "CmdBindVertexBuffer: null VkBuffer");

		VkDeviceSize vkOffset = offset;
		vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &entry->Buffer, &vkOffset);
	}

	void VulkanRHI::CmdBindIndexBuffer(VkCommandBuffer cmdBuffer, BufferHandle h, Uint offset /*= 0*/)
	{
		const BufferEntry* entry = mBufferPool.Get(h);
		SG_ASSERT(entry != nullptr, "CmdBindIndexBuffer: Invalid BufferHandle");

		// Index type is stored in the entry — set at creation time
		vkCmdBindIndexBuffer(cmdBuffer, entry->Buffer, offset, VK_INDEX_TYPE_UINT32);
	}

	BufferHandle VulkanRHI::CreateBuffer(const BufferDesc& desc)
	{
		Log<Severity::Trace>("VulkanRHI::CreateBuffer:\n Name: {0}\n Size {1} bytes\n", desc.DebugName ? desc.DebugName : "Unnamed", desc.Size);
		BufferEntry entry = VulkanBuffer::Create(*this, desc);
		return mBufferPool.Allocate(std::move(entry));
	}

	TextureHandle VulkanRHI::CreateTexture(const TextureDesc& desc, const void* initialData /*= nullptr*/)
	{
		Log<Severity::Trace>("VulkanRHI::CreateTexture of size {0}x{1} with format {2} and usage {3}", desc.Width, desc.Height, static_cast<Uint>(desc.Format), static_cast<Uint>(desc.Usage));
		return TextureHandle::Invalid();
	}

	FramebufferHandle VulkanRHI::CreateFramebuffer(const FramebufferDesc& desc, const void* initialData /*= nullptr*/)
	{
		Log<Severity::Trace>("VulkanRHI::CreateFramebuffer of size {0}x{1} with {2} color attachments and depth attachment: {3}", desc.Width, desc.Height, desc.ColorAttachmentCount, desc.HasDepth);
		return FramebufferHandle::Invalid();
	}

	void VulkanRHI::UploadBuffer(BufferHandle h, const void* data, Uint size, Uint offset)
	{
		BufferEntry* entry = mBufferPool.Get(h);
		SG_ASSERT(entry != nullptr, "UploadBuffer: invalid handle!");

		VulkanBuffer::Upload(*this, *entry, data, size, offset);
	}

	void VulkanRHI::DestroyBuffer(BufferHandle buffer)
	{
		BufferEntry* entry = mBufferPool.Get(buffer);
		if (!entry)		
			return;
		
		Log<Severity::Info>("VulkanRHI::DestroyBuffer: Size: {0} bytes", entry->Size);
		VulkanBuffer::Destroy(*this, *entry);// kills VkBuffer + VmaAllocation
		mBufferPool.Free(buffer); // returns slot to free list
	}

	void VulkanRHI::DestroyTexture(TextureHandle texture)
	{
		Log<Severity::Info>("Destroying texture with handle index {0} and generation {1}", texture.Index, texture.Generation);
	}

	void VulkanRHI::DestroyFramebuffer(FramebufferHandle framebuffer)
	{
		Log<Severity::Info>("Destroying framebuffer with handle index {0} and generation {1}", framebuffer.Index, framebuffer.Generation);
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
}
