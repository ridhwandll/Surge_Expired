// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Core/Core.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanPipeline.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHI.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanUtils.hpp"
#include "Surge/Utility/Filesystem.hpp"

namespace Surge
{
	PipelineEntry VulkanPipeline::Create(const VulkanRHI& rhi, const PipelineDesc& desc, VkRenderPass renderPass)
	{
		SG_ASSERT(renderPass != VK_NULL_HANDLE, "PipelineDesc: renderPass is null");

		PipelineEntry entry = {};
		entry.Desc = desc;

		// Pipeline layout
		VkPipelineLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

		VkPushConstantRange pushRange = {};
		if (desc.PushConstantSize > 0)
		{
			pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
			pushRange.offset = 0;
			pushRange.size = desc.PushConstantSize;
			layoutInfo.pushConstantRangeCount = 1;
			layoutInfo.pPushConstantRanges = &pushRange;
		}

		VkDevice device = rhi.GetDevice();
		VK_CALL(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &entry.Layout));

		// Shaders
		VkShaderModule vertModule = VK_NULL_HANDLE;
		VkShaderModule fragModule = VK_NULL_HANDLE;
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
			}
			else if (spirv.Type == ShaderType::FRAGMENT)
			{
				moduleInfo.codeSize = spirv.SPIRV.size() * sizeof(Uint);
				moduleInfo.pCode = spirv.SPIRV.data();
				VK_CALL(vkCreateShaderModule(device, &moduleInfo, nullptr, &fragModule));
			}
		}
		SG_ASSERT(vertModule, "Vertex shader module is null!");
		SG_ASSERT(fragModule, "Fragment shader module is null!");

		VkPipelineShaderStageCreateInfo stages[2] = {};
		stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		stages[0].module = vertModule;
		stages[0].pName = "main";

		stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages[1].module = fragModule;
		stages[1].pName = "main";

		// Vertex input
		VkVertexInputBindingDescription vkBindings[4] = {};
		VkVertexInputAttributeDescription vkAttributes[8] = {};

		for (uint32_t i = 0; i < desc.BindingCount; i++)
		{
			vkBindings[i].binding = desc.Bindings[i].Binding;
			vkBindings[i].stride = desc.Bindings[i].Stride;
			vkBindings[i].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		}

		for (uint32_t i = 0; i < desc.AttributeCount; i++)
		{
			vkAttributes[i].location = desc.Attributes[i].Location;
			vkAttributes[i].binding = desc.Attributes[i].Binding;
			vkAttributes[i].format = VulkanUtils::ToVkVertexFormat(desc.Attributes[i].Format);
			vkAttributes[i].offset = desc.Attributes[i].Offset;
		}

		VkPipelineVertexInputStateCreateInfo vertexInput = {};
		vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInput.vertexBindingDescriptionCount = desc.BindingCount;
		vertexInput.pVertexBindingDescriptions = vkBindings;
		vertexInput.vertexAttributeDescriptionCount = desc.AttributeCount;
		vertexInput.pVertexAttributeDescriptions = vkAttributes;

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

		VkDynamicState dynamics[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamic = {};
		dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamic.dynamicStateCount = 2;
		dynamic.pDynamicStates = dynamics;

		// Multisample (no MSAA on mobile)
		VkPipelineMultisampleStateCreateInfo multisample = {};
		multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		// Assemble
		VkGraphicsPipelineCreateInfo pipelineInfo = {};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = stages;
		pipelineInfo.pVertexInputState = &vertexInput;
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

	void VulkanPipeline::Destroy(const VulkanRHI& rhi, PipelineEntry& entry)
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
	}

// 	VkShaderModule VulkanPipeline::LoadShader(const VulkanRHI& rhi, const String& name, ShaderType stage)
// 	{
// 		Vector<Uint> SPIRV;
// #ifdef SURGE_PLATFORM_WINDOWS
// 		String source = Filesystem::ReadFile<String>("Engine/Assets/Shaders/" + name);
// 		shaderc::Compiler compiler;
// 		shaderc::CompileOptions options;
// 		options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_1);
// 		shaderc::CompilationResult result = compiler.CompileGlslToSpv(source, ShadercShaderKindFromSurgeShaderType(stage), name.c_str(), options);
// 		if (result.GetCompilationStatus() != shaderc_compilation_status_success)
// 		{
// 			Log<Severity::Error>("Shader compilation failure!");
// 			Log<Severity::Error>("{} Error(s): \n{}", result.GetNumErrors(), result.GetErrorMessage());
// 			SG_ASSERT_INTERNAL("Shader Compilation failure!");
// 		}
// 		else
// 			SPIRV = Vector<Uint>(result.cbegin(), result.cend());
// 
// 		String path = "Engine/Assets/Shaders/" + name + ".spv";
// 		FILE* f = nullptr;
// 		f = fopen(path.c_str(), "wb");
// 		if (f)
// 		{
// 			fwrite(SPIRV.data(), sizeof(Uint), SPIRV.size(), f);
// 			fclose(f);
// 			//Log<Severity::Info>("Cached Shader at: {0}", path);
// 		}
// 
// #elif defined(SURGE_PLATFORM_ANDROID)
// 		{
//             android_app* app = Android::GAndroidApp;
// 			AAssetManager* assetManager = app->activity->assetManager;
// 			AAssetDir* assetDir = AAssetManager_openDir(assetManager, "Engine/Assets/Shaders"); // Path relative to assets folder
// 
// 			std::string fullPath = "Engine/Assets/Shaders/" + name + ".spv";
// 			AAsset* asset = AAssetManager_open(assetManager, fullPath.c_str(), AASSET_MODE_BUFFER);
// 
// 			// Read the buffer
// 			size_t size = AAsset_getLength(asset);
// 			if (size % 4 != 0)
// 			{
// 				Log<Severity::Error>("Shader %s size is not a multiple of 4!", name);
// 				AAsset_close(asset);
// 			}
// 
// 			Vector<Uint> spirvCode(size / sizeof(Uint));
// 			AAsset_read(asset, spirvCode.data(), size);
// 			AAsset_close(asset);
// 			SPIRV = spirvCode;
// 
// 		}
// #endif
// 
// 		VkDevice device = rhi.GetDevice();
// 		VkShaderModuleCreateInfo module_info{
// 			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
// 			.codeSize = SPIRV.size() * sizeof(Uint),
// 			.pCode = SPIRV.data() };
// 
// 		VkShaderModule shaderModule;
// 		VK_CALL(vkCreateShaderModule(device, &module_info, nullptr, &shaderModule));
// 		return shaderModule;
// 	}
}
