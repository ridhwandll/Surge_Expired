// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Abstraction/Vulkan/VulkanComputePipeline.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanRenderContext.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanRenderCommandBuffer.hpp"
#include "VulkanUtils.hpp"
#include "Surge/Core/Defines.hpp"

namespace Surge
{
    VulkanComputePipeline::VulkanComputePipeline(Ref<Shader>& computeShader)
        : mShader(computeShader.As<VulkanShader>()), mPipeline(VK_NULL_HANDLE), mPipelineLayout(VK_NULL_HANDLE)
    {
        Reload();
        mShaderReloadID = computeShader->AddReloadCallback([&]() { this->Reload(); });
    }

    VulkanComputePipeline::~VulkanComputePipeline()
    {
        Release();
        mShader->RemoveReloadCallback(mShaderReloadID);
    }

    void VulkanComputePipeline::Bind(const Ref<RenderCommandBuffer>& renderCmdBuffer)
    {
        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        Uint frameIndex = renderContext->GetFrameIndex();
        VkCommandBuffer vulkanCmdBuffer = renderCmdBuffer.As<VulkanRenderCommandBuffer>()->GetVulkanCommandBuffer(frameIndex);

        vkCmdBindPipeline(vulkanCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mPipeline);
    }

    void VulkanComputePipeline::SetPushConstantData(const Ref<RenderCommandBuffer>& cmdBuffer, const String& bufferName, void* data) const
    {
        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        Uint frameIndex = renderContext->GetFrameIndex();
        VkCommandBuffer vulkanCmdBuffer = cmdBuffer.As<VulkanRenderCommandBuffer>()->GetVulkanCommandBuffer(frameIndex);
        const VkPushConstantRange& pushConstant = mShader.As<VulkanShader>()->GetPushConstantRanges().at(bufferName);

        SG_ASSERT(pushConstant.stageFlags != 0, "Invalid Push constant name: '{0}'!", bufferName);
        vkCmdPushConstants(vulkanCmdBuffer, mPipelineLayout, pushConstant.stageFlags, pushConstant.offset, pushConstant.size, data);
    }

    void VulkanComputePipeline::Dispatch(const Ref<RenderCommandBuffer>& renderCmdBuffer, Uint groupCountX, Uint groupCountY, Uint groupCountZ)
    {
        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        Uint frameIndex = renderContext->GetFrameIndex();
        VkCommandBuffer vulkanCmdBuffer = renderCmdBuffer.As<VulkanRenderCommandBuffer>()->GetVulkanCommandBuffer(frameIndex);
        vkCmdDispatch(vulkanCmdBuffer, groupCountX, groupCountY, groupCountZ);
    }

    void VulkanComputePipeline::Reload()
    {
        Release();
        SCOPED_TIMER("VulkanComputePipeline::Reload");

        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        VkDevice logicalDevice = renderContext->GetDevice()->GetLogicalDevice();

        HashMap<ShaderType, VkShaderModule> shaderModules = mShader.As<VulkanShader>()->GetVulkanShaderModules();
        SG_ASSERT(shaderModules.size() == 1, "The Surge shader must only contain 1 shader, which is compute shader")
        VkPipelineShaderStageCreateInfo shaderStage {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        shaderStage.stage = VulkanUtils::GetVulkanShaderStage(ShaderType::Compute);
        shaderStage.module = shaderModules.at(ShaderType::Compute);
        shaderStage.pName = "main";
        shaderStage.pSpecializationInfo = nullptr;
        Vector<VkDescriptorSetLayout> descriptorSetLayouts;

        // (TODO: switch to bindless)
        { // "Mess Scope" read the comments inside this scope for detail

            // Create an empty layout
            VkDescriptorSetLayoutCreateInfo layoutInfo {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            VK_CALL(vkCreateDescriptorSetLayout(logicalDevice, &layoutInfo, nullptr, &mEmptyLayout));
            // We have to do this because:
            // The descriptor set layouts for a pipeline layout always start from set 0, so if a
            // shader only uses set 5, then you must create a pipeline layout with *5 descriptor sets*
            // Then it becomes: 0- mEmptyLayout; 1- mEmptyLayout; 2- mEmptyLayout; 3- mEmptyLayout; 4- mEmptyLayout; 5- TheRealLayoutFromTheShader
            const std::map<Uint, VkDescriptorSetLayout>& descriptorSetLayoutsMap = mShader.As<VulkanShader>()->GetDescriptorSetLayouts();
            // We can get the maximum number of set used in shader like this because std::map is already sorted in order
            Uint lastKeyOfMap = (--descriptorSetLayoutsMap.end())->first;
            for (Uint i = 0; i <= lastKeyOfMap; i++)
            {
                auto itr = descriptorSetLayoutsMap.find(i);
                if (itr != descriptorSetLayoutsMap.end())
                    descriptorSetLayouts.push_back(itr->second); // Push TheRealLayoutFromTheShader
                else
                    descriptorSetLayouts.push_back(mEmptyLayout); // Push mEmptyLayout
            }
            // Social Credits 100+ for reading these comments

        } // End of "Mess Scope"

        const Vector<VkPushConstantRange> pushConstants = VulkanUtils::GetPushConstantRangesVectorFromHashMap(mShader.As<VulkanShader>()->GetPushConstantRanges());

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipelineLayoutCreateInfo.setLayoutCount = static_cast<Uint>(descriptorSetLayouts.size());
        pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
        pipelineLayoutCreateInfo.pushConstantRangeCount = static_cast<Uint>(pushConstants.size());
        pipelineLayoutCreateInfo.pPushConstantRanges = pushConstants.data();
        VK_CALL(vkCreatePipelineLayout(logicalDevice, &pipelineLayoutCreateInfo, nullptr, &mPipelineLayout));
        SET_VK_OBJECT_DEBUGNAME(mPipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "Compute PipelineLayout");

        VkComputePipelineCreateInfo computePipelineCreateInfo {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        computePipelineCreateInfo.layout = mPipelineLayout;
        computePipelineCreateInfo.flags = 0;
        computePipelineCreateInfo.stage = shaderStage;
        VK_CALL(vkCreateComputePipelines(logicalDevice, nullptr, 1, &computePipelineCreateInfo, nullptr, &mPipeline));
        SET_VK_OBJECT_DEBUGNAME(mPipeline, VK_OBJECT_TYPE_PIPELINE, "Compute Pipeline");
    }

    void VulkanComputePipeline::Release()
    {
        if (!mPipeline)
            return;

        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        VkDevice logicalDevice = renderContext->GetDevice()->GetLogicalDevice();

        vkDestroyPipelineLayout(logicalDevice, mPipelineLayout, nullptr);
        mPipelineLayout = VK_NULL_HANDLE;
        vkDestroyPipeline(logicalDevice, mPipeline, nullptr);
        mPipeline = VK_NULL_HANDLE;
        vkDestroyDescriptorSetLayout(logicalDevice, mEmptyLayout, nullptr);
        mEmptyLayout = VK_NULL_HANDLE;
    }

} // namespace Surge
