// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RHI/RHIDescs.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanResourceEntries.hpp"
#include <volk.h>

namespace Surge
{
	class VulkanRHI;
	class VulkanBindlessRegistry
	{
	public:
		inline static constexpr Uint BINDLESS_SET = 1; // Set 1 is FrameUBO

		inline static constexpr Uint TEXTURE_BINDING = 1;
		inline static constexpr Uint BUFFER_BINDING = 0;

		inline static constexpr Uint MAX_TEXTURES = 4096;
		inline static constexpr Uint MAX_BUFFERS = 1024;

		void Init(const VulkanRHI& rhi);
		void Shutdown(const VulkanRHI& rhi);

		// Called by CreateTexture returns the permanent global index
		Uint RegisterTexture(const VulkanRHI& rhi, VkImageView view, VkSampler sampler);
		Uint RegisterBuffer(const VulkanRHI& rhi, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range = 0);

		// Called by DestroyTexture, frees the slot for reuse
		void UnregisterTexture(Uint bindlessIndex);
		void UnregisterBuffer(Uint bindlessIndex);

		void Bind(VkCommandBuffer cmd, VkPipelineLayout layout);
		VkDescriptorSetLayout GetLayout() const { return mLayout; }
	private:
		Uint AllocTextureSlot();
		Uint AllocBufferSlot();

	private:
		VkDescriptorSetLayout mLayout = VK_NULL_HANDLE;
		VkDescriptorPool mPool = VK_NULL_HANDLE;
		VkDescriptorSet mSet = VK_NULL_HANDLE;

		Vector<Uint> mFreeTextureSlots;
		Vector<Uint> mFreeBufferSlots;
		Uint mNextTextureSlot = 0;
		Uint mNextBufferSlot = 0;
	};

}