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

        if (device != VK_NULL_HANDLE)
        {
            vkDeviceWaitIdle(device);
        }

        for (auto& framebuffer : context.swapchain_framebuffers)
        {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }

        for (auto& per_frame : context.per_frame)
        {
            teardown_per_frame(per_frame);
        }

        context.per_frame.clear();

        for (auto semaphore : context.recycled_semaphores)
        {
            vkDestroySemaphore(device, semaphore, nullptr);
        }

        if (context.pipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(device, context.pipeline, nullptr);
        }

        if (context.pipeline_layout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(device, context.pipeline_layout, nullptr);
        }

        if (context.render_pass != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(device, context.render_pass, nullptr);
        }

        for (VkImageView image_view : context.swapchain_image_views)
        {
            vkDestroyImageView(device, image_view, nullptr);
        }

        if (context.swapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(device, context.swapchain, nullptr);
        }

		VmaAllocator allocator = mRHI->GetBackendRHI().GetAllocator();
        if (vertex_buffer_allocation != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(allocator, vertex_buffer, vertex_buffer_allocation);
        }
        mRHI->Shutdown();
    }

    void Renderer::SubmitPointLight(const PointLightComponent& pointLight, const glm::vec3& position) {}
    void Renderer::SubmitDirectionalLight(const DirectionalLightComponent& dirLight, const glm::vec3& direction) {}

    void Renderer::prepare()
    {        
        Window* window = Core::GetWindow();
        glm::vec2 extent = window->GetSize();
        context.swapchain_dimensions.width = extent.x;
        context.swapchain_dimensions.height = extent.y;

        init_vertex_buffer();
        init_swapchain();

        // Create the necessary objects for rendering
        init_render_pass();
        init_pipeline();
        init_framebuffers();
    }

    void Renderer::update()
    {
        uint32_t index;

        auto res = acquire_next_image(&index);

        // Handle outdated error in acquire.
        if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR)
        {
            resize(context.swapchain_dimensions.width, context.swapchain_dimensions.height);
            res = acquire_next_image(&index);
        }

        if (res != VK_SUCCESS)
        {
            vkQueueWaitIdle(mRHI->GetBackendRHI().GetQueue());
            return;
        }

        render_triangle(index);
        res = present_image(index);

        // Handle Outdated error in present.
        if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR)
        {
            resize(context.swapchain_dimensions.width, context.swapchain_dimensions.height);
        }
        else if (res != VK_SUCCESS)
        {
            LOGE("Failed to present swapchain image.");
        }
    }

    bool Renderer::resize(const uint32_t width, const uint32_t height)
    {
		VkDevice device = mRHI->GetBackendRHI().GetDevice();
		VkPhysicalDevice gpu = mRHI->GetBackendRHI().GetGPU();
        VkSurfaceKHR surface = mRHI->GetBackendRHI().GetSurface();

        if (device == VK_NULL_HANDLE)
        {
            return false;
        }

        VkSurfaceCapabilitiesKHR surface_properties;
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &surface_properties));

        // Only rebuild the swapchain if the dimensions have changed
        if (surface_properties.currentExtent.width == context.swapchain_dimensions.width &&
            surface_properties.currentExtent.height == context.swapchain_dimensions.height)
        {
            return false;
        }

        vkDeviceWaitIdle(device);

        for (auto& framebuffer : context.swapchain_framebuffers)
        {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }

        init_swapchain();
        init_framebuffers();
        return true;
    }

    void Renderer::init_vertex_buffer()
    {
        // Vertex data for a single colored triangle
        const std::vector<Vertex> vertices = {
            {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
            {{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}};

        const VkDeviceSize buffer_size = sizeof(vertices[0]) * vertices.size();

        // Copy Vertex data to a buffer accessible by the device
        VkBufferCreateInfo buffer_info {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = buffer_size,
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT};

        // We use the Vulkan Memory Allocator to find a memory type that can be written and mapped from the host
        // On most setups this will return a memory type that resides in VRAM and is accessible from the host
        VmaAllocationCreateInfo buffer_alloc_ci {
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
            .requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

		VmaAllocator allocator = mRHI->GetBackendRHI().GetAllocator();
        VmaAllocationInfo buffer_alloc_info {};
        vmaCreateBuffer(allocator, &buffer_info, &buffer_alloc_ci, &vertex_buffer, &vertex_buffer_allocation, &buffer_alloc_info);
        if (buffer_alloc_info.pMappedData)        
            memcpy(buffer_alloc_info.pMappedData, vertices.data(), buffer_size);        
        else        
            throw std::runtime_error("Could not map vertex buffer.");
        
    }

    void Renderer::init_per_frame(PerFrame& per_frame)
    {
		VkDevice device = mRHI->GetBackendRHI().GetDevice();
        int32_t graphicsQueueIndex = mRHI->GetBackendRHI().GetQueueIndex();

        VkFenceCreateInfo info {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT};
        VK_CHECK(vkCreateFence(device, &info, nullptr, &per_frame.queue_submit_fence));

        VkCommandPoolCreateInfo cmd_pool_info {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            .queueFamilyIndex = static_cast<Uint>(graphicsQueueIndex)};
        VK_CHECK(vkCreateCommandPool(device, &cmd_pool_info, nullptr, &per_frame.primary_command_pool));

        VkCommandBufferAllocateInfo cmd_buf_info {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = per_frame.primary_command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1};
        VK_CHECK(vkAllocateCommandBuffers(device, &cmd_buf_info, &per_frame.primary_command_buffer));
    }

    void Renderer::teardown_per_frame(PerFrame& per_frame)
    {
		VkDevice device = mRHI->GetBackendRHI().GetDevice();

        if (per_frame.queue_submit_fence != VK_NULL_HANDLE)
        {
            vkDestroyFence(device, per_frame.queue_submit_fence, nullptr);

            per_frame.queue_submit_fence = VK_NULL_HANDLE;
        }

        if (per_frame.primary_command_buffer != VK_NULL_HANDLE)
        {
            vkFreeCommandBuffers(device, per_frame.primary_command_pool, 1, &per_frame.primary_command_buffer);

            per_frame.primary_command_buffer = VK_NULL_HANDLE;
        }

        if (per_frame.primary_command_pool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(device, per_frame.primary_command_pool, nullptr);

            per_frame.primary_command_pool = VK_NULL_HANDLE;
        }

        if (per_frame.swapchain_acquire_semaphore != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(device, per_frame.swapchain_acquire_semaphore, nullptr);

            per_frame.swapchain_acquire_semaphore = VK_NULL_HANDLE;
        }

        if (per_frame.swapchain_release_semaphore != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(device, per_frame.swapchain_release_semaphore, nullptr);

            per_frame.swapchain_release_semaphore = VK_NULL_HANDLE;
        }
    }

    static VkSurfaceFormatKHR select_surface_format(VkPhysicalDevice gpu, VkSurfaceKHR surface, std::vector<VkFormat> const& preferred_formats = {VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_A8B8G8R8_SRGB_PACK32})
    {
        uint32_t surface_format_count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &surface_format_count, nullptr);
        assert(0 < surface_format_count);
        std::vector<VkSurfaceFormatKHR> supported_surface_formats(surface_format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &surface_format_count, supported_surface_formats.data());

        auto it = std::ranges::find_if(supported_surface_formats,
                                       [&preferred_formats](VkSurfaceFormatKHR surface_format) {
                                           return std::ranges::any_of(preferred_formats,
                                                                      [&surface_format](VkFormat format) { return format == surface_format.format; });
                                       });

        // We use the first supported format as a fallback in case none of the preferred formats is available
        return it != supported_surface_formats.end() ? *it : supported_surface_formats[0];
    }

    void Renderer::init_swapchain()
    {
        VkDevice device = mRHI->GetBackendRHI().GetDevice();
		VkPhysicalDevice gpu = mRHI->GetBackendRHI().GetGPU();
		VkSurfaceKHR surface = mRHI->GetBackendRHI().GetSurface();

        VkSurfaceCapabilitiesKHR surface_properties;
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &surface_properties));

        VkSurfaceFormatKHR format = select_surface_format(gpu, surface);

        VkExtent2D swapchain_size {};
        if (surface_properties.currentExtent.width == 0xFFFFFFFF)
        {
            swapchain_size.width = context.swapchain_dimensions.width;
            swapchain_size.height = context.swapchain_dimensions.height;
        }
        else
        {
            swapchain_size = surface_properties.currentExtent;
        }

        // FIFO must be supported by all implementations.
        VkPresentModeKHR swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR;

        // Determine the number of VkImage's to use in the swapchain.
        // Ideally, we desire to own 1 image at a time, the rest of the images can
        // either be rendered to and/or being queued up for display.
        uint32_t desired_swapchain_images = surface_properties.minImageCount + 1;
        if ((surface_properties.maxImageCount > 0) && (desired_swapchain_images > surface_properties.maxImageCount))
        {
            // Application must settle for fewer images than desired.
            desired_swapchain_images = surface_properties.maxImageCount;
        }

        // Figure out a suitable surface transform.
        VkSurfaceTransformFlagBitsKHR pre_transform;
        if (surface_properties.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
        {
            pre_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        }
        else
        {
            pre_transform = surface_properties.currentTransform;
        }

        VkSwapchainKHR old_swapchain = context.swapchain;

        // Find a supported composite type.
        VkCompositeAlphaFlagBitsKHR composite = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        if (surface_properties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
        {
            composite = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        }
        else if (surface_properties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
        {
            composite = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
        }
        else if (surface_properties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
        {
            composite = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
        }
        else if (surface_properties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
        {
            composite = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
        }

        VkSwapchainCreateInfoKHR info {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = surface,
            .minImageCount = desired_swapchain_images,
            .imageFormat = format.format,
            .imageColorSpace = format.colorSpace,
            .imageExtent = swapchain_size,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .preTransform = pre_transform,
            .compositeAlpha = composite,
            .presentMode = swapchain_present_mode,
            .clipped = true,
            .oldSwapchain = old_swapchain};

        VK_CHECK(vkCreateSwapchainKHR(device, &info, nullptr, &context.swapchain));

        if (old_swapchain != VK_NULL_HANDLE)
        {
            for (VkImageView image_view : context.swapchain_image_views)
            {
                vkDestroyImageView(device, image_view, nullptr);
            }

            for (auto& per_frame : context.per_frame)
            {
                teardown_per_frame(per_frame);
            }

            context.swapchain_image_views.clear();

            vkDestroySwapchainKHR(device, old_swapchain, nullptr);
        }

        context.swapchain_dimensions = {swapchain_size.width, swapchain_size.height, format.format};

        uint32_t image_count;
        VK_CHECK(vkGetSwapchainImagesKHR(device, context.swapchain, &image_count, nullptr));

        /// The swapchain images.
        std::vector<VkImage> swapchain_images(image_count);
        VK_CHECK(vkGetSwapchainImagesKHR(device, context.swapchain, &image_count, swapchain_images.data()));

        // Initialize per-frame resources.
        // Every swapchain image has its own command pool and fence manager.
        // This makes it very easy to keep track of when we can reset command buffers and such.
        context.per_frame.clear();
        context.per_frame.resize(image_count);

        for (size_t i = 0; i < image_count; i++)
        {
            init_per_frame(context.per_frame[i]);
        }

        for (size_t i = 0; i < image_count; i++)
        {
            // Create an image view which we can render into.
            VkImageViewCreateInfo view_info {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = swapchain_images[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = context.swapchain_dimensions.format,
                .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}};

            VkImageView image_view;
            VK_CHECK(vkCreateImageView(device, &view_info, nullptr, &image_view));

            context.swapchain_image_views.push_back(image_view);
        }
    }

    void Renderer::init_render_pass()
    {
        VkDevice device = mRHI->GetBackendRHI().GetDevice();
        VkAttachmentDescription attachment {
            .format = context.swapchain_dimensions.format,      // Backbuffer format.
            .samples = VK_SAMPLE_COUNT_1_BIT,                   // Not multisampled.
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,              // When starting the frame, we want tiles to be cleared.
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,            // When ending the frame, we want tiles to be written out.
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,   // Don't care about stencil since we're not using it.
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, // Don't care about stencil since we're not using it.
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,         // The image layout will be undefined when the render pass begins.
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR      // After the render pass is complete, we will transition to PRESENT_SRC_KHR layout.
        };

        // We have one subpass. This subpass has one color attachment.
        // While executing this subpass, the attachment will be in attachment optimal layout.
        VkAttachmentReference color_ref = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

        // We will end up with two transitions.
        // The first one happens right before we start subpass #0, where
        // UNDEFINED is transitioned into COLOR_ATTACHMENT_OPTIMAL.
        // The final layout in the render pass attachment states PRESENT_SRC_KHR, so we
        // will get a final transition from COLOR_ATTACHMENT_OPTIMAL to PRESENT_SRC_KHR.
        VkSubpassDescription subpass {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &color_ref,
        };

        // Create a dependency to external events.
        // We need to wait for the WSI semaphore to signal.
        // Only pipeline stages which depend on COLOR_ATTACHMENT_OUTPUT_BIT will
        // actually wait for the semaphore, so we must also wait for that pipeline stage.
        VkSubpassDependency dependency {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

        // Since we changed the image layout, we need to make the memory visible to
        // color attachment to modify.
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        // Finally, create the renderpass.
        VkRenderPassCreateInfo rp_info {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &attachment,
            .subpassCount = 1,
            .pSubpasses = &subpass,
            .dependencyCount = 1,
            .pDependencies = &dependency};

        VK_CHECK(vkCreateRenderPass(device, &rp_info, nullptr, &context.render_pass));
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
            .layout = context.pipeline_layout, // We need to specify the pipeline layout up front
            .renderPass = context.render_pass  // We need to specify the render pass up front
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

    VkResult Renderer::acquire_next_image(uint32_t* image)
    {
        VkDevice device = mRHI->GetBackendRHI().GetDevice();
        VkSemaphore acquire_semaphore;
        if (context.recycled_semaphores.empty())
        {
            VkSemaphoreCreateInfo info = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
            VK_CHECK(vkCreateSemaphore(device, &info, nullptr, &acquire_semaphore));
        }
        else
        {
            acquire_semaphore = context.recycled_semaphores.back();
            context.recycled_semaphores.pop_back();
        }

        VkResult res = vkAcquireNextImageKHR(device, context.swapchain, UINT64_MAX, acquire_semaphore, VK_NULL_HANDLE, image);
        if (res != VK_SUCCESS)
        {
            context.recycled_semaphores.push_back(acquire_semaphore);
            return res;
        }

        // If we have outstanding fences for this swapchain image, wait for them to complete first.
        // After begin frame returns, it is safe to reuse or delete resources which
        // were used previously.
        //
        // We wait for fences which completes N frames earlier, so we do not stall,
        // waiting for all GPU work to complete before this returns.
        // Normally, this doesn't really block at all,
        // since we're waiting for old frames to have been completed, but just in case.
        if (context.per_frame[*image].queue_submit_fence != VK_NULL_HANDLE)
        {
            vkWaitForFences(device, 1, &context.per_frame[*image].queue_submit_fence, true, UINT64_MAX);
            vkResetFences(device, 1, &context.per_frame[*image].queue_submit_fence);
        }

        if (context.per_frame[*image].primary_command_pool != VK_NULL_HANDLE)
        {
            vkResetCommandPool(device, context.per_frame[*image].primary_command_pool, 0);
        }

        // Recycle the old semaphore back into the semaphore manager.
        VkSemaphore old_semaphore = context.per_frame[*image].swapchain_acquire_semaphore;

        if (old_semaphore != VK_NULL_HANDLE)
        {
            context.recycled_semaphores.push_back(old_semaphore);
        }

        context.per_frame[*image].swapchain_acquire_semaphore = acquire_semaphore;

        return VK_SUCCESS;
    }

    void Renderer::render_triangle(uint32_t swapchain_index)
    {
        VkDevice device = mRHI->GetBackendRHI().GetDevice();
        // Render to this framebuffer.
        VkFramebuffer framebuffer = context.swapchain_framebuffers[swapchain_index];

        // Allocate or re-use a primary command buffer.
        VkCommandBuffer cmd = context.per_frame[swapchain_index].primary_command_buffer;

        // We will only submit this once before it's recycled.
        VkCommandBufferBeginInfo begin_info {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
        // Begin command recording
        vkBeginCommandBuffer(cmd, &begin_info);

        // Set clear color values.
        VkClearValue clear_value {
            .color = {{0.01f, 0.01f, 0.01f, 1.0f}}};

        // Begin the render pass.
        VkRenderPassBeginInfo rp_begin {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = context.render_pass,
            .framebuffer = framebuffer,
            .renderArea = {.extent = {.width = context.swapchain_dimensions.width, .height = context.swapchain_dimensions.height}},
            .clearValueCount = 1,
            .pClearValues = &clear_value};
        // We will add draw commands in the same command buffer.
        vkCmdBeginRenderPass(cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

        // Bind the graphics pipeline.
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, context.pipeline);

        VkViewport vp {
            .width = static_cast<float>(context.swapchain_dimensions.width),
            .height = static_cast<float>(context.swapchain_dimensions.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f};
        // Set viewport dynamically
        vkCmdSetViewport(cmd, 0, 1, &vp);

        VkRect2D scissor {
            .extent = {.width = context.swapchain_dimensions.width, .height = context.swapchain_dimensions.height}};
        // Set scissor dynamically
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // Bind the vertex buffer to source the draw calls from.
        VkDeviceSize offset = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, &vertex_buffer, &offset);

        // Draw three vertices with one instance from the currently bound vertex bound.
        vkCmdDraw(cmd, 3, 1, 0, 0);

        // Complete render pass.
        vkCmdEndRenderPass(cmd);

        // Complete the command buffer.
        VK_CHECK(vkEndCommandBuffer(cmd));

        // Submit it to the queue with a release semaphore.
        if (context.per_frame[swapchain_index].swapchain_release_semaphore == VK_NULL_HANDLE)
        {
            VkSemaphoreCreateInfo semaphore_info {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
            VK_CHECK(vkCreateSemaphore(device, &semaphore_info, nullptr, &context.per_frame[swapchain_index].swapchain_release_semaphore));
        }

        VkPipelineStageFlags wait_stage {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

        VkQueue queue = mRHI->GetBackendRHI().GetQueue();
        VkSubmitInfo info {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &context.per_frame[swapchain_index].swapchain_acquire_semaphore,
            .pWaitDstStageMask = &wait_stage,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmd,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &context.per_frame[swapchain_index].swapchain_release_semaphore};
        // Submit command buffer to graphics queue
        VK_CHECK(vkQueueSubmit(queue, 1, &info, context.per_frame[swapchain_index].queue_submit_fence));
    }

    VkResult Renderer::present_image(uint32_t index)
    {
        VkQueue queue = mRHI->GetBackendRHI().GetQueue();
        VkPresentInfoKHR present {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &context.per_frame[index].swapchain_release_semaphore,
            .swapchainCount = 1,
            .pSwapchains = &context.swapchain,
            .pImageIndices = &index,
        };
        // Present swapchain image
        return vkQueuePresentKHR(queue, &present);
    }

    void Renderer::init_framebuffers()
    {
        VkDevice device = mRHI->GetBackendRHI().GetDevice();
        context.swapchain_framebuffers.clear();

        // Create framebuffer for each swapchain image view
        for (auto& image_view : context.swapchain_image_views)
        {
            // Build the framebuffer.
            VkFramebufferCreateInfo fb_info {
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass = context.render_pass,
                .attachmentCount = 1,
                .pAttachments = &image_view,
                .width = context.swapchain_dimensions.width,
                .height = context.swapchain_dimensions.height,
                .layers = 1};

            VkFramebuffer framebuffer;
            VK_CHECK(vkCreateFramebuffer(device, &fb_info, nullptr, &framebuffer));

            context.swapchain_framebuffers.push_back(framebuffer);
        }
    }

} // namespace Surge
