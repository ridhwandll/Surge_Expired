// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include <volk.h>

namespace Surge
{
	struct PerFrame
	{
		VkCommandPool CmdPool = VK_NULL_HANDLE;
		VkCommandBuffer CmdBuffer = VK_NULL_HANDLE;
		VkFence Fence = VK_NULL_HANDLE;
		VkSemaphore AcquireSemaphore = VK_NULL_HANDLE;
	};

	class VulkanRHI;
	class VulkanFrame
	{
	public:
		void Initialize(const VulkanRHI& rhi, Uint frameCount);
		void Shutdown(const VulkanRHI& rhi);

		// Called at the start of each frame advances the index
		PerFrame& GetCurrentFrame() { return mFrames[mCurrentIndex]; }
		PerFrame& GetFrame(Uint index) { return mFrames[index]; }

		void AdvanceFrame();

		Uint GetCurrentFrameIndex() const { return mCurrentIndex; }

	private:
		void InitSlot(const VulkanRHI& rhi, PerFrame& frame);
		void DestroySlot(const VulkanRHI& rhi, PerFrame& frame);

		Vector<PerFrame> mFrames;
		Uint mCurrentIndex = 0;
	};
}