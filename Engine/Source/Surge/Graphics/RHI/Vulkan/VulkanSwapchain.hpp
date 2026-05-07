// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include <volk.h>

namespace Surge
{
	struct SwapchainDimensions
	{
		Uint Width = 0;
		Uint Height = 0;
		VkFormat Format = VK_FORMAT_UNDEFINED;
	};

	struct SwapchainFrame
	{
		//VkImage Image = VK_NULL_HANDLE; 
		VkImageView View = VK_NULL_HANDLE;
		VkSemaphore ReleaseSemaphore = VK_NULL_HANDLE;
	};

	class VulkanRHI;
	class VulkanSwapchain
	{
	public:
		void Initialize(const VulkanRHI& rhi, Uint width, Uint height);
		void Resize(const VulkanRHI& rhi, Uint width, Uint height);
		void Shutdown(const VulkanRHI& rhi);

		// Called by VulkanRHI frame loop
		// Returns VK_ERROR_OUT_OF_DATE_KHR if rebuild needed
		VkResult AcquireNextImage(const VulkanRHI& rhi, VkSemaphore acquireSemaphore, Uint& outImageIndex);
		VkResult Present(const VulkanRHI& rhi, VkSemaphore releaseSemaphore, Uint imageIndex);

		// Getters used by VulkanRHI when building framebuffers + submitting
		Uint GetImageCount() const { return (Uint)mFrames.size(); }
		const SwapchainFrame& GetFrame(Uint index) const { return mFrames[index]; }
		const Vector<SwapchainFrame>& GetFrames() const { return mFrames; }		
		VkSwapchainKHR GetSwapchain() const { return mSwapchain; }

		const SwapchainDimensions& GetDimensions() const { return mDimensions; }
		Uint GetWidth() const { return mDimensions.Width; }
		Uint GetHeight() const { return mDimensions.Height; }

	private:
		void Create(const VulkanRHI& rhi, Uint width, Uint height);
		void Destroy(const VulkanRHI& rhi);

		static VkSurfaceFormatKHR SelectSurfaceFormat(VkPhysicalDevice gpu, VkSurfaceKHR surface);
		static VkPresentModeKHR SelectPresentMode(VkPhysicalDevice gpu, VkSurfaceKHR surface);

		VkSwapchainKHR mSwapchain = VK_NULL_HANDLE;
		SwapchainDimensions mDimensions = {};
		Vector<SwapchainFrame> mFrames;
	};
}