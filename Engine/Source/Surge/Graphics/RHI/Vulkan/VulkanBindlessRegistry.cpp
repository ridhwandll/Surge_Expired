// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RHI/Vulkan/VulkanBindlessRegistry.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHI.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanUtils.hpp"

namespace Surge
{
	void VulkanBindlessRegistry::Init(const VulkanRHI& rhi)
	{
		VkDevice device = rhi.GetDevice();

		VkDescriptorSetLayoutBinding binding = {};
		binding.binding = 0;
		binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding.descriptorCount = RHISettings::MAX_BINDLESS_TEXTURES;
		binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;

		// These three flags are the core of bindless
		VkDescriptorBindingFlags bindlessFlags =
			VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |          // Unused slots don't need valid data
			VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |        // Update while set is bound
			VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT; // Variable size array

		VkDescriptorSetLayoutBindingFlagsCreateInfoEXT flagsInfo = {};
		flagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
		flagsInfo.bindingCount = 1;
		flagsInfo.pBindingFlags = &bindlessFlags;

		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.pNext = &flagsInfo;
		layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &binding;
		VK_CALL(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &mLayout));

		// Add buffers here later if we want to support bindless buffers, but for now we only need bindless textures
		VkDescriptorPoolSize poolSize = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RHISettings::MAX_BINDLESS_TEXTURES };
		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
		poolInfo.maxSets = 1;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolSize;
		VK_CALL(vkCreateDescriptorPool(device, &poolInfo, nullptr, &mPool));

		VkDescriptorSetVariableDescriptorCountAllocateInfoEXT countInfo = {};
		countInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT;
		countInfo.descriptorSetCount = 1;
		Uint maxCount = RHISettings::MAX_BINDLESS_TEXTURES;
		countInfo.pDescriptorCounts = &maxCount;

		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.pNext = &countInfo;
		allocInfo.descriptorPool = mPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &mLayout;
		VK_CALL(vkAllocateDescriptorSets(device, &allocInfo, &mSet));
	}

	void VulkanBindlessRegistry::Shutdown(const VulkanRHI& rhi)
	{
		if (mLayout)
			vkDestroyDescriptorSetLayout(rhi.GetDevice(), mLayout, nullptr);
		if (mPool)
			vkDestroyDescriptorPool(rhi.GetDevice(), mPool, nullptr);

		mLayout = VK_NULL_HANDLE;
		mPool = VK_NULL_HANDLE;
	}

	Uint VulkanBindlessRegistry::RegisterTexture(const VulkanRHI& rhi, VkImageView view, VkSampler sampler)
	{
		Uint slot;
		if (!mFreeSlots.empty())
		{
			slot = mFreeSlots.back();
			mFreeSlots.pop_back();
		}
		else
		{
			SG_ASSERT(mNextSlot < RHISettings::MAX_BINDLESS_TEXTURES, "Bindless registry full");
			slot = mNextSlot++;
		}

		// Write into the global array UPDATE_AFTER_BIND makes this safe
		// even if the set is currently bound
		VkDescriptorImageInfo imgInfo = {};
		imgInfo.sampler = sampler;
		imgInfo.imageView = view;
		imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet write = {};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = mSet;
		write.dstBinding = 0;
		write.dstArrayElement = slot; //write into specific slot
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write.pImageInfo = &imgInfo;
		vkUpdateDescriptorSets(rhi.GetDevice(), 1, &write, 0, nullptr);

		return slot;
	}

	void VulkanBindlessRegistry::UnregisterTexture(Uint bindlessIndex)
	{
		mFreeSlots.push_back(bindlessIndex);
	}

	void VulkanBindlessRegistry::Bind(VkCommandBuffer cmd, VkPipelineLayout layout)
	{
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &mSet, 0, nullptr);
	}
}
