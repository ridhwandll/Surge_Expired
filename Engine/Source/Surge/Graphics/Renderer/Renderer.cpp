// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Graphics/Camera/EditorCamera.hpp"
#include "Surge/Core/Core.hpp"
#include <exception>
#include "Surge/Utility/Filesystem.hpp"
#include <algorithm>

#ifdef SURGE_PLATFORM_WINDOWS
#include <shaderc/shaderc.hpp>
#elif defined(SURGE_PLATFORM_ANDROID)
#include <android_native_app_glue.h>
#include <android/asset_manager.h>
#include <android/log.h>
#endif

#define VK_CHECK(x)                                                                    \
    do                                                                                 \
    {                                                                                  \
        VkResult err = x;                                                              \
        if (err)                                                                       \
        {                                                                              \
            throw std::runtime_error("Detected Vulkan error");                         \
        }                                                                              \
    } while (0)

#define ASSERT_VK_HANDLE(handle)        \
    do                                  \
    {                                   \
        if ((handle) == VK_NULL_HANDLE) \
        {                               \
            LOGE("Handle is NULL");     \
            abort();                    \
        }                               \
    } while (0)

namespace Surge
{
    void Renderer::Initialize()
    {
        SURGE_PROFILE_FUNC("Renderer::Initialize()");
        mData = CreateScope<RendererData>();
		mRHI = CreateScope<GraphicsRHI>();
        mRHI->Initialize(Core::GetWindow());
        prepare();
    }

    void Renderer::BeginFrame(const RuntimeCamera& camera, const glm::mat4& transform)
    {
        SURGE_PROFILE_FUNC("Renderer::BeginFrame(Camera)");
        mData->ViewMatrix = glm::inverse(transform);
        mData->ProjectionMatrix = camera.GetProjectionMatrix();
        mData->ViewProjection = mData->ProjectionMatrix * mData->ViewMatrix;
        mData->CameraPosition = transform[3];
    }

    void Renderer::BeginFrame(const EditorCamera& camera)
    {
        SURGE_PROFILE_FUNC("Renderer::BeginFrame(EditorCamera)");
        mData->ViewMatrix = camera.GetViewMatrix();
        mData->ProjectionMatrix = camera.GetProjectionMatrix();
        mData->ViewProjection = mData->ProjectionMatrix * mData->ViewMatrix;
        mData->CameraPosition = camera.GetPosition();
    }

    void Renderer::EndFrame()
    {
        SURGE_PROFILE_FUNC("Renderer::EndFrame()");
        update();
        mData->DrawList.clear();
    }

    void Renderer::SetRenderArea(Uint width, Uint height) {}
    void Renderer::Shutdown()
    {
        SURGE_PROFILE_FUNC("Renderer::Shutdown()");
        VkDevice device = mRHI->GetBackendRHI().GetDevice();

        vkDeviceWaitIdle(device);
       
        if (context.pipeline != VK_NULL_HANDLE)        
            vkDestroyPipeline(device, context.pipeline, nullptr);
        
		if (context.pipeline_layout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(device, context.pipeline_layout, nullptr);
                
        mRHI->DestroyBuffer(mVertexBuffer);
        mRHI->DestroyBuffer(mIndexBuffer);
        mRHI->Shutdown();
    }

	const std::vector<Renderer::Vertex> vertices = {
		{{ 0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}}, // 0 — top right,    red
		{{ 0.5f,  0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}}, // 1 — bottom right, green
		{{-0.5f,  0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}, // 2 — bottom left,  blue
		{{-0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 0.0f}}, // 3 — top left,     yellow
	};

	const std::vector<uint32_t> indices = {
		0, 1, 2, // triangle 1
		0, 2, 3  // triangle 2
	};

    void Renderer::prepare()
    {        
        // Create the necessary objects for rendering
        init_pipeline();

		BufferDesc vbDesc = {};
		vbDesc.DebugName = "TriangleVB";
		vbDesc.HostVisible = true;
		vbDesc.Size = sizeof(vertices[0]) * vertices.size();
		vbDesc.Usage = BufferUsage::VERTEX;
		vbDesc.InitialData = vertices.data();
		mVertexBuffer = mRHI->CreateBuffer(vbDesc);

		BufferDesc ibDesc = {};
		ibDesc.Size = sizeof(Uint) * indices.size();
		ibDesc.Usage = BufferUsage::INDEX;
		ibDesc.HostVisible = true;
		ibDesc.InitialData = indices.data();
		ibDesc.DebugName = "TriangleIB";
		mIndexBuffer = mRHI->CreateBuffer(ibDesc);
    }

    void Renderer::update()
    {
		FrameContext ctx = mRHI->BeginFrame();
        render_triangle(ctx, ctx.SwapchainIndex);
        mRHI->EndFrame(ctx);
    }

    void Renderer::init_pipeline()
    {
        VkDevice device = mRHI->GetBackendRHI().GetDevice();
        // Create a blank pipeline layout.
        // We are not binding any resources to the pipeline in this first sample.
        VkPipelineLayoutCreateInfo layout_info {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        VK_CHECK(vkCreatePipelineLayout(device, &layout_info, nullptr, &context.pipeline_layout));

        // The Vertex input properties define the interface between the vertex buffer and the vertex shader.

        // Specify we will use triangle lists to draw geometry.
        VkPipelineInputAssemblyStateCreateInfo input_assembly {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

        // Define the vertex input binding.
        VkVertexInputBindingDescription binding_description {
            .binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};

        // Define the vertex input attribute.
        std::array<VkVertexInputAttributeDescription, 2> attribute_descriptions {
            {{.location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex, position)},
             {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, color)}}};

        // Define the pipeline vertex input.
        VkPipelineVertexInputStateCreateInfo vertex_input {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &binding_description,
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size()),
            .pVertexAttributeDescriptions = attribute_descriptions.data()};

        // Specify rasterization state.
        VkPipelineRasterizationStateCreateInfo raster {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_CLOCKWISE,
            .lineWidth = 1.0f};

        // Our attachment will write to all color channels, but no blending is enabled.
        VkPipelineColorBlendAttachmentState blend_attachment {
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

        VkPipelineColorBlendStateCreateInfo blend {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &blend_attachment};

        // We will have one viewport and scissor box.
        VkPipelineViewportStateCreateInfo viewport {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1};

        // Disable all depth testing.
        VkPipelineDepthStencilStateCreateInfo depth_stencil {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};

        // No multisampling.
        VkPipelineMultisampleStateCreateInfo multisample {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};

        // Specify that these states will be dynamic, i.e. not part of pipeline state object.
        std::array<VkDynamicState, 2> dynamics {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

        VkPipelineDynamicStateCreateInfo dynamic {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = static_cast<uint32_t>(dynamics.size()),
            .pDynamicStates = dynamics.data()};

        std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages {};

        // Vertex stage of the pipeline
        shader_stages[0] = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = load_shader_module("Triangle.vert", ShaderType::Vertex),
            .pName = "main"};

        // Fragment stage of the pipeline
        shader_stages[1] = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = load_shader_module("Triangle.frag", ShaderType::Pixel),
            .pName = "main"};

		VkRenderPass swapchainRenderpass = mRHI->GetBackendRHI().GetRenderPass();
        VkGraphicsPipelineCreateInfo pipe {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = static_cast<uint32_t>(shader_stages.size()),
            .pStages = shader_stages.data(),
            .pVertexInputState = &vertex_input,
            .pInputAssemblyState = &input_assembly,
            .pViewportState = &viewport,
            .pRasterizationState = &raster,
            .pMultisampleState = &multisample,
            .pDepthStencilState = &depth_stencil,
            .pColorBlendState = &blend,
            .pDynamicState = &dynamic,
            .layout = context.pipeline_layout,
            .renderPass = swapchainRenderpass
        };

        VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipe, nullptr, &context.pipeline));

        // Pipeline is baked, we can delete the shader modules now.
        
        vkDestroyShaderModule(device, shader_stages[0].module, nullptr);
        vkDestroyShaderModule(device, shader_stages[1].module, nullptr);
    }
#ifdef SURGE_PLATFORM_WINDOWS
    static shaderc_shader_kind ShadercShaderKindFromSurgeShaderType(const ShaderType& type)
    {
        switch (type)
        {
            case ShaderType::Vertex: return shaderc_glsl_vertex_shader;
            case ShaderType::Pixel: return shaderc_glsl_fragment_shader;
            case ShaderType::Compute: return shaderc_glsl_compute_shader;
            case ShaderType::None: SG_ASSERT_INTERNAL("ShaderType::None is invalid in this case!");
        }

        return static_cast<shaderc_shader_kind>(-1);
    }
#endif
    VkShaderModule Renderer::load_shader_module(const std::string& name, ShaderType stage)
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
            Log<Severity::Error>("{0} Shader compilation failure!");
            Log<Severity::Error>("{0} Error(s): \n{1}", result.GetNumErrors(), result.GetErrorMessage());
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
            Log<Severity::Info>("Cached Shader at: {0}", path);
        }
    
#elif defined(SURGE_PLATFORM_ANDROID)
        {
            auto options = Core::GetClient()->GeClientOptions();
            android_app* app = static_cast<android_app*>(options.AndroidApp);
            AAssetManager* assetManager = app->activity->assetManager;
            AAssetDir* assetDir = AAssetManager_openDir(assetManager, "Engine/Assets/Shaders"); // Path relative to assets folder

            std::string fullPath = "Engine/Assets/Shaders/" + name + ".spv";
            AAsset* asset = AAssetManager_open(assetManager, fullPath.c_str(), AASSET_MODE_BUFFER);

            // 3. Read the buffer
            size_t size = AAsset_getLength(asset);
            if (size % 4 != 0)
            {
                LOGE("Shader %s size is not a multiple of 4!", name);
                AAsset_close(asset);
            }

            Vector<Uint> spirvCode(size / sizeof(uint32_t));
            AAsset_read(asset, spirvCode.data(), size);
            AAsset_close(asset);
            SPIRV = spirvCode;

        }
#endif

        VkDevice device = mRHI->GetBackendRHI().GetDevice();
        VkShaderModuleCreateInfo module_info {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = SPIRV.size() * sizeof(uint32_t),
            .pCode = SPIRV.data()};

        VkShaderModule shader_module;
        VK_CHECK(vkCreateShaderModule(device, &module_info, nullptr, &shader_module));

        return shader_module;
    }

    void Renderer::render_triangle(const FrameContext& ctx, uint32_t swapchain_index)
    {
		VulkanRHI& backendRHI = mRHI->GetBackendRHI();
        VkDevice device = backendRHI.GetDevice();
		VulkanSwapchain& swapchain = backendRHI.GetSwapchain();
        const SwapchainFrame& frame = swapchain.GetFrame(swapchain_index);
        const SwapchainDimensions& swapchainDimensions = swapchain.GetDimensions();

		VkCommandBuffer cmd = backendRHI.GetFrame().GetCurrentFrame().CmdBuffer;

        // Render to this framebuffer.
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, context.pipeline);

		mRHI->CmdBindVertexBuffer(ctx, mVertexBuffer, 0);
        mRHI->CmdBindIndexBuffer(ctx, mIndexBuffer, 0);
        mRHI->CmdDrawIndexed(ctx, indices.size(), 1, 0, 0, 0);
    }

} // namespace Surge
