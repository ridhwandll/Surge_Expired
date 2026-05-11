// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Core/Core.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanPipeline.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHI.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanUtils.hpp"
#include "Surge/Utility/Filesystem.hpp"

namespace Surge
{
	PipelineEntry VulkanPipeline::Create(VulkanRHI& rhi, const PipelineDesc& desc, VkRenderPass renderPass)
	{
		SG_ASSERT(renderPass != VK_NULL_HANDLE, "PipelineDesc: renderPass is null");

		const ShaderReflectionData& reflectedData = desc.Shader_.GetReflectionData();

		PipelineEntry entry = {};
		entry.Desc = desc;

		// Pipeline layout
		VkPipelineLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

		VkDescriptorSetLayout bindlessLayout = rhi.mBindlessRegistry.GetLayout();
		SG_ASSERT(bindlessLayout != VK_NULL_HANDLE, "VulkanPipeline: BindlessLayout not set");

		// WIP: Descriptor set layouts 
		//DescriptorLayoutDesc layoutDesc = {};
		//Uint descriptorSetCount = 1;
		//layoutDesc.BindingCount = 1;
		//for (const ShaderBuffer& buffer : reflectedData.GetBuffers())
		//{
		//	DescriptorBinding dsc{				
		//		.Slot = 0,
		//		.Type = DescriptorType::UNIFORM_BUFFER,
		//		.Count = 1,
		//		.Stage = buffer.ShaderStages,
		//		.Partial = false
		//	};
		//	layoutDesc.Bindings[buffer.Binding] = dsc;
		//}
		//
		//for (const ShaderResource& texture : reflectedData.GetResources())
		//{
		//	DescriptorBinding dsc{
		//		.Slot = 0,
		//		.Type = DescriptorType::TEXTURE,
		//		.Count = texture.ArraySize,
		//		.Stage = texture.ShaderStages,
		//		.Partial = false
		//	};
		//	layoutDesc.Bindings[texture.Binding] = dsc;
		//}
		//entry.DescriptorSetLayout = rhi.CreateDescriptorLayout(layoutDesc);		
		
		// Push constants
		const Vector<ShaderPushConstant>& pushConstants = reflectedData.GetPushConstantBuffers();
		Vector<VkPushConstantRange> pushRanges(pushConstants.size());
		for (size_t i = 0; i < pushConstants.size(); ++i)
		{
			const ShaderPushConstant& pushConstant = pushConstants[i];
			SG_ASSERT(!(pushConstant.ShaderStages & ShaderType::COMPUTE), "Compute shader is not supported in VulkanPipeline yet!");

			if (pushConstant.ShaderStages & ShaderType::VERTEX) pushRanges[i].stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
			if (pushConstant.ShaderStages & ShaderType::FRAGMENT) pushRanges[i].stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;

			pushRanges[i].offset = 0;
			pushRanges[i].size = pushConstant.Size;
		}

		//auto vklayout = rhi.mDescriptorLayoutPool.Get(entry.DescriptorSetLayout)->Layout;
		auto vkLayout = bindlessLayout;
		layoutInfo.pSetLayouts = &vkLayout;
		layoutInfo.setLayoutCount = 1;
		layoutInfo.pushConstantRangeCount = pushRanges.size();
		layoutInfo.pPushConstantRanges = pushRanges.data();

		VkDevice device = rhi.GetDevice();
		VK_CALL(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &entry.Layout));

		// Shaders
		VkShaderModule vertModule = VK_NULL_HANDLE;
		VkShaderModule fragModule = VK_NULL_HANDLE;
		VkPipelineShaderStageCreateInfo stages[2] = {};
		auto& spirvs = desc.Shader_.GetSPIRVs();
		for (const auto& spirv : spirvs)
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

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.pNext = nullptr;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<Uint>(vertexAttributeDescriptions.size());
		vertexInputInfo.pVertexBindingDescriptions = &vertexBindingDescriptions;
		vertexInputInfo.pVertexAttributeDescriptions = vertexAttributeDescriptions.data();

		// Input assembly
		VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VulkanUtils::ToVkTopology(desc.Raster.Topo);

		// Rasterizer
		VkPipelineRasterizationStateCreateInfo raster = {};
		raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		raster.polygonMode = VK_POLYGON_MODE_FILL;
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

		VkDynamicState dynamics[] =
		{
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamic = {};
		dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamic.dynamicStateCount = 2;
		dynamic.pDynamicStates = dynamics;

		// No MSAA on mobile
		VkPipelineMultisampleStateCreateInfo multisample = {};
		multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkGraphicsPipelineCreateInfo pipelineInfo = {};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = stages;
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
		if (desc.DebugName)
		{
			// Name the pipeline and layout for RenderDoc/validation layers
			VkDebugUtilsObjectNameInfoEXT nameInfo = {};
			nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
			nameInfo.objectType = VK_OBJECT_TYPE_PIPELINE;
			nameInfo.objectHandle = (uint64_t)entry.Pipeline;
			nameInfo.pObjectName = desc.DebugName;

		}
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
