// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RHI/RHIDescs.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanResourceEntries.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRenderpassFactory.hpp"

namespace Surge
{
    class VulkanRHI;
    class VulkanDescriptorSet
    {
    public:
        static DescriptorSetEntry Create(const VulkanRHI& rhi, const PipelineEntry* pipeline, Uint setNumber, DescriptorUpdateFrequency frequency, const char* debugName = nullptr);
        static void Bind(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout, VkDescriptorSet setHandle, Uint setIndex);
        static void Update(const VulkanRHI& rhi, DescriptorSetEntry& entry, const DescriptorWrite* writes, Uint writeCount);
        static void Destroy(const VulkanRHI& rhi, DescriptorSetEntry& entry);
    };
}