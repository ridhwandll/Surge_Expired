// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Core/Core.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanPipeline.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHI.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanUtils.hpp"
#include "Surge/Utility/Filesystem.hpp"

namespace Surge
{
    static DescriptorType ShaderBufferUsageToDescriptorType(ShaderBuffer::Usage type)
    {
        switch (type)
        {
        case ShaderBuffer::Usage::STORAGE: return DescriptorType::STORAGE_BUFFER;
        case ShaderBuffer::Usage::UNIFORM: return DescriptorType::UNIFORM_BUFFER;
        default:
            SG_ASSERT_INTERNAL("Invalid ShaderBuffer::Usage type!");
            return DescriptorType::UNIFORM_BUFFER;
        }
    }

    static bool IsSetBindless(const String& name)
    {
        String target = "BINDLESS";

        // Check if "BINDLESS" is present
        if (name.find(target) != std::string::npos)
            return true;

        return false;
    }

    PipelineEntry VulkanPipeline::Create(VulkanRHI& rhi, const PipelineDesc& desc, VkRenderPass renderPass)
    {
        SG_ASSERT(renderPass != VK_NULL_HANDLE, "PipelineDesc: renderPass is null");
        VkDevice device = rhi.GetDevice();

        const ShaderReflectionData& reflectedData = desc.Shader_.GetReflectionData();

        // TODO: Find required descriptor sets count
        //Vector<Uint> descrioptorSetsCount;
        //const Vector<ShaderBuffer>&  shaderBuffers = reflectedData.GetBuffers();
        //const Vector<ShaderResource>& shaderTextures = reflectedData.GetResources();
        //for (const ShaderBuffer& buffer : shaderBuffers)
        //{
        //	// Check if the number of the set is already mentioned in the vector
        //	if (std::find(descrioptorSetsCount.begin(), descrioptorSetsCount.end(), buffer.Set) == descrioptorSetsCount.end())
        //		descrioptorSetsCount.push_back(buffer.Set);
        //}
        //for (const ShaderResource& res : shaderTextures)
        //{
        //	// Check if the number of the set is already mentioned in the vector
        //	if (std::find(descrioptorSetsCount.begin(), descrioptorSetsCount.end(), res.Set) == descrioptorSetsCount.end())
        //		descrioptorSetsCount.push_back(res.Set);
        //}

        PipelineEntry entry = {};
        entry.Desc = desc;

        // Push constants
        const Vector<ShaderPushConstant>& pushConstants = reflectedData.GetPushConstantBuffers();
        // TODO: Remove this hardcoded 1 and make it dynamic based on the number of push constant buffers in the shader
        Vector<VkPushConstantRange> pushRanges(1);
        for (size_t i = 0; i < 1; ++i)
        {
            const ShaderPushConstant& pushConstant = pushConstants[i];
            SG_ASSERT(!(pushConstant.ShaderStages & ShaderType::COMPUTE), "Compute shader is not supported in VulkanPipeline yet!");

            //if (pushConstant.ShaderStages & ShaderType::VERTEX)
            //	pushRanges[i].stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
            //if (pushConstant.ShaderStages & ShaderType::FRAGMENT)
            //	pushRanges[i].stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;

            pushRanges[i].stageFlags |= VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
            pushRanges[i].offset = 0;
            pushRanges[i].size = pushConstant.Size;
        }


        // Pipeline layout
        VkPipelineLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

        std::array<VkDescriptorSetLayout, 2> vkDescSetLayouts = {};

        // WIP: Descriptor set layouts 
        DescriptorLayoutDesc layoutDesc = {};

        layoutDesc.BindingCount = 1;
        for (const ShaderBuffer& buffer : reflectedData.GetBuffers())
        {
            if (IsSetBindless(buffer.BufferName))
                continue; // Skip bindless buffers, as they are handled by the bindless registry

            DescriptorBinding dsc{};
            dsc.Binding = buffer.Binding;
            dsc.Type = ShaderBufferUsageToDescriptorType(buffer.ShaderUsage);
            dsc.Count = 1;
            dsc.Stage = buffer.ShaderStages;
            dsc.Partial = false;
            layoutDesc.Bindings[buffer.Binding] = dsc;
        }
        entry.DescriptorSetLayout = rhi.CreateDescriptorLayout(layoutDesc);
        vkDescSetLayouts[0] = rhi.mDescriptorLayoutPool.Get(entry.DescriptorSetLayout)->Layout;
        vkDescSetLayouts[1] = rhi.mBindlessRegistry.GetLayout(); // Bindless set layout

        // ^^Asscheek fucnkig Vulkan wasted my 5hr here. Summary:
        // Array index of a layout inside pSetLayouts directly establishes its set = N number in GLSL/HLSL
        // pSetLayouts[0] defines the layout for layout(set = 0, binding = ...)
        // pSetLayouts[1] defines the layout for layout(set = 1, binding = ...)
        // pSetLayouts[2] defines the layout for layout(set = 2, binding = ...)
        layoutInfo.pSetLayouts = vkDescSetLayouts.data();
        layoutInfo.setLayoutCount = static_cast<Uint>(vkDescSetLayouts.size());
        layoutInfo.pushConstantRangeCount = pushRanges.size();
        layoutInfo.pPushConstantRanges = pushRanges.data();
        VK_CALL(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &entry.Layout));

        // Shaders
        VkShaderModule vertModule = VK_NULL_HANDLE;
        VkShaderModule fragModule = VK_NULL_HANDLE;
        std::array<VkPipelineShaderStageCreateInfo, 2> stages = {};
        const Vector<SPIRVHandle>& spirvs = desc.Shader_.GetSPIRVs();
        for (const SPIRVHandle& spirv : spirvs)
        {
            VkShaderModuleCreateInfo moduleInfo{};
            moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            if (spirv.Type == ShaderType::VERTEX)
            {
                moduleInfo.codeSize = spirv.SPIRV.size() * sizeof(Uint);
                moduleInfo.pCode = spirv.SPIRV.data();
                VK_CALL(vkCreateShaderModule(device, &moduleInfo, nullptr, &vertModule));

                stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
                stages[0].module = vertModule;
                stages[0].pName = "main";
            }
            else if (spirv.Type == ShaderType::FRAGMENT)
            {
                moduleInfo.codeSize = spirv.SPIRV.size() * sizeof(Uint);
                moduleInfo.pCode = spirv.SPIRV.data();
                VK_CALL(vkCreateShaderModule(device, &moduleInfo, nullptr, &fragModule));

                stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                stages[1].module = fragModule;
                stages[1].pName = "main";
            }
            else if (spirv.Type == ShaderType::COMPUTE)
            {
                SG_ASSERT_INTERNAL("Compute shader is not supported in VulkanPipeline yet");
            }
        }

        // Vertex input
        const std::map<Uint, ShaderStageInput>& stageInputs = reflectedData.GetStageInputs().at(ShaderType::VERTEX);

        Uint stride = 0;
        for (const auto& [location, stageInput] : stageInputs)
            stride += stageInput.Size;

        VkVertexInputBindingDescription vertexBindingDescriptions{};
        Vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions(stageInputs.size());
        if(!stageInputs.empty()) // We need to support fullscreen quad with zero vertex input as well, so we can't assert that stage inputs must be present
        {
            vertexBindingDescriptions.binding = 0;
            vertexBindingDescriptions.stride = stride;
            vertexBindingDescriptions.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            for (Uint i = 0; i < stageInputs.size(); i++)
            {
                const ShaderStageInput& input = stageInputs.at(i);
                vertexAttributeDescriptions[i].binding = 0;
                vertexAttributeDescriptions[i].location = i;
                vertexAttributeDescriptions[i].format = VulkanUtils::ShaderDataTypeToVulkanFormat(input.DataType);
                vertexAttributeDescriptions[i].offset = input.Offset;
            }
        }

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.pNext = nullptr;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &vertexBindingDescriptions;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<Uint>(vertexAttributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = vertexAttributeDescriptions.data();

        // Input assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VulkanUtils::ToVkTopology(desc.Raster.Topo);

        // Rasterizer
        VkPipelineRasterizationStateCreateInfo raster = {};
        raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.polygonMode = VulkanUtils::ToVkPolygonMode(desc.Raster.Polygon);
        raster.cullMode = VulkanUtils::ToVkCullMode(desc.Raster.Cull);
        raster.frontFace = VulkanUtils::ToVkFrontFace(desc.Raster.Front);
        raster.lineWidth = desc.Raster.LineWidth;
        raster.depthClampEnable = desc.Raster.DepthClamp ? VK_TRUE : VK_FALSE;

        // Depth stencil
        VkPipelineDepthStencilStateCreateInfo depthStencil = {};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.pNext = nullptr;
        depthStencil.depthTestEnable = desc.Depth.TestEnable ? VK_TRUE : VK_FALSE;
        depthStencil.depthWriteEnable = desc.Depth.WriteEnable ? VK_TRUE : VK_FALSE;
        depthStencil.depthCompareOp = VulkanUtils::ToVkCompareOp(desc.Depth.Op);

        // Blend
        VkPipelineColorBlendAttachmentState blendAttachment = {};
        blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        blendAttachment.blendEnable = desc.Blend.Enable ? VK_TRUE : VK_FALSE;
        blendAttachment.srcColorBlendFactor = VulkanUtils::ToVkBlendFactor(desc.Blend.SrcColor);
        blendAttachment.dstColorBlendFactor = VulkanUtils::ToVkBlendFactor(desc.Blend.DstColor);
        blendAttachment.colorBlendOp = VulkanUtils::ToVkBlendOp(desc.Blend.ColorOp);
        blendAttachment.srcAlphaBlendFactor = VulkanUtils::ToVkBlendFactor(desc.Blend.SrcAlpha);
        blendAttachment.dstAlphaBlendFactor = VulkanUtils::ToVkBlendFactor(desc.Blend.DstAlpha);
        blendAttachment.alphaBlendOp = VulkanUtils::ToVkBlendOp(desc.Blend.AlphaOp);

        VkPipelineColorBlendStateCreateInfo blend = {};
        blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend.attachmentCount = 1;
        blend.pAttachments = &blendAttachment;

        // Viewport
        VkPipelineViewportStateCreateInfo viewport = {};
        viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport.viewportCount = 1;
        viewport.scissorCount = 1;

        std::array<VkDynamicState, 2> dynamics = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic = {};
        dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic.dynamicStateCount = dynamics.size();
        dynamic.pDynamicStates = dynamics.data();

        // No MSAA on mobile
        VkPipelineMultisampleStateCreateInfo multisample = {};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = stages.size();
        pipelineInfo.pStages = stages.data();
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewport;
        pipelineInfo.pRasterizationState = &raster;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &blend;
        pipelineInfo.pDynamicState = &dynamic;
        pipelineInfo.layout = entry.Layout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;
        VK_CALL(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &entry.Pipeline));

        vkDestroyShaderModule(device, vertModule, nullptr);
        vkDestroyShaderModule(device, fragModule, nullptr);

#if defined(SURGE_DEBUG)
        rhi.SetDebugName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)entry.Pipeline, desc.DebugName);
#endif

        return entry;
    }

    void VulkanPipeline::Destroy(VulkanRHI& rhi, PipelineEntry& entry)
    {
        VkDevice device = rhi.GetDevice();
        if (entry.Pipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(device, entry.Pipeline, nullptr);
            entry.Pipeline = VK_NULL_HANDLE;
        }

        if (entry.Layout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(device, entry.Layout, nullptr);
            entry.Layout = VK_NULL_HANDLE;
        }
        if (entry.DescriptorSetLayout != DescriptorLayoutHandle::Invalid())
        {
            rhi.DestroyDescriptorLayout(entry.DescriptorSetLayout);
            entry.DescriptorSetLayout = DescriptorLayoutHandle::Invalid();
        }
    }
}
