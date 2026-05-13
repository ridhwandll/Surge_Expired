// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RHI/RHIDescs.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanResourceEntries.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRenderpassFactory.hpp"

namespace Surge
{
	class VulkanRHI;
	class VulkanFramebuffer
	{
	public:
		static FramebufferEntry Create(const VulkanRHI& rhi, const FramebufferDesc& desc, VulkanRenderpassFactory& rpFactory, const HandlePool<TextureHandle, TextureEntry>& texPool);
		static void Destroy(const VulkanRHI& rhi, FramebufferEntry& entry);
	};
}