// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Window/Window.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include "Surge/Graphics/RHI/RHIDescs.hpp"
#include "Surge/Graphics/RHI/RHIFrameContext.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanDevice.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanDebugger.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanResourceEntries.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanFrame.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanSwapchain.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanFramebuffer.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanImGui.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanDescriptorSet.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanUploader.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanGPUTimer.hpp"

#include <volk.h>
#include <vk_mem_alloc.h>

#define FORCE_VALIDATION 0
#if FORCE_VALIDATION == 1
    #define ENABLE_IF_VK_VALIDATION(x) { x; }
#else
    #ifdef SURGE_DEBUG
        #define ENABLE_IF_VK_VALIDATION(x) { x; }
    #else
        #define ENABLE_IF_VK_VALIDATION(x)
    #endif // SURGE_DEBUG
#endif

namespace Surge
{
    struct FrameContext;
    class VulkanRHI
    {
    public:
        VulkanRHI() = default;
        ~VulkanRHI() = default;
        void Initialize(Window* window, bool enableImGui);
        void WaitIdle() const;
        void Shutdown();

        void BeginFrame(FrameContext& outCtx);
        void EndFrame(const FrameContext& ctx);
        void Resize();

        // Android destroys the native window when the app is backgrounded, the surface + swapchain must follow it
        void OnSurfaceDestroyed();
        void OnSurfaceCreated(Window* window);

        // Returns true if presentation is paced by the RHI (Swappy), false if the caller has to limit the frame rate itself
        bool SetTargetFrameRate(float fps);
        void SetFramePacingEnabled(bool enabled);

        bool IsFormatSupported(ImageFormat format) const;
        Uint GetSurfaceRotation() const { return mSwapchain.GetRotation(); }

        const RHIStats& GetStats();

        // GPU timing (VulkanGPUTimer)
        void SetGPUTimingEnabled(bool enabled) { mGPUTimer.SetEnabled(enabled); }
        bool IsGPUTimingEnabled() const { return mGPUTimer.IsEnabled(); }
        bool SupportsGPUTiming() const { return mGPUTimer.IsSupported(); }
        bool SupportsGPUTimingInRenderPass() const { return !mIsTileBasedGPU; } // Tilers run a whole render pass per tile, timestamps inside it mean nothing
        const GPUTimings& GetGPUTimings() const { return mGPUTimer.GetResults(); }
        void CmdBeginGPUScope(const FrameContext& ctx, std::string_view name) { mGPUTimer.BeginScope(mFrame.GetFrame(ctx.FrameIndex).CmdBuffer, name); }
        void CmdEndGPUScope(const FrameContext& ctx) { mGPUTimer.EndScope(mFrame.GetFrame(ctx.FrameIndex).CmdBuffer); }
        void CmdWaitForSwapchainImage(const FrameContext& ctx);

        BufferHandle CreateBuffer(const BufferDesc& desc);
        void UploadBuffer(BufferHandle h, const void* data, Uint size, Uint offset);
        void DestroyBuffer(BufferHandle buffer);

        ImageHandle CreateImage(const ImageDesc& desc);
        void DestroyImage(ImageHandle h);
        void ResizeImage(ImageHandle h, Uint width, Uint height);
        void RecreateImage(ImageHandle h, const ImageDesc& desc); // Keeps the handle stable, content is undefined afterwards
        uint64_t GetImageSize(ImageHandle h) const;
        const ImageDesc& GetDesc(ImageHandle h) const;
        bool IsValid(ImageHandle h) const;
        bool IsImageSampleable(ImageHandle h) const;

        FramebufferHandle CreateFramebuffer(const FramebufferDesc& desc);
        void DestroyFramebuffer(FramebufferHandle h);
        void ResizeFramebuffer(FramebufferHandle h, Uint width, Uint height, bool resizeImages);
        void RecreateFramebuffer(FramebufferHandle h, const FramebufferDesc& desc); // Same handle, new attachments/load-store ops (must stay render pass compatible)
        const FramebufferDesc& GetDesc(FramebufferHandle h) const;

        PipelineHandle CreatePipeline(const PipelineDesc& desc);
        PipelineHandle CreateComputePipeline(const ComputePipelineDesc& desc);
        bool SupportsCompute() const { return mDevice.SupportsCompute(); }
        Uint GetMaxImageDimension2D() const { return mDevice.GetMaxImageDimension2D(); }
        void DestroyPipeline(PipelineHandle h);

        SamplerHandle CreateSampler(const SamplerDesc& desc);
        void DestroySampler(SamplerHandle h);

        DescriptorSetHandle CreateDescriptorSet(PipelineHandle pipelineHandle, DescriptorSetSlot slot, DescriptorUpdateFrequency frequency, const char* debugName = nullptr);
        void UpdateDescriptorSet(DescriptorSetHandle setHandle, const DescriptorWrite* writes, Uint writeCount, Uint frameIndex);
        void DestroyDescriptorSet(DescriptorSetHandle h);

        // Commands
        void CmdDrawIndexed(const FrameContext& ctx, Uint indexCount, Uint instanceCount, Uint firstIndex, int32_t vertexOffset, Uint firstInstance);
        void CmdDraw(const FrameContext& ctx, Uint vertexCount, Uint instanceCount, Uint firstVertex, Uint firstInstance);
        void CmdDispatch(const FrameContext& ctx, Uint groupCountX, Uint groupCountY, Uint groupCountZ); // Must be recorded outside of a renderpass

        void CmdBindVertexBuffer(const FrameContext& ctx, BufferHandle h, Uint offset = 0);
        void CmdBindIndexBuffer(const FrameContext& ctx, BufferHandle h, Uint offset = 0);
        void CmdBindPipeline(const FrameContext& ctx, PipelineHandle h);

        void CmdPushConstants(const FrameContext& ctx, PipelineHandle h, ShaderType shaderStage, Uint offset, Uint size, const void* data);
        void CmdBlitToSwapchain(const FrameContext& ctx, ImageHandle srcHandle);
        void CmdTransitionImageLayout(const FrameContext& ctx, ImageHandle h, ImageUsage newLayout);
        void CmdTransitionImageLayout(VkCommandBuffer cmd, ImageHandle h, ImageUsage newLayout);
        void CmdSetDepthBias(const FrameContext& ctx, float constantFactor, float clamp, float slopeFactor);
        void CmdSetViewport(const FrameContext& ctx, Uint x, Uint y, Uint width, Uint height);
        void CmdBeginSwapchainRenderpass(const FrameContext& ctx);
        void CmdEndSwapchainRenderpass(const FrameContext& ctx);

        void CmdBeginRenderPass(const FrameContext& ctx, FramebufferHandle h);
        void CmdEndRenderPass(const FrameContext& ctx, FramebufferHandle h);

        // slot maps to layout(set = N) in GLSL
        void CmdBindDescriptorSet(const FrameContext& ctx, PipelineHandle pipeline, DescriptorSetHandle setHandle, DescriptorSetSlot slot);

        VulkanUploader& GetUploader() const { return mUploader; }
        VkPipelineCache GetPipelineCache() const { return mPipelineCache; }
        void* GetNativeWindowHandle() const { return mWindow ? mWindow->GetNativeWindowHandle() : nullptr; }

        void SetDebugName(VkObjectType objType, uint64_t objectHandle, const String& name) const { mDebugger.SetDebugName(*this, objType, objectHandle, name); }

        ImTextureID AddImGuiImage(ImageHandle h);
        ImTextureID GetImGuiImage(ImageHandle h);
        void DestroyImGuiImage(ImageHandle h);

        // Getters for internal use by Vulkan* classes
        const VulkanSwapchain& GetSwapchain() const { return mSwapchain; }
        VulkanSwapchain& GetSwapchain() { return mSwapchain; }
        VulkanFrame& GetFrame() { return mFrame; }

        VkInstance GetInstance() const { return mInstance; }
        VkSurfaceKHR GetSurface() const { return mSurface; }
        VkFramebuffer GetSwapchainFramebuffer(Uint index) const { return mSwapchainFramebuffers[index]; }
        VkRenderPass GetRenderPass() const { return mRenderPass; }
        VkDevice GetDevice() const { return mDevice.GetDevice(); }
        VkPhysicalDevice GetGPU() const { return mDevice.GetGPU(); }
        VkQueue GetQueue() const { return mDevice.GetQueue(); }
        VmaAllocator GetAllocator() const { return mDevice.GetAllocator(); }
        int32_t GetQueueIndex() const { return mDevice.GetQueueIndex(); }

        const Vector<VkDescriptorPool>& GetDescriptorPools() const { return mVkDescriptorPools; }
        const Vector<VkDescriptorPool>& GetNonResetableDescriptorPools() const { return mNonResetableVkDescriptorPools; }

        void ShowPoolDebugImGuiWindow(float frameTimeMilliseconds);

    private:
        void CreateInstance();
        void FillStats();

        void CreateSurface(Window* window);
        void DestroySurface();

        void CreateSwapchainFramebuffers();
        void DestroySwapchainFramebuffers();

        void CreateSwapchainRenderpass();
        void DestroySwapchainRenderpass();

        void ResizeInternal();
        void CreateDescriptorPools();

        void CreatePipelineCache();
        void DestroyPipelineCache();

        Vector<const char*> GetRequiredInstanceExtensions();
        Vector<const char*> GetRequiredInstanceLayers();

        void FlushDeletionQueue(Uint frameIndex);
        void DeferImGuiTextureDestroy(ImTextureID& id);

        // Which slot's deletion queue may free a resource destroyed right now
        // While recording: the current slot, its fence also covers this frame's commands
        // Between frames (game/Lua update, frame end callbacks): the current index already points at the NEXT slot, which is flushed at the very next
        // BeginFrame while the frame that was just submitted may still use the resource, so the last submitted slot is used instead
        Uint GetDeletionQueueIndex() const
        {
            const Uint current = mFrame.GetCurrentFrameIndex();
            return mIsRecording ? current : (current + RHISettings::FRAMES_IN_FLIGHT - 1) % RHISettings::FRAMES_IN_FLIGHT;
        }
    private:
        RHIStats mStats;

        VulkanDevice mDevice;
        VulkanDebugger mDebugger;
        VulkanFrame mFrame;
        VulkanSwapchain mSwapchain;
        VulkanImGuiContext mImGuiContext;
        VulkanRenderpassFactory mRenderPassCache;

        Vector<VkFramebuffer> mSwapchainFramebuffers;
        VkRenderPass mRenderPass = VK_NULL_HANDLE;

        VkInstance mInstance { VK_NULL_HANDLE };
        VkSurfaceKHR mSurface { VK_NULL_HANDLE };
        Window* mWindow = nullptr;

        mutable VulkanUploader mUploader; // Mutable: WaitIdle() (const) has to submit pending uploads first
        VulkanGPUTimer mGPUTimer;
        bool mIsTileBasedGPU = false;
        VkPipelineCache mPipelineCache = VK_NULL_HANDLE;
        float mTargetFrameRate = 60.0f;

        Vector<VkDescriptorPool> mVkDescriptorPools;
        Vector<VkDescriptorPool> mNonResetableVkDescriptorPools;

        //Pools
        HandlePool<PipelineHandle, PipelineEntry> mPipelinePool;
        HandlePool<BufferHandle, BufferEntry> mBufferPool;
        HandlePool<FramebufferHandle, FramebufferEntry> mFramebufferPool;
        HandlePool<ImageHandle, ImageEntry> mTexturePool;
        HandlePool<SamplerHandle, SamplerEntry> mSamplerPool;
        HandlePool<DescriptorSetHandle, DescriptorSetEntry> mDescriptorSetPool;

        // Deletion queue
        struct DeferredDeletes
        {
            Vector<BufferEntry> Buffers;
            Vector<ImageEntry> Images;
            Vector<FramebufferEntry> Framebuffers;
            Vector<PipelineEntry> Pipelines;
            Vector<SamplerEntry> Samplers;
            Vector<DescriptorSetEntry> DescriptorSets;
            Vector<ImTextureID> ImGuiTextures; // Descriptor sets owned by the ImGui Vulkan backend
        };
        std::array<DeferredDeletes, RHISettings::FRAMES_IN_FLIGHT> mDeletionQueues;
        bool mIsShuttingDown = false;
        bool mIsRecording = false;
        bool mPresentSuboptimal = false;

        friend class VulkanPipeline;
        friend class VulkanImage;
        friend class VulkanDescriptorSet;
    };

}