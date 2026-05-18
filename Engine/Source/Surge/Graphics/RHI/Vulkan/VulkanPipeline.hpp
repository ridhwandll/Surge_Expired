// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/String.hpp"
#include "Surge/Graphics/RHI/RHIDescs.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanResourceEntries.hpp"
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
    private:
        static Vector<VkDescriptorSetLayout> CreateDescriptorSetLayouts(VkDevice device, const ShaderReflectionData& reflectedData);
        static Vector<VkPushConstantRange> CreatePushConstantRanges(const ShaderReflectionData& reflectedData);
        static Pair<VkVertexInputBindingDescription, Vector<VkVertexInputAttributeDescription>> CreateVertexAttributes(const ShaderReflectionData& reflectedData);
    };
}
