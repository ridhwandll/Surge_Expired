// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RHI/RHIDescs.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanResourceEntries.hpp"
#include <volk.h>
#include <vk_mem_alloc.h>

namespace Surge
{
	class VulkanRHI;
	class VulkanBuffer
	{
	public:
		static BufferEntry Create(const VulkanRHI& rhi, const BufferDesc& desc);
		static void Upload(BufferEntry& entry, const void* data, Uint size, Uint offset = 0);
		static void Destroy(const VulkanRHI& rhi, BufferEntry& entry);
	};

}