// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/Core/HandlePool.hpp"
#include "Surge/Graphics/RHI/RHIResources.hpp"
#include <volk.h>

// Forward-declare VMA handle types so the full vk_mem_alloc.h header is only
// pulled in by .cpp files (avoids the heavy include in every translation unit).
typedef struct VmaAllocator_T*  VmaAllocator;
typedef struct VmaAllocation_T* VmaAllocation;

// ---------------------------------------------------------------------------
// VulkanResources.hpp
//   Concrete Vulkan backing structs for every RHI resource kind.
//   Each type gets its own HandlePool inside VulkanResourceRegistry.
//   ALL Vulkan backend code obtains resources through GetVulkanRegistry().
// ---------------------------------------------------------------------------

namespace Surge::RHI
{
    // ── Backing structs ──────────────────────────────────────────────────────

    struct VulkanBuffer
    {
        VkBuffer       Buffer     = VK_NULL_HANDLE;
        VmaAllocation  Allocation = nullptr;
        VkDeviceSize   Size       = 0;
        BufferUsage    Usage      = BufferUsage::None;
        GPUMemoryUsage MemUsage   = GPUMemoryUsage::GPUOnly;
    };

    struct VulkanTexture
    {
        VkImage       Image      = VK_NULL_HANDLE;
        VkImageView   ImageView  = VK_NULL_HANDLE;
        VkSampler     Sampler    = VK_NULL_HANDLE;
        VmaAllocation Allocation = nullptr;
        TextureDesc   Desc       = {};
        VkImageLayout Layout     = VK_IMAGE_LAYOUT_UNDEFINED;
    };

    struct VulkanSampler
    {
        VkSampler   Sampler = VK_NULL_HANDLE;
        SamplerDesc Desc    = {};
    };

    struct VulkanShader
    {
        VkShaderModule Module = VK_NULL_HANDLE;
        ShaderStage    Stage  = ShaderStage::Vertex;
    };

    struct VulkanGraphicsPipeline
    {
        VkPipeline       Pipeline = VK_NULL_HANDLE;
        VkPipelineLayout Layout   = VK_NULL_HANDLE;
    };

    struct VulkanComputePipeline
    {
        VkPipeline       Pipeline = VK_NULL_HANDLE;
        VkPipelineLayout Layout   = VK_NULL_HANDLE;
    };

    struct VulkanRenderPass
    {
        VkRenderPass RenderPass = VK_NULL_HANDLE;
    };

    struct VulkanFramebuffer
    {
        VkFramebuffer Framebuffer = VK_NULL_HANDLE;
        VkRenderPass  RenderPass  = VK_NULL_HANDLE; // back-reference for commands
        uint32_t      Width       = 0;
        uint32_t      Height      = 0;
    };

    struct VulkanCommandBuffer
    {
        VkCommandBuffer CmdBuf    = VK_NULL_HANDLE;
        VkCommandPool   Pool      = VK_NULL_HANDLE;
        VkFence         Fence     = VK_NULL_HANDLE;
        QueueType       QueueType = QueueType::Graphics;
    };

    // Per-frame swapchain objects (semaphores, fence, cmd buffer from swapchain pool).
    struct VulkanSwapchainFrame
    {
        VkCommandBuffer CmdBuf          = VK_NULL_HANDLE;
        VkSemaphore     PresentSemaphore = VK_NULL_HANDLE;
        VkFence         Fence            = VK_NULL_HANDLE;
    };

    struct VulkanSwapchain
    {
        VkSwapchainKHR              Swapchain        = VK_NULL_HANDLE;
        VkSurfaceKHR                Surface          = VK_NULL_HANDLE;
        VkRenderPass                RenderPass       = VK_NULL_HANDLE;
        VkExtent2D                  Extent           = {};
        VkFormat                    ColorFormat      = VK_FORMAT_UNDEFINED;
        VkCommandPool               CommandPool      = VK_NULL_HANDLE;
        Vector<VkImage>             Images;
        Vector<VkImageView>         ImageViews;
        Vector<VkFramebuffer>       Framebuffers;
        Vector<VkSemaphore>         RenderSemaphores;
        VulkanSwapchainFrame        Frames[3]        = {};
        uint32_t                    ImageCount       = 0;
        uint32_t                    CurrentFrameIdx  = 0;
        uint32_t                    CurrentImageIdx  = 0;
        bool                        Vsync            = false;
    };

    // ── Resource Registry ─────────────────────────────────────────────────────
    // Singleton that owns every HandlePool for the Vulkan backend.
    // Accessed via GetVulkanRegistry() to replace the old SURGE_GET_VULKAN_CONTEXT
    // pattern throughout backend code.

    class SURGE_API VulkanResourceRegistry
    {
    public:
        static VulkanResourceRegistry& Get();

        HandlePool<VulkanBuffer,           BufferTag>           Buffers;
        HandlePool<VulkanTexture,          TextureTag>          Textures;
        HandlePool<VulkanSampler,          SamplerTag>          Samplers;
        HandlePool<VulkanShader,           ShaderTag>           Shaders;
        HandlePool<VulkanGraphicsPipeline, GraphicsPipelineTag> GraphicsPipelines;
        HandlePool<VulkanComputePipeline,  ComputePipelineTag>  ComputePipelines;
        HandlePool<VulkanRenderPass,       RenderPassTag>       RenderPasses;
        HandlePool<VulkanFramebuffer,      FramebufferTag>      Framebuffers;
        HandlePool<VulkanCommandBuffer,    CommandBufferTag>    CommandBuffers;
        HandlePool<VulkanSwapchain,        SwapchainTag>        Swapchains;

        // Convenience typed getters (with handle validation in debug builds)
        VulkanBuffer&           GetBuffer(BufferHandle h)             { return Buffers.Get(h); }
        VulkanTexture&          GetTexture(TextureHandle h)           { return Textures.Get(h); }
        VulkanSampler&          GetSampler(SamplerHandle h)           { return Samplers.Get(h); }
        VulkanShader&           GetShader(ShaderHandle h)             { return Shaders.Get(h); }
        VulkanGraphicsPipeline& GetGraphicsPipeline(GraphicsPipelineHandle h) { return GraphicsPipelines.Get(h); }
        VulkanComputePipeline&  GetComputePipeline(ComputePipelineHandle h)   { return ComputePipelines.Get(h); }
        VulkanRenderPass&       GetRenderPass(RenderPassHandle h)     { return RenderPasses.Get(h); }
        VulkanFramebuffer&      GetFramebuffer(FramebufferHandle h)   { return Framebuffers.Get(h); }
        VulkanCommandBuffer&    GetCommandBuffer(CommandBufferHandle h) { return CommandBuffers.Get(h); }
        VulkanSwapchain&        GetSwapchain(SwapchainHandle h)       { return Swapchains.Get(h); }

    private:
        VulkanResourceRegistry() = default;
        SURGE_DISABLE_COPY(VulkanResourceRegistry);
    };

    // Short-hand accessor used throughout the Vulkan backend.
    SURGE_API VulkanResourceRegistry& GetVulkanRegistry();

} // namespace Surge::RHI
