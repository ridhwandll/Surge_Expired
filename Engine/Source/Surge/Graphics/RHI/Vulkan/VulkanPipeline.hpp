// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/String.hpp"
#include "Surge/Graphics/RHI/RHIDescs.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanResourcePools.hpp"
#include <volk.h>
#include <vk_mem_alloc.h>

namespace Surge
{
	class VulkanRHI;
	class VulkanPipeline
	{
	public:
		static PipelineEntry Create(VulkanRHI& rhi, const PipelineDesc& desc, VkRenderPass renderPass);
		static void Destroy(VulkanRHI& rhi, PipelineEntry& entry);
	};

}