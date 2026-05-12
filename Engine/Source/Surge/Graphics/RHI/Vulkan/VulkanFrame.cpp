// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RHI/Vulkan/VulkanFrame.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHI.hpp"

namespace Surge
{
	void VulkanFrame::Initialize(const VulkanRHI& rhi, Uint framesInFlight)
	{
		SG_ASSERT(framesInFlight > 0, "VulkanFrame: frameCount must be > 0");

		mFrames.resize(framesInFlight);
		for (auto& frame : mFrames)
			InitSlot(rhi, frame);
	}

	void VulkanFrame::Shutdown(const VulkanRHI& rhi)
	{
		for (auto& frame : mFrames)
			DestroySlot(rhi, frame);

		mFrames.clear();
		mCurrentIndex = 0;
	}

	void VulkanFrame::AdvanceFrame()
	{
		mCurrentIndex = (mCurrentIndex + 1) % (Uint)mFrames.size();
	}

	void VulkanFrame::InitSlot(const VulkanRHI& rhi, PerFrame& frame)
	{
		VkDevice device = rhi.GetDevice();
		Uint queueIndex = rhi.GetQueueIndex();

		// Fence
		// Created pre-signaled so the first vkWaitForFences on frame 0 doesn't block forever waiting for a submit that never happened
		VkFenceCreateInfo fenceInfo = {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		VK_CALL(vkCreateFence(device, &fenceInfo, nullptr, &frame.Fence));
		
		// Command pool 
		// TRANSIENT -> tells the driver these buffers are short-lived (reset every frame)
		// RESET_COMMAND_BUFFER -> not needed since we reset the entire pool at once
		VkCommandPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		poolInfo.queueFamilyIndex = queueIndex;
		VK_CALL(vkCreateCommandPool(device, &poolInfo, nullptr, &frame.CmdPool));

		// Command buffer
		// One primary command buffer per frame slot
		VkCommandBufferAllocateInfo cmdInfo = {};
		cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdInfo.commandPool = frame.CmdPool;
		cmdInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdInfo.commandBufferCount = 1;
		VK_CALL(vkAllocateCommandBuffers(device, &cmdInfo, &frame.CmdBuffer));

		// Semaphores
		VkSemaphoreCreateInfo semInfo = {};
		semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		VK_CALL(vkCreateSemaphore(device, &semInfo, nullptr, &frame.AcquireSemaphore));
	}

	void VulkanFrame::DestroySlot(const VulkanRHI& rhi, PerFrame& frame)
	{
		VkDevice device = rhi.GetDevice();

		if (frame.CmdPool != VK_NULL_HANDLE)
		{
			vkDestroyCommandPool(device, frame.CmdPool, nullptr);
			frame.CmdPool = VK_NULL_HANDLE;
			frame.CmdBuffer = VK_NULL_HANDLE; // Implicitly freed
		}

		if (frame.Fence != VK_NULL_HANDLE)
		{
			vkDestroyFence(device, frame.Fence, nullptr);
			frame.Fence = VK_NULL_HANDLE;
		}

		if (frame.AcquireSemaphore != VK_NULL_HANDLE)
		{
			vkDestroySemaphore(device, frame.AcquireSemaphore, nullptr);
			frame.AcquireSemaphore = VK_NULL_HANDLE;
		}
	}

}
