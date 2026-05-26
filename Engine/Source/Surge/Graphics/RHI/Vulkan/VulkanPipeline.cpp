// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Core/Core.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanPipeline.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHI.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanUtils.hpp"
#include "Surge/Utility/Filesystem.hpp"

namespace Surge
{
    static bool IsSingleChannelColor(ImageFormat format)
    {
        switch(format)
        {
            case Surge::ImageFormat::R8_UNORM:
                return true;
            case Surge::ImageFormat::RGBA8_SRGB:
            case Surge::ImageFormat::RGBA8_UNORM:
            case Surge::ImageFormat::BGRA8_SRGB:
            case Surge::ImageFormat::R16G16B16A16_SFLOAT:
            case Surge::ImageFormat::B10G11R11_UFLOAT_PACK32:
                return false;
            default:
                SG_ASSERT_INTERNAL("IsSingleChannelColor: Unhandled Image Format!(Are you passing a depth format in IsSingleChannelColor function?)");
        }
    }

    VkDescriptorType ShaderImageUsageToVulkan(ShaderResource::Usage type)
    {
        switch(type)
        {
            case ShaderResource::Usage::SAMPLED: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            case ShaderResource::Usage::STORAGE: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        }
        SG_ASSERT(false, "ShaderResource::Usage is invalid");
        return VkDescriptorType();
    }

    VkDescriptorType ShaderBufferTypeToVulkan(ShaderBuffer::Usage type)
    {
        switch(type)
        {
            case ShaderBuffer::Usage::STORAGE: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            case ShaderBuffer::Usage::UNIFORM: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }
        SG_ASSERT(false, "ShaderBuffer::Usage is invalid");
        return VkDescriptorType();
    }

    static VkDescriptorSetLayout sEmptyLayout = VK_NULL_HANDLE;

    PipelineEntry VulkanPipeline::Create(VulkanRHI& rhi, const PipelineDesc& desc, VkRenderPass renderPass)
    {
        SG_ASSERT(renderPass != VK_NULL_HANDLE, "PipelineDesc: renderPass is null");

        PipelineEntry entry = {};
        entry.Desc = desc;

        VkDevice device = rhi.GetDevice();

        const ShaderReflectionData& shaderReflection = desc.Shader_.GetReflectionData();
        Vector<VkPushConstantRange> pushRanges = CreatePushConstantRanges(shaderReflection);
        Vector<VkDescriptorSetLayout> descriptorSetLayouts = CreateDescriptorSetLayouts(device, shaderReflection);

        entry.DescSetLayoutsCount = descriptorSetLayouts.size();
        for(Uint i = 0; i < descriptorSetLayouts.size(); i++)
            entry.DescSetLayouts[i] = descriptorSetLayouts[i];

        // Pipeline layout
        VkPipelineLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = entry.DescSetLayoutsCount;
        layoutInfo.pSetLayouts = entry.DescSetLayouts;
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
        auto [vertexBindingDescriptions, vertexAttributeDescriptions] = CreateVertexAttributes(shaderReflection);
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

        // Depth
        VkPipelineDepthStencilStateCreateInfo depthStencil = {};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.pNext = nullptr;
        depthStencil.depthTestEnable = desc.Depth.TestEnable ? VK_TRUE : VK_FALSE;
        depthStencil.depthWriteEnable = desc.Depth.WriteEnable ? VK_TRUE : VK_FALSE;
        depthStencil.depthCompareOp = VulkanUtils::ToVkCompareOp(desc.Depth.Op);

        // Stencil
        depthStencil.stencilTestEnable = desc.Stencil.Enable ? VK_TRUE : VK_FALSE;

        // Front face stencil ops
        depthStencil.front.failOp = VulkanUtils::ToVkStencilOp(desc.Stencil.Front.Fail);
        depthStencil.front.depthFailOp = VulkanUtils::ToVkStencilOp(desc.Stencil.Front.DepthFail);
        depthStencil.front.passOp = VulkanUtils::ToVkStencilOp(desc.Stencil.Front.Pass);
        depthStencil.front.compareOp = VulkanUtils::ToVkCompareOp(desc.Stencil.Front.CompareOp_);
        depthStencil.front.compareMask = desc.Stencil.Front.CompareMask;
        depthStencil.front.writeMask = desc.Stencil.Front.WriteMask;
        depthStencil.front.reference = desc.Stencil.Front.Reference;

        // Back face stencil ops
        depthStencil.back.failOp = VulkanUtils::ToVkStencilOp(desc.Stencil.Back.Fail);
        depthStencil.back.depthFailOp = VulkanUtils::ToVkStencilOp(desc.Stencil.Back.DepthFail);
        depthStencil.back.passOp = VulkanUtils::ToVkStencilOp(desc.Stencil.Back.Pass);
        depthStencil.back.compareOp = VulkanUtils::ToVkCompareOp(desc.Stencil.Back.CompareOp_);
        depthStencil.back.compareMask = desc.Stencil.Back.CompareMask;
        depthStencil.back.writeMask = desc.Stencil.Back.WriteMask;
        depthStencil.back.reference = desc.Stencil.Back.Reference;

        // Blend
        VkPipelineColorBlendAttachmentState blendAttachment = {};
        blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        if(!desc.TargetSwapchain)
        {
            const FramebufferDesc& fbdesc = rhi.GetDesc(desc.TargetFramebuffer);
            if((fbdesc.ColorAttachmentCount == 1))
            {
                const ImageDesc& imgDesc = rhi.GetDesc(fbdesc.ColorAttachments[0].Handle);
                if(IsSingleChannelColor(imgDesc.Format))
                {
                    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
                }
            }
        }
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

        SET_VK_DEBUG_NAME(rhi, VK_OBJECT_TYPE_PIPELINE_LAYOUT, (uint64_t)entry.Layout, (desc.DebugName + String(" [Pipeline Layout]")));
        SET_VK_DEBUG_NAME(rhi, VK_OBJECT_TYPE_PIPELINE, (uint64_t)entry.Pipeline, (desc.DebugName + String(" [Pipeline]")));

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

        for(Uint i = 0; i < entry.DescSetLayoutsCount; i++)
        {
             if (entry.DescSetLayouts[i] != VK_NULL_HANDLE)
             {
                 vkDestroyDescriptorSetLayout(device, entry.DescSetLayouts[i], nullptr);
                 entry.DescSetLayouts[i] = VK_NULL_HANDLE;
             }
        }
        entry.DescSetLayoutsCount = 0;

        if(sEmptyLayout)
        {
            vkDestroyDescriptorSetLayout(device, sEmptyLayout, nullptr);
            sEmptyLayout = VK_NULL_HANDLE;
        }
    }

    Vector<VkDescriptorSetLayout> VulkanPipeline::CreateDescriptorSetLayouts(VkDevice device, const ShaderReflectionData& reflectedData)
    {
        const Vector<ShaderBuffer>& shaderBuffers = reflectedData.GetBuffers();
        const Vector<ShaderResource>& shaderResources = reflectedData.GetResources();

        // Find required descriptor sets count
        Vector<Uint> descriptorSetCount; // Vector<SetNumber in Shader>
        for(const ShaderBuffer& buffer : shaderBuffers)
        {
            // Check if the number of the set is already mentioned in the vector
            if(std::find(descriptorSetCount.begin(), descriptorSetCount.end(), buffer.Set) == descriptorSetCount.end())
                descriptorSetCount.push_back(buffer.Set);
        }
        for(const ShaderResource& res : shaderResources)
        {
            // Check if the number of the set is already mentioned in the vector
            if(std::find(descriptorSetCount.begin(), descriptorSetCount.end(), res.Set) == descriptorSetCount.end())
                descriptorSetCount.push_back(res.Set);
        }

        // Iterate through all the sets and create the layouts
        std::map<Uint/*SetNumber*/, VkDescriptorSetLayout> descriptorSetLayoutsMap;
        for(const Uint& descriptorSet : descriptorSetCount)
        {
            Vector<VkDescriptorSetLayoutBinding> layoutBindings;
            for(const ShaderBuffer& buffer : shaderBuffers)
            {
                if(buffer.Set != descriptorSet)
                    continue;

                layoutBindings.emplace_back();
                VkDescriptorSetLayoutBinding& layoutBinding = layoutBindings.back();
                layoutBinding.binding = buffer.Binding;
                layoutBinding.descriptorCount = 1; // TODO: Need to add arrays
                layoutBinding.descriptorType = ShaderBufferTypeToVulkan(buffer.ShaderUsage);
                layoutBinding.stageFlags = VulkanUtils::ShaderTypeToVulkanShaderStage(buffer.ShaderStages);
            }

            for(const ShaderResource& texture : shaderResources)
            {
                if(texture.Set != descriptorSet)
                    continue;

                layoutBindings.emplace_back();
                VkDescriptorSetLayoutBinding& layoutBinding = layoutBindings.back();
                layoutBinding.binding = texture.Binding;
                layoutBinding.descriptorCount = texture.ArraySize;
                layoutBinding.descriptorType = ShaderImageUsageToVulkan(texture.ShaderUsage);
                layoutBinding.stageFlags = VulkanUtils::ShaderTypeToVulkanShaderStage(texture.ShaderStages);
            }

            VkDescriptorSetLayoutCreateInfo layoutInfo { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
            layoutInfo.flags = 0;
            layoutInfo.bindingCount = static_cast<Uint>(layoutBindings.size());
            layoutInfo.pBindings = layoutBindings.data();
            VK_CALL(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayoutsMap[descriptorSet]));
        }

        Vector<VkDescriptorSetLayout> descriptorSetLayouts;
        { // "Mess Scope" read the comments inside this scope for details
            // Create an empty layout
            if(!sEmptyLayout)
            {
                VkDescriptorSetLayoutCreateInfo layoutInfo { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
                VK_CALL(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &sEmptyLayout));
            }
            // Array index of a layout inside pSetLayouts directly establishes its set = N number in GLSL/HLSL
            // pSetLayouts[0] defines the layout for layout(set = 0, binding = ...)
            // pSetLayouts[1] defines the layout for layout(set = 1, binding = ...)
            // pSetLayouts[2] defines the layout for layout(set = 2, binding = ...)

            // We have to do this sEmptyLayout thing because:
            // The descriptor set layouts for a pipeline layout always start from set 0, so if a
            // shader only uses set 5, then you must create a pipeline layout with *5 descriptor sets*
            // Then it becomes: 0- sEmptyLayout; 1- sEmptyLayout; 2- sEmptyLayout; 3- sEmptyLayout; 4- sEmptyLayout; 5- TheRealLayoutFromTheShader
            // We can get the maximum number of set used in shader like this because std::map is already sorted in order
            if(!descriptorSetLayoutsMap.empty())
            {
                Uint lastKeyOfMap = (--descriptorSetLayoutsMap.end())->first;
                for(Uint i = 0; i <= lastKeyOfMap; i++)
                {
                    auto itr = descriptorSetLayoutsMap.find(i);
                    if(itr != descriptorSetLayoutsMap.end())
                        descriptorSetLayouts.push_back(itr->second); // Push TheRealLayoutFromTheShader
                    else
                        descriptorSetLayouts.push_back(sEmptyLayout); // Push sEmptyLayout
                }
            }

        } // End of "Mess Scope"
        // Social Credits 100+ for reading these comments 
        // Asscheek fucnkig Vulkan wasted my 5hr here

        return descriptorSetLayouts;
    }

    Surge::Vector<VkPushConstantRange> VulkanPipeline::CreatePushConstantRanges(const ShaderReflectionData& reflectedData)
    {
        const Vector<ShaderPushConstant>& pushConstants = reflectedData.GetPushConstantBuffers();
        // TODO: Remove this hardcoded 1 and make it dynamic based on the number of push constant buffers in the shader
        if (pushConstants.empty())
            return {};

        Vector<VkPushConstantRange> pushRanges(1);
        for(size_t i = 0; i < 1; ++i)
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
        return pushRanges;
    }

    Pair<VkVertexInputBindingDescription, Vector<VkVertexInputAttributeDescription>> VulkanPipeline::CreateVertexAttributes(const ShaderReflectionData& reflectedData)
    {
        if(reflectedData.GetStageInputs().find(ShaderType::VERTEX) == reflectedData.GetStageInputs().end())
            return {};

        const std::map<Uint, ShaderStageInput>& stageInputs = reflectedData.GetStageInputs().at(ShaderType::VERTEX);

        Uint stride = 0;
        for(const auto& [location, stageInput] : stageInputs)
            stride += stageInput.Size;

        VkVertexInputBindingDescription vertexBindingDescriptions {};
        Vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions(stageInputs.size());
        if(!stageInputs.empty()) // We need to support fullscreen quad/triangle with zero vertex input as well, so we can't assert that stage inputs must be present
        {
            vertexBindingDescriptions.binding = 0;
            vertexBindingDescriptions.stride = stride;
            vertexBindingDescriptions.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            for(Uint i = 0; i < stageInputs.size(); i++)
            {
                const ShaderStageInput& input = stageInputs.at(i);
                vertexAttributeDescriptions[i].binding = 0;
                vertexAttributeDescriptions[i].location = i;
                vertexAttributeDescriptions[i].format = VulkanUtils::ShaderDataTypeToVulkanFormat(input.DataType);
                vertexAttributeDescriptions[i].offset = input.Offset;
            }
        }

        return { vertexBindingDescriptions, vertexAttributeDescriptions };
    }

}
