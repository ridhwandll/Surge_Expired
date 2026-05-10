// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RHI/RHIDescs.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanResourcePools.hpp"
#include <volk.h>

namespace Surge
{
	class VulkanRHI;
	class VulkanBindlessRegistry
	{
	public:
		static constexpr Uint MAX_TEXTURES = 4096;

		void Init(const VulkanRHI& rhi);
		void Shutdown(const VulkanRHI& rhi);

		// Called by CreateTexture returns the permanent global index
		Uint RegisterTexture(const VulkanRHI& rhi, VkImageView view, VkSampler sampler);

		// Called by DestroyTexture, frees the slot for reuse
		void UnregisterTexture(Uint bindlessIndex);

		void Bind(VkCommandBuffer cmd, VkPipelineLayout layout);
		VkDescriptorSetLayout GetLayout() const { return mLayout; }

	private:
		VkDescriptorSetLayout mLayout = VK_NULL_HANDLE;
		VkDescriptorPool mPool = VK_NULL_HANDLE;
		VkDescriptorSet mSet = VK_NULL_HANDLE;

		// Free slot tracking
		Vector<Uint> mFreeSlots;
		Uint mNextSlot = 0;
	};

}