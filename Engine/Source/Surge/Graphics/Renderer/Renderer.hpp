// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Memory.hpp"
#include "Surge/ECS/Components.hpp"
#include "Lights.hpp"
#include "Surge/Graphics/RHI/RHI.hpp"

#define FRAMES_IN_FLIGHT 2
#define BASE_SHADER_PATH "Engine/Assets/Shaders" //Sadkek, we don't have an asset manager yet

#include <volk.h>
#include <vk_mem_alloc.h>

namespace Surge
{
	enum class ShaderType
	{
		None = 0,
		Vertex = BIT(0),
		Pixel = BIT(1),
		Compute = BIT(2)
	};
	MAKE_BIT_ENUM(ShaderType, uint16_t)

    struct DrawCommand
    {
		//DrawCommand(MeshComponent* meshComp, const glm::mat4& transform)
		//    : MeshComp(meshComp), Transform(transform) {}

        //MeshComponent* MeshComp;
        glm::mat4 Transform;
    };

    class SURGE_API Scene;
    struct RendererData
    {
        Vector<DrawCommand> DrawList;
        Scene* SceneContext;

        // Camera
        glm::vec3 CameraPosition;
        glm::mat4 ViewMatrix;
        glm::mat4 ProjectionMatrix;
        glm::mat4 ViewProjection;
    };

    class EditorCamera;
    class SURGE_API Renderer
    {
    public:
        Renderer() = default;
        ~Renderer() = default;

        void Initialize();
        void Shutdown();

        void BeginFrame(const RuntimeCamera& camera, const glm::mat4& transform);
        void BeginFrame(const EditorCamera& camera);
        void EndFrame();
        void SetRenderArea(Uint width, Uint height);

        //FORCEINLINE void SubmitMesh(MeshComponent& meshComp, const glm::mat4& transform) { mData->DrawList.push_back(DrawCommand(&meshComp, transform)); }
        void SubmitPointLight(const PointLightComponent& pointLight, const glm::vec3& position);
        void SubmitDirectionalLight(const DirectionalLightComponent& dirLight, const glm::vec3& direction);

        RendererData* GetData() { return mData.get(); }
        void SetSceneContext(Ref<Scene>& scene) { mData->SceneContext = scene.Raw(); }

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // RENDERER 2026
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    private:
        struct SwapchainDimensions
        {
            uint32_t width = 0;
            uint32_t height = 0;            
            VkFormat format = VK_FORMAT_UNDEFINED; // Pixel format of the swapchain.
        };
        struct PerFrame
        {
            VkFence queue_submit_fence = VK_NULL_HANDLE;
            VkCommandPool primary_command_pool = VK_NULL_HANDLE;
            VkCommandBuffer primary_command_buffer = VK_NULL_HANDLE;
            VkSemaphore swapchain_acquire_semaphore = VK_NULL_HANDLE;
            VkSemaphore swapchain_release_semaphore = VK_NULL_HANDLE;
        };
        struct Context
        {
            VkSwapchainKHR swapchain = VK_NULL_HANDLE;
            SwapchainDimensions swapchain_dimensions;

            std::vector<VkImageView> swapchain_image_views;
            std::vector<VkFramebuffer> swapchain_framebuffers;

            /// The renderpass description.
            VkRenderPass render_pass = VK_NULL_HANDLE;

            /// The graphics pipeline.
            VkPipeline pipeline = VK_NULL_HANDLE;

            /**
             * The pipeline layout for resources.
             * Not used in this sample, but we still need to provide a dummy one.
             */
            VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;

            /// A set of semaphores that can be reused.
            std::vector<VkSemaphore> recycled_semaphores;

            /// A set of per-frame data.
            std::vector<PerFrame> per_frame;
        };

        struct Vertex
        {
            glm::vec3 position;
            glm::vec3 color;
        };

        VkBuffer vertex_buffer = VK_NULL_HANDLE;
        VkDeviceMemory vertex_buffer_memory = VK_NULL_HANDLE;
        VmaAllocation vertex_buffer_allocation = VK_NULL_HANDLE;

        void prepare();
        void update();
        bool resize(const uint32_t width, const uint32_t height);

        void init_vertex_buffer();
        void init_per_frame(PerFrame& per_frame);
        void teardown_per_frame(PerFrame& per_frame);
        void init_swapchain();
        void init_render_pass();
        void init_pipeline();

        VkShaderModule load_shader_module(const std::string& path, ShaderType stage);
        VkResult acquire_next_image(uint32_t* image);
        void render_triangle(uint32_t swapchain_index);
        VkResult present_image(uint32_t index);
        void init_framebuffers();

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    private:
        Context context;
        Scope<GraphicsRHI> mRHI;
        Scope<RendererData> mData;
    };
} // namespace Surge