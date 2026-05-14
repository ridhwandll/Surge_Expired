// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Graphics/RHI/RHISettings.hpp"
#include "Surge/Graphics/RHI/RHIDescs.hpp"
#include <volk.h>
#include <vk_mem_alloc.h>
#include "imgui.h"

namespace Surge
{
	struct TextureEntry
	{
		VkImage Image = VK_NULL_HANDLE;
		VkImageView View = VK_NULL_HANDLE;
		Uint BindlessIndex = UINT32_MAX;
		ImTextureID ImGuiID = NULL;
		VkDeviceSize Size = 0; //bytes
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

		DescriptorLayoutHandle DescriptorSetLayout = {};
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

	struct SamplerEntry
	{
		VkSampler Sampler = VK_NULL_HANDLE;

		SamplerDesc Desc = {};
	};

	struct DescriptorLayoutEntry
	{
		VkDescriptorSetLayout Layout = VK_NULL_HANDLE;		
		DescriptorLayoutDesc Desc = {};
	};

	enum class DescriptorUpdateFrequency
	{
		STATIC,  // set once, never updated, skybox, font atlas, LUTs
		DYNAMIC, // updated per frame. per-object params, animated materials
	};

	//class GraphicsRHI;
	struct DescriptorSetEntry
	{
		VkDescriptorSet Sets[RHISettings::FRAMES_IN_FLIGHT] = {};
		VkDescriptorPool Pool = VK_NULL_HANDLE; // owns its own pool
		DescriptorLayoutHandle Layout = DescriptorLayoutHandle::Invalid();
	    DescriptorUpdateFrequency Frequency = DescriptorUpdateFrequency::STATIC;
		Uint Count = 1; // 1 if Static, FRAMES_IN_FLIGHT if Dynamic
	};
}
