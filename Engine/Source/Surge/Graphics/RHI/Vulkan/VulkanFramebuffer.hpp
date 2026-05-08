// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RHI/RHIDescriptors.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanResourcePools.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRenderpassCache.hpp"

namespace Surge
{
	class VulkanRHI;
	class VulkanFramebuffer
	{
	public:
		static FramebufferEntry Create(const VulkanRHI& rhi, const FramebufferDesc& desc, VulkanRenderpassCache& cache, const HandlePool<TextureHandle, TextureEntry>& texPool);
		static void Destroy(const VulkanRHI& rhi, FramebufferEntry& entry);
	};
}