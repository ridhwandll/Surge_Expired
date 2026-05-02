// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include <volk.h>
#include <vk_mem_alloc.h>

namespace Surge
{
	struct TextureEntry
	{
		VkImage Image = VK_NULL_HANDLE;
		VkImageView View = VK_NULL_HANDLE;
		VmaAllocation Allocation = VK_NULL_HANDLE;
		VkImageLayout Layout = VK_IMAGE_LAYOUT_UNDEFINED;
		Uint Width = 0;
		Uint Height = 0;
		Uint Mips = 0;
	};

	struct BufferEntry
	{
		VkBuffer Buffer = VK_NULL_HANDLE;
		VmaAllocation Allocation = VK_NULL_HANDLE;
		void* MappedPtr = nullptr;
		uint64_t Size = 0;
	};

	struct FramebufferEntry
	{
		VkRenderPass RenderPass = VK_NULL_HANDLE;
		VkFramebuffer Framebuffer = VK_NULL_HANDLE;
		Uint Width = 0;
		Uint Height = 0;
	};
}