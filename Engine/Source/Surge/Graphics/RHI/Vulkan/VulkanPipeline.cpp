// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Core/Core.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanPipeline.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHI.hpp"

#ifdef SURGE_PLATFORM_WINDOWS
#include <shaderc/shaderc.hpp>
#elif defined(SURGE_PLATFORM_ANDROID)
#include "Surge/Platform/Android/AndroidApp.hpp"
#include <android_native_app_glue.h>
#include <android/asset_manager.h>
#endif
#include "Surge/Utility/Filesystem.hpp"

namespace Surge
{
	static VkFormat ToVkFormat(VertexFormat f)
	{
		switch (f)
		{
		case VertexFormat::FLOAT:  return VK_FORMAT_R32_SFLOAT;
		case VertexFormat::FLOAT2: return VK_FORMAT_R32G32_SFLOAT;
		case VertexFormat::FLOAT3: return VK_FORMAT_R32G32B32_SFLOAT;
		case VertexFormat::FLOAT4: return VK_FORMAT_R32G32B32A32_SFLOAT;
		case VertexFormat::INT:    return VK_FORMAT_R32_SINT;
		case VertexFormat::INT2:   return VK_FORMAT_R32G32_SINT;
		case VertexFormat::INT3:   return VK_FORMAT_R32G32B32_SINT;
		case VertexFormat::INT4:   return VK_FORMAT_R32G32B32A32_SINT;
		}
		return VK_FORMAT_UNDEFINED;
	}

	static VkCullModeFlags ToVkCullMode(CullMode c)
	{
		switch (c)
		{
		case CullMode::NONE:  return VK_CULL_MODE_NONE;
		case CullMode::FRONT: return VK_CULL_MODE_FRONT_BIT;
		case CullMode::BACK:  return VK_CULL_MODE_BACK_BIT;
		}
		return VK_CULL_MODE_NONE;
	}

	static VkFrontFace ToVkFrontFace(FrontFace f)
	{
		switch (f)
		{
		case FrontFace::CLOCKWISE:         return VK_FRONT_FACE_CLOCKWISE;
		case FrontFace::COUNTER_CLOCKWISE: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
		}
		return VK_FRONT_FACE_CLOCKWISE;
	}

	static VkPrimitiveTopology ToVkTopology(Topology t)
	{
		switch (t)
		{
		case Topology::TRIANGLE_LIST:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		case Topology::TRIANGLE_STRIP: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		case Topology::LINE_LIST:      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		case Topology::POINT_LIST:     return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		}
		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	}

	static VkCompareOp ToVkCompareOp(CompareOp op)
	{
		switch (op)
		{
		case CompareOp::NEVER:            return VK_COMPARE_OP_NEVER;
		case CompareOp::LESS:             return VK_COMPARE_OP_LESS;
		case CompareOp::EQUAL:            return VK_COMPARE_OP_EQUAL;
		case CompareOp::LESS_OR_EQUAL:    return VK_COMPARE_OP_LESS_OR_EQUAL;
		case CompareOp::GREATER:          return VK_COMPARE_OP_GREATER;
		case CompareOp::NOT_EQUAL:        return VK_COMPARE_OP_NOT_EQUAL;
		case CompareOp::GREATER_OR_EQUAL: return VK_COMPARE_OP_GREATER_OR_EQUAL;
		case CompareOp::ALWAYS:           return VK_COMPARE_OP_ALWAYS;
		}
		return VK_COMPARE_OP_LESS;
	}

	static VkBlendFactor ToVkBlendFactor(BlendFactor f)
	{
		switch (f)
		{
		case BlendFactor::ZERO:                return VK_BLEND_FACTOR_ZERO;
		case BlendFactor::ONE:                 return VK_BLEND_FACTOR_ONE;
		case BlendFactor::SRC_ALPHA:           return VK_BLEND_FACTOR_SRC_ALPHA;
		case BlendFactor::ONE_MINUS_SRC_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		case BlendFactor::DST_ALPHA:           return VK_BLEND_FACTOR_DST_ALPHA;
		case BlendFactor::ONE_MINUS_DST_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
		}
		return VK_BLEND_FACTOR_ZERO;
	}

	static VkBlendOp ToVkBlendOp(BlendOp op)
	{
		switch (op)
		{
		case BlendOp::ADD:              return VK_BLEND_OP_ADD;
		case BlendOp::SUBTRACT:         return VK_BLEND_OP_SUBTRACT;
		case BlendOp::REVERSE_SUBTRACT: return VK_BLEND_OP_REVERSE_SUBTRACT;
		case BlendOp::MIN:              return VK_BLEND_OP_MIN;
		case BlendOp::MAX:              return VK_BLEND_OP_MAX;
		}
		return VK_BLEND_OP_ADD;
	}

	PipelineEntry VulkanPipeline::Create(const VulkanRHI& rhi, const PipelineDesc& desc, VkRenderPass renderPass)
	{
		SG_ASSERT(desc.VertShaderName, "PipelineDesc: VertShaderName is null!");
		SG_ASSERT(desc.FragShaderName, "PipelineDesc: FragShaderName is null!");
		SG_ASSERT(renderPass != VK_NULL_HANDLE, "PipelineDesc: renderPass is null");

		PipelineEntry entry = {};
		entry.PushConstantSize = desc.PushConstantSize;

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
		VkShaderModule vertModule = LoadShader(rhi, desc.VertShaderName, ShaderType::VERTEX);
		VkShaderModule fragModule = LoadShader(rhi, desc.FragShaderName, ShaderType::PIXEL);

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
			vkAttributes[i].format = ToVkFormat(desc.Attributes[i].Format);
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
		inputAssembly.topology = ToVkTopology(desc.Raster.Topo);

		// Rasterizer
		VkPipelineRasterizationStateCreateInfo raster = {};
		raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		raster.polygonMode = VK_POLYGON_MODE_FILL;
		raster.cullMode = ToVkCullMode(desc.Raster.Cull);
		raster.frontFace = ToVkFrontFace(desc.Raster.Front);
		raster.lineWidth = desc.Raster.LineWidth;
		raster.depthClampEnable = desc.Raster.DepthClamp ? VK_TRUE : VK_FALSE;

		// Depth stencil
		VkPipelineDepthStencilStateCreateInfo depthStencil = {};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = desc.Depth.TestEnable ? VK_TRUE : VK_FALSE;
		depthStencil.depthWriteEnable = desc.Depth.WriteEnable ? VK_TRUE : VK_FALSE;
		depthStencil.depthCompareOp = ToVkCompareOp(desc.Depth.Op);

		// Blend
		VkPipelineColorBlendAttachmentState blendAttachment = {};
		blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |VK_COLOR_COMPONENT_G_BIT |VK_COLOR_COMPONENT_B_BIT |VK_COLOR_COMPONENT_A_BIT;
		blendAttachment.blendEnable = desc.Blend.Enable ? VK_TRUE : VK_FALSE;
		blendAttachment.srcColorBlendFactor = ToVkBlendFactor(desc.Blend.SrcColor);
		blendAttachment.dstColorBlendFactor = ToVkBlendFactor(desc.Blend.DstColor);
		blendAttachment.colorBlendOp = ToVkBlendOp(desc.Blend.ColorOp);
		blendAttachment.srcAlphaBlendFactor = ToVkBlendFactor(desc.Blend.SrcAlpha);
		blendAttachment.dstAlphaBlendFactor = ToVkBlendFactor(desc.Blend.DstAlpha);
		blendAttachment.alphaBlendOp = ToVkBlendOp(desc.Blend.AlphaOp);

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

#ifdef SURGE_PLATFORM_WINDOWS
	static shaderc_shader_kind ShadercShaderKindFromSurgeShaderType(const ShaderType& type)
	{
		switch (type)
		{
		case ShaderType::VERTEX: return shaderc_glsl_vertex_shader;
		case ShaderType::PIXEL: return shaderc_glsl_fragment_shader;
		case ShaderType::COMPUTE: return shaderc_glsl_compute_shader;
		case ShaderType::NONE: SG_ASSERT_INTERNAL("ShaderType::NONE is invalid in this case!");
		}

		return static_cast<shaderc_shader_kind>(-1);
	}
#endif

	VkShaderModule VulkanPipeline::LoadShader(const VulkanRHI& rhi, const String& name, ShaderType stage)
	{
		Vector<Uint> SPIRV;
#ifdef SURGE_PLATFORM_WINDOWS
		String source = Filesystem::ReadFile<String>("Engine/Assets/Shaders/" + name);
		shaderc::Compiler compiler;
		shaderc::CompileOptions options;
		options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_1);
		shaderc::CompilationResult result = compiler.CompileGlslToSpv(source, ShadercShaderKindFromSurgeShaderType(stage), name.c_str(), options);
		if (result.GetCompilationStatus() != shaderc_compilation_status_success)
		{
			Log<Severity::Error>("Shader compilation failure!");
			Log<Severity::Error>("{} Error(s): \n{}", result.GetNumErrors(), result.GetErrorMessage());
			SG_ASSERT_INTERNAL("Shader Compilation failure!");
		}
		else
			SPIRV = Vector<Uint>(result.cbegin(), result.cend());

		String path = "Engine/Assets/Shaders/" + name + ".spv";
		FILE* f = nullptr;
		f = fopen(path.c_str(), "wb");
		if (f)
		{
			fwrite(SPIRV.data(), sizeof(Uint), SPIRV.size(), f);
			fclose(f);
			//Log<Severity::Info>("Cached Shader at: {0}", path);
		}

#elif defined(SURGE_PLATFORM_ANDROID)
		{
            android_app* app = Android::GAndroidApp;
			AAssetManager* assetManager = app->activity->assetManager;
			AAssetDir* assetDir = AAssetManager_openDir(assetManager, "Engine/Assets/Shaders"); // Path relative to assets folder

			std::string fullPath = "Engine/Assets/Shaders/" + name + ".spv";
			AAsset* asset = AAssetManager_open(assetManager, fullPath.c_str(), AASSET_MODE_BUFFER);

			// Read the buffer
			size_t size = AAsset_getLength(asset);
			if (size % 4 != 0)
			{
				Log<Severity::Error>("Shader %s size is not a multiple of 4!", name);
				AAsset_close(asset);
			}

			Vector<Uint> spirvCode(size / sizeof(Uint));
			AAsset_read(asset, spirvCode.data(), size);
			AAsset_close(asset);
			SPIRV = spirvCode;

		}
#endif

		VkDevice device = rhi.GetDevice();
		VkShaderModuleCreateInfo module_info{
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = SPIRV.size() * sizeof(Uint),
			.pCode = SPIRV.data() };

		VkShaderModule shaderModule;
		VK_CALL(vkCreateShaderModule(device, &module_info, nullptr, &shaderModule));
		return shaderModule;
	}
}
