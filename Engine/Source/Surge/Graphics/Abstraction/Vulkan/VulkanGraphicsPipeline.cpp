// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Abstraction/Vulkan/VulkanGraphicsPipeline.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanRenderCommandBuffer.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanShader.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanUtils.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanFramebuffer.hpp"
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/Logger/Logger.hpp"

namespace Surge
{
    VulkanGraphicsPipeline::VulkanGraphicsPipeline(const GraphicsPipelineSpecification& pipelineSpec)
        : mSpecification(pipelineSpec)
    {
        Reload();
        mShaderReloadID = mSpecification.Shader->AddReloadCallback([&]() { this->Reload(); });
    }

    VulkanGraphicsPipeline::~VulkanGraphicsPipeline()
    {
        mSpecification.Shader->RemoveReloadCallback(mShaderReloadID);
        Clear();
    }

    void VulkanGraphicsPipeline::Reload()
    {
        Clear();
        LOGI("Pipeline Reload start: %s", mSpecification.DebugName.c_str());

        SCOPED_TIMER("VulkanGraphicsPipeline::Reload ({0})", mSpecification.DebugName);
        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        VkDevice logicalDevice = renderContext->GetDevice()->GetLogicalDevice();
        LOGI("Got logical device: %p", logicalDevice);

        // Setting up all the shaders into a create info class SURGE_API
        HashMap<ShaderType, VkShaderModule> shaderModules = mSpecification.Shader.As<VulkanShader>()->GetVulkanShaderModules();

        Vector<VkPipelineShaderStageCreateInfo> shaderStages;
        for (const auto& shader : shaderModules)
        {
            if (shader.first == ShaderType::Compute)
                continue;

            VkPipelineShaderStageCreateInfo& shaderStageInfo = shaderStages.emplace_back();
            shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStageInfo.stage = VulkanUtils::GetVulkanShaderStage(shader.first);
            shaderStageInfo.module = shader.second;
            shaderStageInfo.pName = "main";
            shaderStageInfo.pSpecializationInfo = nullptr;
            LOGI("Got shader: %p", shader.second);
        }

        const ShaderReflectionData& reflectedData = mSpecification.Shader->GetReflectionData();

        // We only need the stage input of vertex shader to generate the input layout
        const std::map<Uint, ShaderStageInput>& stageInputs = reflectedData.GetStageInputs().at(ShaderType::Vertex);
        LOGI("Stage inputs map size: %zu", stageInputs.size());

        // Calculate the stride
        Uint stride = 0;
        for (const auto& [location, stageInput] : stageInputs)
            stride += stageInput.Size;

        VkVertexInputBindingDescription vertexBindingDescriptions;
        vertexBindingDescriptions.binding = 0;
        vertexBindingDescriptions.stride = stride;
        vertexBindingDescriptions.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        Vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions(stageInputs.size());
        for (Uint i = 0; i < stageInputs.size(); i++)
        {
            const ShaderStageInput& input = stageInputs.at(i);
            vertexAttributeDescriptions[i].binding = 0;
            vertexAttributeDescriptions[i].location = i;
            vertexAttributeDescriptions[i].format = VulkanUtils::ShaderDataTypeToVulkanFormat(input.DataType);
            vertexAttributeDescriptions[i].offset = input.Offset;
        }

        VkPipelineVertexInputStateCreateInfo vertexInputInfo {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<Uint>(vertexAttributeDescriptions.size());
        vertexInputInfo.pVertexBindingDescriptions = &vertexBindingDescriptions;
        vertexInputInfo.pVertexAttributeDescriptions = vertexAttributeDescriptions.data();

        // Input Assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssembly {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        inputAssembly.topology = VulkanUtils::GetVulkanPrimitiveTopology(mSpecification.Topology);
        inputAssembly.primitiveRestartEnable = VK_FALSE;
        inputAssembly.flags = 0;

        // Setting the viewport and scissors to nullptr because later we will use dynamic pipeline states that avoids
        // pipeline recreation on window resize
        VkPipelineViewportStateCreateInfo viewportState {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewportState.viewportCount = 1;
        viewportState.pViewports = nullptr;
        viewportState.scissorCount = 1;
        viewportState.pScissors = nullptr;

        // Rasterizer stage
        VkPipelineRasterizationStateCreateInfo rasterizer {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VulkanUtils::GetVulkanPolygonMode(mSpecification.PolygonMode);
        //rasterizer.lineWidth = mSpecification.LineWidth;
        rasterizer.cullMode = VulkanUtils::GetVulkanCullModeFlags(mSpecification.CullingMode);
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // TODO: Maybe add as an specification option?
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f;
        rasterizer.depthBiasClamp = 0.0f;
        rasterizer.depthBiasSlopeFactor = 0.0f;

        // We wont use MSAA, never again
        VkPipelineMultisampleStateCreateInfo multisampling {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // Depth state
        VkPipelineDepthStencilStateCreateInfo depthStencil {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depthStencil.depthTestEnable = mSpecification.UseDepth;
        depthStencil.depthWriteEnable = mSpecification.UseDepth;
        depthStencil.depthCompareOp = VulkanUtils::GetVulkanCompareOp(mSpecification.DepthCompOperation);
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = mSpecification.UseStencil;

        // Blending
        VkPipelineColorBlendAttachmentState colorBlendAttachment {};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE; // TODO: Enable blending
        VkPipelineColorBlendStateCreateInfo colorBlending {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        // Setting up the pipeline layout
        Ref<VulkanShader> vulkanShader = mSpecification.Shader.As<VulkanShader>();
        Vector<VkDescriptorSetLayout> descriptorSetLayouts(0);
        const Vector<VkPushConstantRange> pushConstants = VulkanUtils::GetPushConstantRangesVectorFromHashMap(vulkanShader->GetPushConstantRanges());

        // (TODO: switch to bindless)
        { // "Mess Scope" read the comments inside this scope for detail

            // Create an empty layout
            VkDescriptorSetLayoutCreateInfo layoutInfo {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            VK_CALL(vkCreateDescriptorSetLayout(logicalDevice, &layoutInfo, nullptr, &mEmptyLayout));
            // We have to do this because:
            // The descriptor set layouts for a pipeline layout always start from set 0, so if a
            // shader only uses set 5, then you must create a pipeline layout with *5 descriptor sets*
            // Then it becomes: 0- mEmptyLayout; 1- mEmptyLayout; 2- mEmptyLayout; 3- mEmptyLayout; 4- mEmptyLayout; 5- TheRealLayoutFromTheShader
            const std::map<Uint, VkDescriptorSetLayout>& descriptorSetLayoutsMap = vulkanShader->GetDescriptorSetLayouts();
            // We can get the maximum number of set used in shader like this because std::map is already sorted in order
            if (!descriptorSetLayoutsMap.empty())
            {
                Uint lastKeyOfMap = (--descriptorSetLayoutsMap.end())->first;
                for (Uint i = 0; i <= lastKeyOfMap; i++)
                {
                    auto itr = descriptorSetLayoutsMap.find(i);
                    if (itr != descriptorSetLayoutsMap.end())
                        descriptorSetLayouts.push_back(itr->second); // Push TheRealLayoutFromTheShader
                    else
                        descriptorSetLayouts.push_back(mEmptyLayout); // Push mEmptyLayout
                }
            }
            // Social Credits 100+ for reading these comments

        } // End of "Mess Scope"

        VkPipelineLayoutCreateInfo pipelineLayoutInfo {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipelineLayoutInfo.setLayoutCount = static_cast<Uint>(descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
        //// TODO: Fix this hardcoded push constant count & array index!!!!!
        pipelineLayoutInfo.pushConstantRangeCount = 1; // static_cast<Uint>(pushConstants.size());
        pipelineLayoutInfo.pPushConstantRanges = &pushConstants[0];
        /////////////////////////////////////////////////////////////////
        VK_CALL(vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &mPipelineLayout));
        SET_VK_OBJECT_DEBUGNAME(mPipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "Graphics PipelineLayout");

        // Using dynamic pipeline states to avoid pipeline recreation when resizing
        std::array<VkDynamicState, 2> dynamicStates;
        dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
        dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;
        VkPipelineDynamicStateCreateInfo dynamicStatesCreateInfo {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamicStatesCreateInfo.dynamicStateCount = static_cast<Uint>(dynamicStates.size());
        dynamicStatesCreateInfo.pDynamicStates = dynamicStates.data();

        VkGraphicsPipelineCreateInfo pipelineInfo {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.stageCount = static_cast<Uint>(shaderStages.size());
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicStatesCreateInfo;
        pipelineInfo.layout = mPipelineLayout;

        if (mSpecification.TargetFramebuffer)
            pipelineInfo.renderPass = mSpecification.TargetFramebuffer.As<VulkanFramebuffer>()->GetVulkanRenderPass();
        else
            pipelineInfo.renderPass = renderContext->GetSwapChain()->GetVulkanRenderPass();

        pipelineInfo.subpass = 0;
        LOGI("About to call vkCreateGraphicsPipelines");

        // Log each vertex attribute format
        for (Uint i = 0; i < vertexAttributeDescriptions.size(); i++)
            LOGI("  attr[%u] location=%u format=%u offset=%u",
                 i,
                 vertexAttributeDescriptions[i].location,
                 vertexAttributeDescriptions[i].format,
                 vertexAttributeDescriptions[i].offset);

        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(renderContext->GetDevice()->GetPhysicalDevice(), VK_FORMAT_R32G32B32_SFLOAT, &props);
        LOGI("R32G32B32_SFLOAT bufferFeatures: %u", props.bufferFeatures);
        for (const auto& [type, module] : shaderModules)
            LOGI("  ShaderModule type=%d handle=%p", (int)type, (void*)module);
        LOGI("colorBlending attachmentCount: %u", colorBlending.attachmentCount);
        VK_CALL(vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mPipeline));
        LOGI("Pipeline created successfully");
        SET_VK_OBJECT_DEBUGNAME(mPipeline, VK_OBJECT_TYPE_PIPELINE, "Graphics Pipeline");
    }

    void VulkanGraphicsPipeline::Bind(const Ref<RenderCommandBuffer>& cmdBuffer) const
    {
        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        Uint frameIndex = renderContext->GetFrameIndex();
        VkCommandBuffer vulkanCmdBuffer = cmdBuffer.As<VulkanRenderCommandBuffer>()->GetVulkanCommandBuffer(frameIndex);

        VkExtent2D extent;
        if (mSpecification.TargetFramebuffer)
        {
            const FramebufferSpecification& framebufferSpec = mSpecification.TargetFramebuffer->GetSpecification();
            extent.width = framebufferSpec.Width;
            extent.height = framebufferSpec.Height;
        }
        else
            extent = renderContext->GetSwapChain()->GetVulkanExtent2D();

        vkCmdSetLineWidth(vulkanCmdBuffer, mSpecification.LineWidth);
        vkCmdBindPipeline(vulkanCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);
    }

    void VulkanGraphicsPipeline::SetPushConstantData(const Ref<RenderCommandBuffer>& cmdBuffer, const String& bufferName, void* data) const
    {
        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        Uint frameIndex = renderContext->GetFrameIndex();
        VkCommandBuffer vulkanCmdBuffer = cmdBuffer.As<VulkanRenderCommandBuffer>()->GetVulkanCommandBuffer(frameIndex);
        const VkPushConstantRange& pushConstant = mSpecification.Shader.As<VulkanShader>()->GetPushConstantRanges().at(bufferName);

        SG_ASSERT(pushConstant.stageFlags != 0, "Invalid Push constant name: '{0}'!", bufferName);
        vkCmdPushConstants(vulkanCmdBuffer, mPipelineLayout, pushConstant.stageFlags, pushConstant.offset, pushConstant.size, data);
    }

    void VulkanGraphicsPipeline::DrawIndexed(const Ref<RenderCommandBuffer>& cmdBuffer, Uint indicesCount, Uint baseIndex, Uint baseVertex) const
    {
        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        Uint frameIndex = renderContext->GetFrameIndex();
        VkCommandBuffer vulkanCmdBuffer = cmdBuffer.As<VulkanRenderCommandBuffer>()->GetVulkanCommandBuffer(frameIndex);

        vkCmdDrawIndexed(vulkanCmdBuffer, indicesCount, 1, baseIndex, baseVertex, 0);
    }

    void VulkanGraphicsPipeline::Clear()
    {
        if (!mPipeline)
            return;

        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        VkDevice device = renderContext->GetDevice()->GetLogicalDevice();
        vkDeviceWaitIdle(device);
        vkDestroyPipeline(device, mPipeline, nullptr);
        mPipeline = VK_NULL_HANDLE;
        vkDestroyPipelineLayout(device, mPipelineLayout, nullptr);
        mPipelineLayout = VK_NULL_HANDLE;
        vkDestroyDescriptorSetLayout(device, mEmptyLayout, nullptr);
        mEmptyLayout = VK_NULL_HANDLE;
    }

} // namespace Surge
