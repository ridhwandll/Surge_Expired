// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Graphics/RHI/RHISettings.hpp"
#include "Surge/Graphics/RHI/RHIDescs.hpp"
#include <volk.h>
#include <vk_mem_alloc.h>
#include "imgui.h"

namespace Surge
{
    struct ImageEntry
    {
        VkImage Image = VK_NULL_HANDLE;
        VkImageView View = VK_NULL_HANDLE;

        ImTextureID ImGuiID = NULL;
        VkDeviceSize Size = 0; //bytes
        VmaAllocation Allocation = VK_NULL_HANDLE;
        VkImageLayout Layout = VK_IMAGE_LAYOUT_UNDEFINED;

        ImageDesc Desc = {};
    };

    struct BufferEntry
    {
        VkBuffer Buffer = VK_NULL_HANDLE;
        VmaAllocation Allocation = VK_NULL_HANDLE;
        void* MappedPtr = nullptr;

        BufferDesc Desc = {};
    };

    struct PipelineEntry
    {
        VkPipeline Pipeline = VK_NULL_HANDLE;
        VkPipelineLayout Layout = VK_NULL_HANDLE;

        //  Guaranteed minimum is 4 VkDescriptorSetLayout per pipeline, guaranteed to be able to simultaneously use sets 0 through 3 across all Vulkan 1.1 compliant hardware         
        VkDescriptorSetLayout DescSetLayouts[4];
        Uint DescSetLayoutsCount = 0;

        PipelineDesc Desc = {};
    };

    struct FramebufferEntry
    {
        VkFramebuffer Framebuffer = VK_NULL_HANDLE;
        VkRenderPass RenderPass = VK_NULL_HANDLE; // Borrowed from cache, do NOT destroy

        // Cached clear values
        std::array<VkClearValue, 9> ClearValues = {}; // 8 color + 1 depth
        Uint ClearCount = 0;

        FramebufferDesc Desc = {};
    };

    struct SamplerEntry
    {
        VkSampler Sampler = VK_NULL_HANDLE;
        SamplerDesc Desc = {};
    };

    struct DescriptorSetEntry
    {
        DescriptorUpdateFrequency Frequency = DescriptorUpdateFrequency::STATIC;

        VkDescriptorSet Sets[RHISettings::FRAMES_IN_FLIGHT] = {};
        Uint Count = 1; // 1 if DescriptorUpdateFrequency::STATIC, FRAMES_IN_FLIGHT if DescriptorUpdateFrequency::DYNAMIC
    };
}
