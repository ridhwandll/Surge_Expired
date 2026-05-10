// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RHI/RHIDescs.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanResourcePools.hpp"
#include <volk.h>

namespace Surge
{
	class VulkanRHI;
	class VulkanTexture
	{
	public:
		static TextureEntry Create(const VulkanRHI& rhi, const TextureDesc& desc);
		static void Destroy(const VulkanRHI& rhi, TextureEntry& entry);
		static void UploadData(VulkanRHI& rhi, TextureHandle h, const void* data, Uint size);

		// Record a pipeline barrier that transitions the image layout, Updates entry.Layout to newLayout after recording
		static void TransitionLayout(VkCommandBuffer cmd, TextureEntry& entry, VkImageLayout newLayout);

		static VkImageAspectFlags GetAspectFlags(TextureFormat format); 
	};

}