// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RHI/Vulkan/VulkanDescriptorSet.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHI.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanUtils.hpp"

namespace Surge
{

    DescriptorSetEntry VulkanDescriptorSet::Create(const VulkanRHI& rhi, const PipelineEntry* pipeline, Uint setNumber, DescriptorUpdateFrequency frequency, const char* debugName /*= nullptr*/)
    {
        VkDevice device = rhi.GetDevice();
        const Vector<VkDescriptorPool>& pools = rhi.GetNonResetableDescriptorPools();

        DescriptorSetEntry entry = {};
        entry.Frequency = frequency;
        entry.Count = (frequency == DescriptorUpdateFrequency::DYNAMIC) ? RHISettings::FRAMES_IN_FLIGHT : 1;

        VkDescriptorSetLayout layout = pipeline->DescSetLayouts[setNumber];
        SG_ASSERT(layout, "VulkanDescriptorSet::Create: invalid VkDescriptorSetLayout");

        for(Uint i = 0; i < entry.Count; i++)
        {
            VkDescriptorSetAllocateInfo allocInfo = {};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = pools[i];
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &layout;
            VK_CALL(vkAllocateDescriptorSets(device, &allocInfo, &entry.Sets[i]));


            String name = String(debugName) + (entry.Count > 1 ? " [Frame " + std::to_string(i) + "]" : "");
            SET_VK_DEBUG_NAME(rhi, VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)entry.Sets[i], name);
        }

        return entry;
    }

    void VulkanDescriptorSet::Bind(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout, VkDescriptorSet setHandle, DescriptorSetSlot slot)
    {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, (Uint)slot, 1, &setHandle, 0, nullptr);
    }

    void VulkanDescriptorSet::Update(const VulkanRHI& rhi, DescriptorSetEntry& entry, const DescriptorWrite* writes, Uint writeCount, Uint frameIndex)
    {
        // Dynamic: write into current frame's copy
        // Static: write into the single copy
        VkDescriptorSet targetSet = (entry.Frequency == DescriptorUpdateFrequency::DYNAMIC) ? entry.Sets[frameIndex] : entry.Sets[0];

        Vector<VkDescriptorImageInfo> imageInfos;
        Vector<VkDescriptorBufferInfo> bufferInfos;
        Vector<VkWriteDescriptorSet> vkWrites;
        imageInfos.reserve(writeCount);
        bufferInfos.reserve(writeCount);
        vkWrites.reserve(writeCount);

        for(Uint i = 0; i < writeCount; i++)
        {
            const DescriptorWrite& w = writes[i];

            VkWriteDescriptorSet vkWrite = {};
            vkWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            vkWrite.dstSet = targetSet;
            vkWrite.dstBinding = w.Binding;
            vkWrite.dstArrayElement = w.ArrayIndex;
            vkWrite.descriptorCount = 1;
            vkWrite.descriptorType = VulkanUtils::ToVkDescriptorType(w.Type);

            switch(w.Type)
            {
                case DescriptorType::TEXTURE:
                case DescriptorType::STORAGE_TEXTURE:
                {
                    const ImageEntry* tex = rhi.mTexturePool.Get(w.Texture);
                    SG_ASSERT(tex, "UpdateDescriptorSet: invalid TextureHandle at slot");

                    const SamplerEntry* smp = rhi.mSamplerPool.Get(w.Sampler);
                    SG_ASSERT(smp, "UpdateDescriptorSet: invalid SamplerHandle at slot");

                    VkDescriptorImageInfo img = {};
                    img.sampler = smp->Sampler;
                    img.imageView = tex->View;
                    img.imageLayout = (w.Type == DescriptorType::STORAGE_TEXTURE) ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    imageInfos.push_back(img);
                    vkWrite.pImageInfo = &imageInfos.back();
                    break;
                }
                case DescriptorType::UNIFORM_BUFFER:
                case DescriptorType::STORAGE_BUFFER:
                {
                    const BufferEntry* buf = rhi.mBufferPool.Get(w.Buffer);
                    SG_ASSERT(buf, "UpdateDescriptorSet: invalid BufferHandle at slot");

                    VkDescriptorBufferInfo bufInfo = {};
                    bufInfo.buffer = buf->Buffer;
                    bufInfo.offset = w.BufferOffset;
                    bufInfo.range = (w.BufferRange == 0) ? buf->Desc.Size : w.BufferRange;
                    bufferInfos.push_back(bufInfo);
                    vkWrite.pBufferInfo = &bufferInfos.back();
                    break;
                }
                default:
                    SG_ASSERT(false, "UpdateDescriptorSet: unhandled DescriptorType");
                    break;
            }
            vkWrites.push_back(vkWrite);
        }

        vkUpdateDescriptorSets(rhi.GetDevice(), (Uint)vkWrites.size(), vkWrites.data(), 0, nullptr);
    }

    void VulkanDescriptorSet::Destroy(const VulkanRHI& rhi, DescriptorSetEntry& entry)
    {
        VkDevice device = rhi.GetDevice();
        const Vector<VkDescriptorPool>& pools = rhi.GetNonResetableDescriptorPools();

        for(Uint index = 0; index < entry.Count; index++)
        {
            VkDescriptorPool pool = pools[index];
            vkFreeDescriptorSets(device, pool, 1, &entry.Sets[index]);
            entry.Sets[index] = VK_NULL_HANDLE;
        }
    }

}
