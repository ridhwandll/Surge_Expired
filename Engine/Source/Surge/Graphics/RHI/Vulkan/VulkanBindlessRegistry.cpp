// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RHI/Vulkan/VulkanBindlessRegistry.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHI.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanUtils.hpp"

namespace Surge
{
    void VulkanBindlessRegistry::Init(const VulkanRHI& rhi)
    {
        VkDevice device = rhi.GetDevice();

        // Bindless layout

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {};
        bindings[0].binding = BUFFER_BINDING;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[0].descriptorCount = MAX_BUFFERS; //Max lights + Materials?
        bindings[0].stageFlags = VK_SHADER_STAGE_ALL;

        bindings[1].binding = TEXTURE_BINDING;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[1].descriptorCount = RHISettings::MAX_BINDLESS_TEXTURES;
        bindings[1].stageFlags = VK_SHADER_STAGE_ALL;

        // These three flags are the core of bindless
        std::array<VkDescriptorBindingFlags, 2> bindingFlags = {
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT ,
        };

        VkDescriptorSetLayoutBindingFlagsCreateInfoEXT flagsInfo = {};
        flagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
        flagsInfo.bindingCount = bindingFlags.size();
        flagsInfo.pBindingFlags = bindingFlags.data();

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.pNext = &flagsInfo;
        layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;
        layoutInfo.bindingCount = bindings.size();
        layoutInfo.pBindings = bindings.data();
        VK_CALL(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &mLayout));

        // Pool
        std::array<VkDescriptorPoolSize, 2> poolSizes = {};
        poolSizes[0] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RHISettings::MAX_BINDLESS_TEXTURES };
        poolSizes[1] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_BUFFERS };
        
        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = poolSizes.size();
        poolInfo.pPoolSizes = poolSizes.data();
        VK_CALL(vkCreateDescriptorPool(device, &poolInfo, nullptr, &mPool));

        // Descriptor set alloc
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
        Uint slot = AllocTextureSlot();

        VkDescriptorImageInfo imgInfo = {};
        imgInfo.sampler = sampler;
        imgInfo.imageView = view;
        imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = mSet;
        write.dstBinding = TEXTURE_BINDING;
        write.dstArrayElement = slot;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &imgInfo;

        vkUpdateDescriptorSets(rhi.GetDevice(), 1, &write, 0, nullptr);
        return slot;
    }

    Uint VulkanBindlessRegistry::RegisterBuffer(const VulkanRHI& rhi, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range)
    {
        SG_ASSERT(buffer != VK_NULL_HANDLE, "BindlessRegistry: cannot register null buffer!");

        Uint slot = AllocBufferSlot();

        VkDescriptorBufferInfo bufInfo = {};
        bufInfo.buffer = buffer;
        bufInfo.offset = offset;
        bufInfo.range = (range == 0) ? VK_WHOLE_SIZE : range;

        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = mSet;
        write.dstBinding = BUFFER_BINDING;
        write.dstArrayElement = slot;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.pBufferInfo = &bufInfo;

        vkUpdateDescriptorSets(rhi.GetDevice(), 1, &write, 0, nullptr);
        return slot;

    }

    void VulkanBindlessRegistry::UnregisterTexture(Uint bindlessIndex)
    {
        mFreeTextureSlots.push_back(bindlessIndex);
    }

    void VulkanBindlessRegistry::UnregisterBuffer(Uint bindlessIndex)
    {
        mFreeBufferSlots.push_back(bindlessIndex);
    }

    void VulkanBindlessRegistry::Bind(VkCommandBuffer cmd, VkPipelineLayout layout)
    {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, BINDLESS_SET, 1, &mSet, 0, nullptr);
    }

    Uint VulkanBindlessRegistry::AllocTextureSlot()
    {
        if (!mFreeTextureSlots.empty())
        {
            Uint slot = mFreeTextureSlots.back();
            mFreeTextureSlots.pop_back();
            return slot;
        }
        SG_ASSERT(mNextTextureSlot < MAX_TEXTURES, "BindlessRegistry: texture slots exhausted!");
        return mNextTextureSlot++;
    }

    Uint VulkanBindlessRegistry::AllocBufferSlot()
    {
        if (!mFreeBufferSlots.empty())
        {
            Uint slot = mFreeBufferSlots.back();
            mFreeBufferSlots.pop_back();
            return slot;
        }
        SG_ASSERT(mNextBufferSlot < MAX_BUFFERS, "BindlessRegistry: buffer slots exhausted!");
        return mNextBufferSlot++;
    }
}
