// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include <volk.h>
#include <vk_mem_alloc.h>
#include "../RHIDescriptors.hpp"

namespace Surge
{
	struct TextureEntry
	{
		VkImage Image = VK_NULL_HANDLE;
		VkImageView View = VK_NULL_HANDLE;
		VmaAllocation Allocation = VK_NULL_HANDLE;
		VkImageLayout Layout = VK_IMAGE_LAYOUT_UNDEFINED;

		TextureDesc Desc = {};
	};

	struct BufferEntry
	{
		VkBuffer Buffer = VK_NULL_HANDLE;
		VmaAllocation Allocation = VK_NULL_HANDLE;
		void* MappedPtr = nullptr;

		BufferDesc Desc = {};
	};

	struct PipelineEntry
	{
		VkPipeline Pipeline = VK_NULL_HANDLE;
		VkPipelineLayout Layout = VK_NULL_HANDLE;

		PipelineDesc Desc = {};
	};

	struct FramebufferEntry
	{
		VkFramebuffer Framebuffer = VK_NULL_HANDLE;
		VkRenderPass RenderPass = VK_NULL_HANDLE; // Borrowed from cache, do NOT destroy

		// Cached clear values
		std::array<VkClearValue, 9> ClearValues = {}; // 8 color + 1 depth
		Uint ClearCount = 0;

		FramebufferDesc Desc = {};
	};
}