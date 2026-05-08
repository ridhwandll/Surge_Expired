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
		VkFormat Format;
		VkImage Image = VK_NULL_HANDLE;
		VkImageView View = VK_NULL_HANDLE;
		VmaAllocation Allocation = VK_NULL_HANDLE;
		VkImageLayout Layout = VK_IMAGE_LAYOUT_UNDEFINED;
		Uint Width = 0;
		Uint Height = 0;
		Uint Mips = 0;

		TextureDesc Desc = {};
	};

	struct BufferEntry
	{
		VkBuffer Buffer = VK_NULL_HANDLE;
		VmaAllocation Allocation = VK_NULL_HANDLE;
		void* MappedPtr = nullptr;
		uint64_t Size = 0;

		BufferDesc Desc = {};
	};

	struct PipelineEntry
	{
		VkPipeline Pipeline = VK_NULL_HANDLE;
		VkPipelineLayout Layout = VK_NULL_HANDLE;
		Uint PushConstantSize = 0;
		Uint Generation = 0;

		PipelineDesc Desc = {};
	};

	struct FramebufferEntry
	{
		VkFramebuffer Framebuffer = VK_NULL_HANDLE;
		VkRenderPass RenderPass = VK_NULL_HANDLE; // Borrowed from cache, do NOT destroy
		Uint Width = 0;
		Uint Height = 0;

		// Cached clear values
		VkClearValue ClearValues[9] = {}; // 8 color + 1 depth
		Uint ClearCount = 0;

		FramebufferDesc Desc = {};
	};
}