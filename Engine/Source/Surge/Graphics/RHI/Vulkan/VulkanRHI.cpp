// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RHI/RHISettings.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHI.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanDebugger.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanBuffer.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanPipeline.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanUtils.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanImage.hpp"

#include "Surge/Core/Core.hpp"
#include "Surge/Core/Logger/Logger.hpp"

#ifdef SURGE_PLATFORM_ANDROID
#include <android/native_window.h>
#endif
#include "Backends/imgui_impl_vulkan.h"
#include <SurgeReflect/Enum.hpp>

#define LOG_OBJ_CREATION_DELETION 0
#if LOG_OBJ_CREATION_DELETION
#define VK_RHI_LOG(x) x
#else
#define VK_RHI_LOG(x)
#endif

namespace Surge
{
    static Vector<String> ValidateExtensions(const Vector<const char*>& required, const Vector<VkExtensionProperties>& available)
    {
        Vector<String> missingExtensions;

        for(auto extension : required)
        {
            bool found = false;
            for(auto& availableExtension : available)
            {
                if(strcmp(availableExtension.extensionName, extension) == 0)
                {
                    found = true;
                    break;
                }
            }

            if(!found)
            {
                missingExtensions.push_back(extension);
            }
        }

        return missingExtensions;
    }

    static String GetVendorName(Uint vendorID)
    {
        switch (vendorID) {
        case 0x10DE: return "NVIDIA";
        case 0x1002: return "AMD";
        case 0x8086: return "INTEL";
        case 0x13B5: return "ARM (Mali)";
        case 0x5143: return "Qualcomm (Adreno)";
        case 0x1010: return "ImgTec (PowerVR)";
        default:     return "Unknown Vendor";
        }
    }

    void VulkanRHI::Initialize(Window* window)
    {
        SCOPED_TIMER("VulkanRHI::Initialize");

        CreateInstance();
        CreateSurface(window);
        mDevice.Initialize(mInstance, mSurface);
        mFrame.Initialize(*this, RHISettings::FRAMES_IN_FLIGHT);
        mSwapchain.Initialize(*this, window->GetSize().x, window->GetSize().y);

        CreateSwapchainRenderpass();
        CreateSwapchainFramebuffers();

        mImGuiContext.Init(*this);
        FillStats();
        CreateDescriptorPools();
    }

    void VulkanRHI::WaitIdle() const
    {
        vkDeviceWaitIdle(mDevice.GetDevice());
    }

    void VulkanRHI::Shutdown()
    {
        SCOPED_TIMER("VulkanRHI::Shutdown");

        // Destroy the descriptor pools
        for(VkDescriptorPool& pool : mVkDescriptorPools)
            vkDestroyDescriptorPool(mDevice, pool, nullptr);
        for(VkDescriptorPool& pool : mNonResetableVkDescriptorPools)
            vkDestroyDescriptorPool(mDevice, pool, nullptr);

        mRenderPassCache.Shutdown(*this);
        mImGuiContext.Shutdown(*this);

        mDescriptorSetPool.ForEachAlive([&](const DescriptorSetHandle& h, DescriptorSetEntry& entry) { DestroyDescriptorSet(h); SG_ASSERT_INTERNAL("You forgot to destroy a descriptor layout manually!"); });
        mSamplerPool.ForEachAlive([&](const SamplerHandle& h, SamplerEntry& entry){ DestroySampler(h); SG_ASSERT_INTERNAL("You forgot to destroy a sampler manually!"); });
        mFramebufferPool.ForEachAlive([&](const FramebufferHandle& h, FramebufferEntry& entry) { VulkanFramebuffer::Destroy(*this, entry); SG_ASSERT_INTERNAL("You forgot to destroy a framebuffer manually!"); });
        mTexturePool.ForEachAlive([&](const ImageHandle& h, ImageEntry& entry) { VulkanImage::Destroy(*this, entry); SG_ASSERT_INTERNAL("You forgot to destroy a texture manually!"); });
        mBufferPool.ForEachAlive([&](const BufferHandle& h, BufferEntry& entry) { VulkanBuffer::Destroy(*this, entry); SG_ASSERT_INTERNAL("You forgot to destroy a buffer manually!"); });
        mPipelinePool.ForEachAlive([&](const PipelineHandle& h, PipelineEntry& entry) { VulkanPipeline::Destroy(*this, entry); SG_ASSERT_INTERNAL("You forgot to destroy a pipeline manually!"); });

        DestroySwapchainFramebuffers();
        DestroySwapchainRenderpass();

        mFrame.Shutdown(*this);
        mSwapchain.Shutdown(*this);
        ENABLE_IF_VK_VALIDATION(mDebugger.EndDiagnostics(mInstance));
        mDevice.Destroy();
        vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
        vkDestroyInstance(mInstance, nullptr);
    }

    void VulkanRHI::BeginFrame(FrameContext& outCtx)
    {
        SURGE_PROFILE_FUNC("VulkanRHI::BeginFrame");
        mStats.Reset();
        mImGuiContext.BeginFrame();
        VkDevice device = mDevice.GetDevice();

        const PerFrame& frame = mFrame.GetCurrentVkFrame();
        Uint swapchainWidth = mSwapchain.GetWidth();
        Uint swapchainHeight = mSwapchain.GetHeight();

        // Wait for this SLOT's fence
        // This slot was used N frames ago, wait until the GPU is done with it
        {
            //Timer fenceTimer("vkWaitForFences", true);
            vkWaitForFences(device, 1, &frame.Fence, VK_TRUE, UINT64_MAX);
        }

        // Ask swapchain which IMAGE is available
        // This is unpredictable, diver may return any index
        // AcquireSemaphore signals when the image is actually ready to write
        Uint imageIndex = 0;
        VkResult result = mSwapchain.AcquireNextImage(*this, frame.AcquireSemaphore, imageIndex);

        outCtx.FrameIndex = mFrame.GetCurrentFrameIndex();
        outCtx.SwapchainIndex = imageIndex;
        outCtx.Width = swapchainWidth;
        outCtx.Height = swapchainHeight;
        //Log<Severity::Debug>("-------------Beginning CPU frame: FrameIndex: {}-------------", outCtx.FrameIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        {
            // Handle resize 
            // IS this good? Or I should get the window size from Core?
            ResizeInternal();
            result = mSwapchain.AcquireNextImage(*this, frame.AcquireSemaphore, imageIndex);
            outCtx.Width = mSwapchain.GetWidth();
            outCtx.Height = mSwapchain.GetHeight();
        }
        SG_ASSERT(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR, "VulkanRHI: AcquireNextImage failed");

        vkResetFences(device, 1, &frame.Fence);
        vkResetCommandPool(device, frame.CmdPool, 0);

        // Reset descriptor pool
        VK_CALL(vkResetDescriptorPool(mDevice, mVkDescriptorPools[outCtx.FrameIndex], 0));

        // Begin recording
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(frame.CmdBuffer, &beginInfo);
    }

    void VulkanRHI::EndFrame(const FrameContext& ctx)
    {
        SURGE_PROFILE_FUNC("VulkanRHI::EndFrame");
        // End recording
        const PerFrame& frame = mFrame.GetCurrentVkFrame();
        vkEndCommandBuffer(frame.CmdBuffer);

        //Log<Severity::Debug>("-------------Ending CPU frame: FrameIndex: {}-------------", ctx.FrameIndex);
        // Submit: frame slot semaphores sync with swapchain image
        // Wait on AcquireSemaphore -> GPU waits until compositor releases the image
        // Signal ReleaseSemaphore -> tells compositor GPU is done writing to it
        // Signal Fence -> tells CPU this slot is free N frames from now
        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSemaphore releaseSemaphore = mSwapchain.GetFrame(ctx.SwapchainIndex).ReleaseSemaphore;

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &frame.AcquireSemaphore;
        submitInfo.pWaitDstStageMask = &waitStage;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &frame.CmdBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &releaseSemaphore;
        vkQueueSubmit(mDevice.GetQueue(), 1, &submitInfo, frame.Fence);

        // Present: swapchain image index + release semaphore from slot
        // We Dont check for out of date here, if the swapchain is out of date we will catch it at the beginning of the next frame when we try to acquire an image
        // (Rid) Is this good?
        mSwapchain.Present(*this, releaseSemaphore, ctx.SwapchainIndex);

        // Advance SLOT index (completely independent of swapchain image index)
        mFrame.AdvanceFrame();
    }

    void VulkanRHI::Resize()
    {
#ifdef SURGE_PLATFORM_ANDROID
        // TODO: Android destroys and creates surface when window is minimized!
        //VulkanRHI& rhi = Core::GetRenderer()->GetRHI()->GetBackendRHI();
        //vkDestroySurfaceKHR(rhi.GetInstance(), rhi.GetSurface(), nullptr);
        //CreateSurface(Core::GetWindow());
#endif		
        Core::AddFrameEndCallback([this]() {
                ResizeInternal();
            });
    }

    const RHIStats& VulkanRHI::GetStats()
    {
        // We need to write the memory stats every time RHI::GetStats() is called because GPU memory can change dynamically
        const GPUMemoryStats& memStats = mDevice.GetMemoryStats();
        uint64_t bytes = memStats.AllocatedBytes.load(std::memory_order_relaxed);
        mStats.UsedGPUMemory = (float)bytes / (1024.0f * 1024.0f);
        mStats.TotalAllowedGPUMemory = (float)memStats.BudgetBytes / (1024.0f * 1024.0f);
        mStats.AllocationCount = memStats.AllocationCount.load(std::memory_order_relaxed);

        return mStats;
    }

    BufferHandle VulkanRHI::CreateBuffer(const BufferDesc& desc)
    {
        VK_RHI_LOG(Log<Severity::Trace>("VulkanRHI::CreateBuffer: Name: {0} Size: {1} bytes", desc.DebugName ? desc.DebugName : "Unnamed", desc.Size));
        BufferEntry entry = VulkanBuffer::Create(*this, desc);
        return mBufferPool.Allocate(std::move(entry));
    }

    void VulkanRHI::UploadBuffer(BufferHandle h, const void* data, Uint size, Uint offset)
    {
        SURGE_PROFILE_FUNC("VulkanRHI::UploadBuffer");
        BufferEntry* entry = mBufferPool.Get(h);
        SG_ASSERT(entry != nullptr, "UploadBuffer: invalid handle!");

        VulkanBuffer::Upload(*entry, data, size, offset);
    }

    void VulkanRHI::DestroyBuffer(BufferHandle buffer)
    {
        BufferEntry* entry = mBufferPool.Get(buffer);
        if (!entry)
            return;

        VK_RHI_LOG(Log<Severity::Info>("VulkanRHI::DestroyBuffer: Size: {0} bytes", entry->Desc.Size));		
        VulkanBuffer::Destroy(*this, *entry);// kills VkBuffer + VmaAllocation
        mBufferPool.Free(buffer); // Return slot to free list
    }

    ImageHandle VulkanRHI::CreateImage(const ImageDesc& desc)
    {
        // TRANSFER_DST is required when InitialData is provided
        SG_ASSERT(!(desc.InitialData && !(desc.Usage & ImageUsage::TRANSFER_DST)), "TextureDesc: InitialData provided but TRANSFER_DST not set in Usage. Add TextureUsage::TRANSFER_DST to upload pixel data");

        ImageEntry entry = VulkanImage::Create(*this, desc);

        if(desc.GenerateImGuiID)
        {
            SG_ASSERT(desc.Usage & ImageUsage::SAMPLED, "ImageDesc: GenerateImGuiID is true but SAMPLED not set in Usage!");
            entry.ImGuiID = mImGuiContext.AddImage(entry.View);
        }

        ImageHandle h = mTexturePool.Allocate(std::move(entry));

        if (desc.InitialData && desc.DataSize > 0)
            UploadImageData(h, desc.InitialData, desc.DataSize);

        VK_RHI_LOG(Log<Severity::Trace>("VulkanRHI::CreateTexture of size {0}x{1} with format {2} and usage {3}", desc.Width, desc.Height, static_cast<Uint>(desc.Format), static_cast<Uint>(desc.Usage)));
        return h;
    }

    void VulkanRHI::DestroyImage(ImageHandle h)
    {
        ImageEntry* entry = mTexturePool.Get(h);
        if (!entry)
            return;

        VK_RHI_LOG(Log<Severity::Info>("Destroying texture with handle index {0} and generation {1}", h.Index, h.Generation));

        if (entry->Desc.GenerateImGuiID)
            DestroyImGuiImage(h);

        VulkanImage::Destroy(*this, *entry);
        mTexturePool.Free(h);		
    }

    void VulkanRHI::UploadImageData(ImageHandle h, const void* data, Uint size)
    {
        SG_ASSERT(data && size > 0, "UploadTextureData: data is null or size is 0");
        VulkanImage::UploadData(*this, h, data, size);
    }

    void VulkanRHI::ResizeImage(ImageHandle h, Uint width, Uint height)
    {
        //WaitIdle();

        ImageEntry* entry = mTexturePool.Get(h);
        if (!entry)
            return;

        if (entry->Desc.GenerateImGuiID)
            DestroyImGuiImage(h);

        VulkanImage::Destroy(*this, *entry);
        ImageDesc desc = entry->Desc;
        desc.Width = width;
        desc.Height = height;

        *entry = VulkanImage::Create(*this, desc);
        if (desc.GenerateImGuiID)
            entry->ImGuiID = mImGuiContext.AddImage(entry->View);
    }

    uint64_t VulkanRHI::GetImageSize(ImageHandle h) const
    {
        const ImageEntry* entry = mTexturePool.Get(h);
        SG_ASSERT(entry, "GetImageSize: invalid TextureHandle");
        return entry->Size;
    }

    FramebufferHandle VulkanRHI::CreateFramebuffer(const FramebufferDesc& desc)
    {
        VK_RHI_LOG(Log<Severity::Trace>("VulkanRHI::CreateFramebuffer of size {0}x{1} with {2} color attachments and depth attachment: {3}", desc.Width, desc.Height, desc.ColorAttachmentCount, desc.HasDepth));
        FramebufferEntry entry = VulkanFramebuffer::Create(*this, desc, mRenderPassCache, mTexturePool);
        return mFramebufferPool.Allocate(std::move(entry));
    }

    void VulkanRHI::DestroyFramebuffer(FramebufferHandle h)
    {
        FramebufferEntry* entry = mFramebufferPool.Get(h);
        if (!entry)
            return;

        VK_RHI_LOG(Log<Severity::Info>("Destroying framebuffer with handle index {0} and generation {1}", h.Index, h.Generation));
        VulkanFramebuffer::Destroy(*this, *entry);
        mFramebufferPool.Free(h);
    }

    void VulkanRHI::ResizeFramebuffer(FramebufferHandle h, Uint width, Uint height)
    {
        WaitIdle();

        FramebufferEntry* entry = mFramebufferPool.Get(h);
        if (!entry)
            return;

        VulkanFramebuffer::Destroy(*this, *entry);

        // Resize the attached Textures
        for (Uint i = 0; i < entry->Desc.ColorAttachmentCount; i++)
            ResizeImage(entry->Desc.ColorAttachments[i].Handle, width, height);
        if (entry->Desc.HasDepth)
            ResizeImage(entry->Desc.DepthAttachment.Handle, width, height);

        // Rebuild with new dimensions
        FramebufferDesc desc = entry->Desc;
        desc.Width = width;
        desc.Height = height;

        *entry = VulkanFramebuffer::Create(*this, desc, mRenderPassCache, mTexturePool);
    }

    const FramebufferDesc& VulkanRHI::GetDesc(FramebufferHandle h) const
    {
        const FramebufferEntry* entry = mFramebufferPool.Get(h);
        SG_ASSERT(entry, "GetDesc: invalid FramebufferHandle");
        return entry->Desc;
    }

    const ImageDesc& VulkanRHI::GetDesc(ImageHandle h) const
    {
        const ImageEntry* entry = mTexturePool.Get(h);
        SG_ASSERT(entry, "GetDesc: invalid TextureHandle");
        return entry->Desc;
    }

    PipelineHandle VulkanRHI::CreatePipeline(const PipelineDesc& desc)
    {
        SG_ASSERT(!desc.TargetFramebuffer.IsNull() || desc.TargetSwapchain, "PipelineDesc: must set either TargetFramebuffer or TargetSwapchain");
        SG_ASSERT(!((!desc.TargetFramebuffer.IsNull()) && desc.TargetSwapchain), "PipelineDesc: cannot set both TargetFramebuffer and TargetSwapchain");

        VK_RHI_LOG(Log<Severity::Trace>("VulkanRHI::CreatePipeline: Name: {0}", desc.DebugName ? desc.DebugName : "Unnamed"));

        VkRenderPass renderPass = VK_NULL_HANDLE;

        if (!desc.TargetFramebuffer.IsNull())
        {
            FramebufferEntry* fb = mFramebufferPool.Get(desc.TargetFramebuffer);
            SG_ASSERT(fb, "CreatePipeline: invalid TargetFramebuffer handle");
            renderPass = fb->RenderPass; // Borrowed from cache
        }
        else if (desc.TargetSwapchain)
            renderPass = mRenderPass;
        
        SG_ASSERT(renderPass != VK_NULL_HANDLE, "CreatePipeline: failed to resolve render pass");
        PipelineEntry entry = VulkanPipeline::Create(*this, desc, renderPass);
        return mPipelinePool.Allocate(std::move(entry));
    }

    void VulkanRHI::DestroyPipeline(PipelineHandle h)
    {
        PipelineEntry* entry = mPipelinePool.Get(h);
        if (!entry)
            return;

        VK_RHI_LOG(Log<Severity::Info>("Destroying pipeline with handle index {0} and generation {1}", h.Index, h.Generation));
        VulkanPipeline::Destroy(*this, *entry);
        mPipelinePool.Free(h);
    }

    SamplerHandle VulkanRHI::CreateSampler(const SamplerDesc& desc)
    {
        VkSamplerCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        info.magFilter = VulkanUtils::ToVkFilter(desc.Mag);
        info.minFilter = VulkanUtils::ToVkFilter(desc.Min);
        info.mipmapMode = desc.Mip == MipmapMode::LINEAR ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
        info.addressModeU = VulkanUtils::ToVkWrap(desc.WrapU);
        info.addressModeV = VulkanUtils::ToVkWrap(desc.WrapV);
        info.addressModeW = VulkanUtils::ToVkWrap(desc.WrapU);
        info.mipLodBias = desc.MipBias;
        info.minLod = 0.0f;
        info.maxLod = VK_LOD_CLAMP_NONE;
        info.anisotropyEnable = desc.Anisotropy ? VK_TRUE : VK_FALSE;
        info.maxAnisotropy = desc.MaxAniso;
        if (desc.CompareEnable)
        {
            info.compareEnable = VK_TRUE;
            info.compareOp = VulkanUtils::ToVkCompareOp(desc.CompareOp_);
            info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        }

        SamplerEntry entry = {};
        entry.Desc = desc;

        VK_RHI_LOG(Log<Severity::Trace>("VulkanRHI::CreateSampler: MagFilter: {0}, MinFilter: {1}, MipMode: {2}, WrapU: {3}, WrapV: {4}, MipBias: {5}, Anisotropy: {6}, MaxAniso: {7}",
            desc.Mag, desc.Min, desc.Mip, desc.WrapU, desc.WrapV, desc.MipBias, desc.Anisotropy, desc.MaxAniso));

        VK_CALL(vkCreateSampler(mDevice, &info, nullptr, &entry.Sampler));
        return mSamplerPool.Allocate(std::move(entry));
    }

    void VulkanRHI::DestroySampler(SamplerHandle h)
    {
        VK_RHI_LOG(Log<Severity::Info>("Destroying sampler with handle index {0} and generation {1}", h.Index, h.Generation));
        SamplerEntry* entry = mSamplerPool.Get(h);
        if (!entry)
            return;

        vkDestroySampler(mDevice, entry->Sampler, nullptr);
        mSamplerPool.Free(h);
    }

    void VulkanRHI::CmdBindDescriptorSet(const FrameContext& ctx, PipelineHandle pipeline, DescriptorSetHandle setHandle, DescriptorSetSlot slot)
    {
        PipelineEntry* entry = mPipelinePool.Get(pipeline);
        DescriptorSetEntry* setEntry = mDescriptorSetPool.Get(setHandle);

        SG_ASSERT(entry, "BindDescriptorSet: invalid PipelineHandle");
        SG_ASSERT(setEntry, "BindDescriptorSet: invalid DescriptorSetHandle");

        VkCommandBuffer cmd = mFrame.GetFrame(ctx.FrameIndex).CmdBuffer;
        Uint setToBind = (setEntry->Frequency == DescriptorUpdateFrequency::DYNAMIC) ? ctx.FrameIndex : 0;
        VulkanDescriptorSet::Bind(cmd, entry->Layout, setEntry->Sets[setToBind], slot);
    }

    DescriptorSetHandle VulkanRHI::CreateDescriptorSet(PipelineHandle pipelineHandle, DescriptorSetSlot slot, DescriptorUpdateFrequency frequency, const char* debugName /*= nullptr*/)
    {
        const PipelineEntry* pipelineEntry = mPipelinePool.Get(pipelineHandle);
        SG_ASSERT(pipelineEntry, "CreateDescriptorSet: invalid PipelineHandle");

        DescriptorSetEntry entry = VulkanDescriptorSet::Create(*this, pipelineEntry, slot, frequency, debugName);
        return mDescriptorSetPool.Allocate(std::move(entry));
    }

    void VulkanRHI::UpdateDescriptorSet(DescriptorSetHandle setHandle, const DescriptorWrite* writes, Uint writeCount, Uint frameIndex)
    {
        DescriptorSetEntry* entry = mDescriptorSetPool.Get(setHandle);
        SG_ASSERT(entry, "UpdateDescriptorSet: invalid DescriptorSetHandle");
        VulkanDescriptorSet::Update(*this, *entry, writes, writeCount, frameIndex);
    }

    void VulkanRHI::DestroyDescriptorSet(DescriptorSetHandle h)
    {
        DescriptorSetEntry* entry = mDescriptorSetPool.Get(h);
        if(!entry)
            return;

        VulkanDescriptorSet::Destroy(*this, *entry);
        mDescriptorSetPool.Free(h);
    }

    void VulkanRHI::CmdDrawIndexed(const FrameContext& ctx, Uint indexCount, Uint instanceCount, Uint firstIndex, int32_t vertexOffset, Uint firstInstance)
    {
        VkCommandBuffer cmd = mFrame.GetFrame(ctx.FrameIndex).CmdBuffer;
        vkCmdDrawIndexed(cmd, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
        mStats.DrawCalls++;
    }

    void VulkanRHI::CmdDraw(const FrameContext& ctx, Uint vertexCount, Uint instanceCount, Uint firstVertex, Uint firstInstance)
    {
        VkCommandBuffer cmd = mFrame.GetFrame(ctx.FrameIndex).CmdBuffer;
        vkCmdDraw(cmd, vertexCount, instanceCount, firstVertex, firstInstance);
        mStats.DrawCalls++;
    }

    void VulkanRHI::CmdBindVertexBuffer(const FrameContext& ctx, BufferHandle h, Uint offset /*= 0*/)
    {
        VkCommandBuffer cmd = mFrame.GetFrame(ctx.FrameIndex).CmdBuffer;
        const BufferEntry* entry = mBufferPool.Get(h);
        SG_ASSERT(entry != nullptr, "CmdBindVertexBuffer: Invalid BufferHandle");
        SG_ASSERT(entry->Buffer != VK_NULL_HANDLE, "CmdBindVertexBuffer: null VkBuffer");

        VkDeviceSize vkOffset = offset;
        vkCmdBindVertexBuffers(cmd, 0, 1, &entry->Buffer, &vkOffset);
    }

    void VulkanRHI::CmdBindIndexBuffer(const FrameContext& ctx, BufferHandle h, Uint offset /*= 0*/)
    {
        VkCommandBuffer cmd = mFrame.GetFrame(ctx.FrameIndex).CmdBuffer;
        const BufferEntry* entry = mBufferPool.Get(h);
        SG_ASSERT(entry != nullptr, "CmdBindIndexBuffer: Invalid BufferHandle");
        vkCmdBindIndexBuffer(cmd, entry->Buffer, offset, VK_INDEX_TYPE_UINT32);
    }

    void VulkanRHI::CmdBindPipeline(const FrameContext& ctx, PipelineHandle h)
    {
        VkCommandBuffer cmd = mFrame.GetFrame(ctx.FrameIndex).CmdBuffer;
        PipelineEntry* entry = mPipelinePool.Get(h);
        SG_ASSERT(entry, "CmdBindPipeline: invalid handle");
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, entry->Pipeline);
    }

    void VulkanRHI::CmdPushConstants(const FrameContext& ctx, PipelineHandle h, ShaderType shaderStage, Uint offset, Uint size, const void* data)
    {
        VkCommandBuffer cmd = mFrame.GetFrame(ctx.FrameIndex).CmdBuffer;
        PipelineEntry* entry = mPipelinePool.Get(h);
        SG_ASSERT(entry, "CmdPushConstants: invalid handle");
        vkCmdPushConstants(cmd, entry->Layout, VulkanUtils::ShaderTypeToVulkanShaderStage(shaderStage), offset, size, data);
    }

    void VulkanRHI::CmdBlitToSwapchain(const FrameContext& ctx, ImageHandle srcHandle)
    {
        VkCommandBuffer cmd = mFrame.GetFrame(ctx.FrameIndex).CmdBuffer;
        ImageEntry* src = mTexturePool.Get(srcHandle);
        SG_ASSERT(src, "CmdBlitToSwapchain: invalid source TextureHandle");

        VkImage swapchainImage = mSwapchain.GetFrame(ctx.SwapchainIndex).Image;
        Uint w = mSwapchain.GetDimensions().Width;
        Uint h = mSwapchain.GetDimensions().Height;
        
        // Transition offscreen: src->Layout to TRANSFER_SRC
        VkImageMemoryBarrier srcBarrier = {};
        srcBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        srcBarrier.oldLayout = src->Layout;
        srcBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        srcBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        srcBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        srcBarrier.image = src->Image;
        srcBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        srcBarrier.subresourceRange.levelCount = 1;
        srcBarrier.subresourceRange.layerCount = 1;

        // Transition swapchain: UNDEFINED to TRANSFER_DST
        VkImageMemoryBarrier dstBarrier = {};
        dstBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        dstBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        dstBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        dstBarrier.srcAccessMask = 0;
        dstBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        dstBarrier.image = swapchainImage;
        dstBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        dstBarrier.subresourceRange.levelCount = 1;
        dstBarrier.subresourceRange.layerCount = 1;

        VkImageMemoryBarrier preBlit[2] = { srcBarrier, dstBarrier };
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, preBlit);

        // Blit
        VkImageBlit region = {};
        region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.layerCount = 1;
        region.srcOffsets[0] = { 0, 0, 0 };
        region.srcOffsets[1] = { (int32_t)w, (int32_t)h, 1 };
        region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.layerCount = 1;
        region.dstOffsets[0] = { 0, 0, 0 };
        region.dstOffsets[1] = { (int32_t)w, (int32_t)h, 1 };
        vkCmdBlitImage(cmd, src->Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_NEAREST);

        // Transition swapchain: TRANSFER_DST to COLOR_ATTACHMENT
        // ImGui pass will render on top (needs COLOR_ATTACHMENT_OPTIMAL)
        VkImageMemoryBarrier postBlit = {};
        postBlit.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        postBlit.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        postBlit.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        postBlit.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        postBlit.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        postBlit.image = swapchainImage;
        postBlit.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        postBlit.subresourceRange.levelCount = 1;
        postBlit.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &postBlit);

        // Track layout for offscreen texture
        src->Layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    }

    void VulkanRHI::CmdTransitionImageLayout(const FrameContext& ctx, ImageHandle h, ImageUsage newLayout)
    {
        ImageEntry* entry = mTexturePool.Get(h);
        SG_ASSERT(entry, "VulkanRHI::CmdTransitionTextureLayout: invalid TextureHandle");

        VkCommandBuffer cmd = mFrame.GetFrame(ctx.FrameIndex).CmdBuffer;
        VkImageLayout vkLayout = VulkanUtils::ImageUsageToVkLayout(newLayout); // Validate newLayout is compatible with our supported usages
        VulkanImage::TransitionLayout(cmd, *entry, vkLayout);
    }

    void VulkanRHI::CmdBeginSwapchainRenderpass(const FrameContext& ctx)
    {
        // Set clear color values
        VkClearValue clearValue{ .color = {{0.1f, 0.1f, 0.1f, 1.0f}} };

        VkRenderPassBeginInfo rpbegin{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = mRenderPass,
            .framebuffer = mSwapchainFramebuffers[ctx.SwapchainIndex],
            .renderArea = {.extent = {.width = ctx.Width, .height = ctx.Height}},
            .clearValueCount = 1,
            .pClearValues = &clearValue };

        VkCommandBuffer cmd = mFrame.GetCurrentVkFrame().CmdBuffer;
        vkCmdBeginRenderPass(cmd, &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport vp{
            .width = static_cast<float>(ctx.Width),
            .height = static_cast<float>(ctx.Height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f };
        vkCmdSetViewport(cmd, 0, 1, &vp); // Set viewport dynamically
        VkRect2D scissor{ .extent = {.width = ctx.Width, .height = ctx.Height} };
        vkCmdSetScissor(cmd, 0, 1, &scissor); // Set scissor dynamically
    }

    void VulkanRHI::CmdEndSwapchainRenderpass(const FrameContext& ctx)
    {
        VkCommandBuffer cmd = mFrame.GetFrame(ctx.FrameIndex).CmdBuffer;
        mImGuiContext.EndFrame(cmd);
        vkCmdEndRenderPass(cmd);
    }

    void VulkanRHI::CmdBeginRenderPass(const FrameContext& ctx, FramebufferHandle h)
    {
        FramebufferEntry* entry = mFramebufferPool.Get(h);
        SG_ASSERT(entry, "CmdBeginRenderPass: invalid FramebufferHandle");

        VkCommandBuffer cmd = mFrame.GetFrame(ctx.FrameIndex).CmdBuffer;

        VkRenderPassBeginInfo rpInfo = {};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass = entry->RenderPass;
        rpInfo.framebuffer = entry->Framebuffer;
        rpInfo.renderArea.extent = { entry->Desc.Width, entry->Desc.Height };
        rpInfo.clearValueCount = entry->ClearCount;
        rpInfo.pClearValues = entry->ClearValues.data();
        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport vp = {};
        vp.width = (float)entry->Desc.Width;
        vp.height = (float)entry->Desc.Height;
        vp.minDepth = 0.0f;
        vp.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &vp);

        VkRect2D scissor = {};
        scissor.extent = { entry->Desc.Width, entry->Desc.Height };
        vkCmdSetScissor(cmd, 0, 1, &scissor);
    }

    void VulkanRHI::CmdEndRenderPass(const FrameContext& ctx, FramebufferHandle h)
    {
        VkCommandBuffer cmd = mFrame.GetFrame(ctx.FrameIndex).CmdBuffer;
        vkCmdEndRenderPass(cmd);

        FramebufferEntry* entry = mFramebufferPool.Get(h);
        // Starting a Renderpass transitions attachments to finalLayout, so we need to update our tracked layout to match
        if (entry)
        {
            for (Uint i = 0; i < entry->Desc.ColorAttachmentCount; i++)
            {
                ImageEntry* tex = mTexturePool.Get(entry->Desc.ColorAttachments[i].Handle);
                if (tex)
                    tex->Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // finalLayout
            }
            if (entry->Desc.HasDepth)
            {
                ImageEntry* depth = mTexturePool.Get(entry->Desc.DepthAttachment.Handle);
                if (depth)
                    depth->Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            }
        }
    }

    VkCommandBuffer VulkanRHI::BeginOneTimeCommands() const
    {
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = mFrame.GetFrame(0).CmdPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(mDevice, &allocInfo, &cmd);

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfo);
        return cmd;
    }

    void VulkanRHI::EndOneTimeCommands(VkCommandBuffer cmd) const
    {
        vkEndCommandBuffer(cmd);

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;

        vkQueueSubmit(mDevice.GetQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(mDevice.GetQueue());
        vkFreeCommandBuffers(mDevice.GetDevice(), mFrame.GetFrame(0).CmdPool, 1, &cmd);
    }

    ImTextureID VulkanRHI::AddImGuiImage(ImageHandle h)
    {
        ImageEntry* entry = mTexturePool.Get(h);
        SG_ASSERT(entry, "AddImGuiImage: invalid TextureHandle");

        return mImGuiContext.AddImage(entry->View);
    }

    ImTextureID VulkanRHI::GetImGuiImage(ImageHandle h)
    {
        ImageEntry* entry = mTexturePool.Get(h);
        SG_ASSERT(entry, "GetImGuiImage: invalid TextureHandle");
        SG_ASSERT(entry->Desc.GenerateImGuiID, "GetImGuiImage: Texture was not created with GenerateImGuiID flag!");
        return entry->ImGuiID;
    }

    void VulkanRHI::DestroyImGuiImage(ImageHandle h)
    {
        ImageEntry* entry = mTexturePool.Get(h);
        SG_ASSERT(entry, "DestroyImGuiImage: invalid TextureHandle");
        SG_ASSERT(entry->Desc.GenerateImGuiID, "DestroyImGuiImage: Texture was not created with GenerateImGuiID flag!");
        mImGuiContext.DestroyImage(entry->ImGuiID);
    }

    void VulkanRHI::ShowPoolDebugImGuiWindow()
    {
        ImFont* regularFont = ImGui::GetIO().Fonts->Fonts[0];
        ImFont* boldFont = ImGui::GetIO().Fonts->Fonts[1];
        ImGui::PushFont(boldFont, 25.0f);
        ImGui::TextUnformatted("Vulkan RHI");
        ImGui::Separator();
        ImGui::PopFont();

        float currentFrameTime = Core::GetClock().GetMilliseconds();
        String frameTimeText = std::format("Frame Time: {:.2f} ms", currentFrameTime);
        ImGui::PushFont(regularFont, 20.0f);
        ImGui::TextUnformatted(frameTimeText.c_str());
        ImGui::PopFont();

        constexpr float boldFontSize = 18.0f;
        ImGui::PushFont(boldFont, boldFontSize);
        ImGui::TextUnformatted("GPU Info");
        ImGui::PopFont();
        ImGui::Text("GPU: %s", mStats.GPUName.c_str());
        ImGui::Text("Vendor: %s", mStats.VendorName.c_str());
        ImGui::Text("%s", mStats.RHIVersion.c_str());
        ImGui::Text("Draw call(s): %i", mStats.DrawCalls);
        ImGui::Text("Frames in Flight: %d", RHISettings::FRAMES_IN_FLIGHT);

        // We need to call GetStats() here to update the memory stats before displaying them
        auto& memStats = GetStats();
        float used = memStats.UsedGPUMemory;
        float total = memStats.TotalAllowedGPUMemory;
        float frac = (total > 0.0f) ? (used / total) : 0.0f;
        ImGui::PushFont(boldFont, boldFontSize);
        ImGui::TextUnformatted("GPU Memory");
        ImGui::PopFont();
        ImGui::Text("Allocations: %llu", memStats.AllocationCount);
        ImGui::Text("%.3f MB / %.3f MB", used, total);
        ImGui::ProgressBar(frac, ImVec2(-1, 0));
        ImGui::Separator();
        ImGui::PushFont(boldFont, boldFontSize);
        ImGui::TextUnformatted("RHI Pools");
        ImGui::PopFont();

        //ImGui::PushFont(boldFont, boldFontSize);
        if (ImGui::TreeNode("BufferPool"))
        {
            //ImGui::PopFont();
            ImGui::Text("Alive objects: %d", mBufferPool.AliveObjCount());
            mBufferPool.ForEachAlive([](const BufferHandle& h, BufferEntry& entry)
                {
                    String bufText = std::format("BufferHandle ({}, {})", h.Index, h.Generation);
                    if (ImGui::TreeNode(bufText.c_str()))
                    {
                        ImGui::Text("Debug Name: %s", entry.Desc.DebugName.c_str());
                        ImGui::Text("Size: %.3f MB", entry.Desc.Size / (1024.0f * 1024.0f));
                        ImGui::Text("Usage: %s", VulkanUtils::BufferUsageToString(entry.Desc.Usage));
                        ImGui::Text("Host Visible: %s", entry.Desc.HostVisible ? "Yes" : "No");
                        ImGui::TreePop();
                    }
                });
            ImGui::TreePop();
        }

        ImGui::Separator();
        //ImGui::PushFont(mImGuiContext.GetBoldFont(), boldFontSize);
        if (ImGui::TreeNode("PipelinePool"))
        {
            //ImGui::PopFont();
            ImGui::Text("Alive objects: %d", mPipelinePool.AliveObjCount());
            mPipelinePool.ForEachAlive([](const PipelineHandle& h, PipelineEntry& entry)
                {
                    String pipeText = std::format("PipelineHandle ({}, {})", h.Index, h.Generation);
                    if (ImGui::TreeNode(pipeText.c_str()))
                    {
                        ImGui::Text("Debug Name: %s", entry.Desc.DebugName.c_str());
                        ImGui::Text("Target: %s", entry.Desc.TargetSwapchain ? "Swapchain" : "Framebuffer");
                        ImGui::Text("Shader: %s", entry.Desc.Shader_.GetName().c_str());
                        ImGui::TreePop();
                    }
                });
            ImGui::TreePop();
        }
        ImGui::Separator();
        //ImGui::PushFont(mImGuiContext.GetBoldFont(), boldFontSize);
        if (ImGui::TreeNode("FramebufferPool"))
        {
            //ImGui::PopFont();
            ImGui::Text("Alive objects: %d", mFramebufferPool.AliveObjCount());
            mFramebufferPool.ForEachAlive([this](const FramebufferHandle& h, FramebufferEntry& entry)
                {
                    FramebufferDesc desc = entry.Desc;
                    String fbufText = std::format("FramebufferHandle ({}, {})", h.Index, h.Generation);
                    if (ImGui::TreeNode(fbufText.c_str()))
                    {
                        ImGui::Text("Debug Name: %s", entry.Desc.DebugName.c_str());
                        ImGui::Text("Dimensions: %dx%d", desc.Width, desc.Height);
                        ImGui::Text("Color Attachment Count: %d", desc.ColorAttachmentCount);
                        if (desc.ColorAttachmentCount > 0)
                        {
                            if (ImGui::TreeNode("ColorAttachments"))
                            {
                                for (Uint i = 0; i < desc.ColorAttachmentCount; i++)
                                {
                                    ImageEntry* texEntry = mTexturePool.Get(desc.ColorAttachments[i].Handle);
                                    ImageDesc tDesc = texEntry->Desc;
                                    String texText = std::format("TextureHandle ({}, {})", h.Index, h.Generation);
                                    if (ImGui::TreeNode(texText.c_str()))
                                    {
                                        ImGui::Text("Debug Name: %s", tDesc.DebugName.c_str());
                                        ImGui::Text("Dimensions: %dx%d", tDesc.Width, tDesc.Height);
                                        ImGui::Text("Format: %s", SurgeReflect::EnumToString(tDesc.Format).data());
                                        ImGui::Text("Usage: %s", VulkanUtils::TextureUsageToString(tDesc.Usage));
                                        ImGui::Text("Size: %.5f MB", texEntry->Size / (1024.0f * 1024.0f));
                                        ImGui::TreePop();
                                    }
                                }
                                ImGui::TreePop();
                            }
                        }

                        ImGui::Text("Has Depth: %s", desc.HasDepth ? "Yes" : "No");
                        if (desc.HasDepth)
                        {
                            ImGui::Text("Depth Attachment:");
                            ImGui::Text("TextureHandle (%d, %d)\nLoadOp: %s\nStoreOp: %s", desc.DepthAttachment.Handle.Index, desc.DepthAttachment.Handle.Generation,
                                        SurgeReflect::EnumToString(desc.DepthAttachment.Load).data(), SurgeReflect::EnumToString(desc.DepthAttachment.Store).data());
                        }
                        ImGui::TreePop();
                    }
                });
            ImGui::TreePop();
        }

        ImGui::Separator();
        //ImGui::PushFont(mImGuiContext.GetBoldFont(), boldFontSize);
        if (ImGui::TreeNode("SamplerPool"))
        {
            //ImGui::PopFont();
            ImGui::Text("Alive objects: %d", mSamplerPool.AliveObjCount());
            mSamplerPool.ForEachAlive([](const SamplerHandle& h, SamplerEntry& entry)
                {
                    SamplerDesc desc = entry.Desc;
                    String texText = std::format("{} ({}, {})", desc.DebugName, h.Index, h.Generation);
                    if (ImGui::TreeNode(texText.c_str()))
                    {
                        ImGui::Text("Debug Name: %s", desc.DebugName.c_str());
                        ImGui::Text("Anisotropy:");
                        ImGui::Text("Enabled: %s", desc.Anisotropy ? "Yes" : "No");
                        ImGui::Text("Max Anisotropy: %.1f", desc.MaxAniso);
                        ImGui::Text("Filter Mode:");
                        ImGui::Text("Min Filter: %s", SurgeReflect::EnumToString(desc.Min).data());
                        ImGui::Text("Mag Filter: %s", SurgeReflect::EnumToString(desc.Mag).data());
                        ImGui::Text("Mipmap Mode: %s", SurgeReflect::EnumToString(desc.Mip).data());
                        ImGui::Text("Wrap Mode:");
                        ImGui::Text("U: %s", SurgeReflect::EnumToString(desc.WrapU).data());
                        ImGui::Text("V: %s", SurgeReflect::EnumToString(desc.WrapV).data());
                        ImGui::Text("Mip Bias: %.2f", desc.MipBias);
                        ImGui::TreePop();
                    }
                });
            ImGui::TreePop();
        }
        ImGui::Separator();
        if (ImGui::TreeNode("TexturePool"))
        {
            ImGui::Text("Alive objects: %d", mTexturePool.AliveObjCount());
            mTexturePool.ForEachAlive([this](const ImageHandle& h, ImageEntry& entry)
                {
                    ImageDesc desc = entry.Desc;
                    String texText = std::format("{} ({}, {})", desc.DebugName, h.Index, h.Generation);
                    if (ImGui::TreeNode(texText.c_str()))
                    {

                        ImGui::Text("Debug Name: %s", desc.DebugName.c_str());
                        ImGui::Text("Dimensions: %dx%d", desc.Width, desc.Height);
                        ImGui::Text("Format: %s", SurgeReflect::EnumToString(desc.Format).data());
                        ImGui::Text("Usage: %s", VulkanUtils::TextureUsageToString(desc.Usage));
                        ImGui::Text("Size: %.5f MB", entry.Size / (1024.0f * 1024.0f));
                        if((desc.Usage & ImageUsage::SAMPLED) && desc.GenerateImGuiID)
                        {
                            float aspectRatio = (float)desc.Width / (float)desc.Height;
                            glm::vec2 imgSize = (aspectRatio > 1.0f) ? ImVec2(200.0f, 200.0f / aspectRatio) : ImVec2(200.0f * aspectRatio, 200.0f);
                            ImGui::Text("Preview: %.2f, %.2f", imgSize.x, imgSize.y);
                            ImGui::Image(entry.ImGuiID, imgSize);
                        }
                        else
                        {
                            ImGui::TextUnformatted("ImGui Preview is not available for this image");
                        }
                        ImGui::TreePop();
                    }
                });
            ImGui::TreePop();
        }
    }

    void VulkanRHI::CreateInstance()
    {
        Log<Severity::Info>("Initializing vulkan instance....");

        VK_CALL(volkInitialize());

        Vector<const char*> requiredInstanceExtensions = GetRequiredInstanceExtensions();
        Vector<const char*> requestedInstanceLayers = GetRequiredInstanceLayers();

        VkApplicationInfo app{
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "SurgePlayer",
            .pEngineName = "SurgeEngine",
            .apiVersion = VK_API_VERSION_1_1 };

        VkInstanceCreateInfo instanceinfo{
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &app,
            .enabledLayerCount = static_cast<Uint>(requestedInstanceLayers.size()),
            .ppEnabledLayerNames = requestedInstanceLayers.data(),
            .enabledExtensionCount = static_cast<Uint>(requiredInstanceExtensions.size()),
            .ppEnabledExtensionNames = requiredInstanceExtensions.data() };

        ENABLE_IF_VK_VALIDATION(mDebugger.Create(instanceinfo));

        // Create the Vulkan instance
        VK_CALL(vkCreateInstance(&instanceinfo, nullptr, &mInstance));
        volkLoadInstance(mInstance);

        ENABLE_IF_VK_VALIDATION(mDebugger.StartDiagnostics(mInstance));
    }

    void VulkanRHI::FillStats()
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(mDevice.GetGPU(), &props);

        mStats.GPUName = props.deviceName;

        Uint major = VK_VERSION_MAJOR(props.apiVersion);
        Uint minor = VK_VERSION_MINOR(props.apiVersion);
        Uint patch = VK_VERSION_PATCH(props.apiVersion);
        mStats.RHIVersion = std::format("Vulkan Version: {}.{}.{}", major, minor, patch);
        
        mStats.VendorName = GetVendorName(props.vendorID);
    }

    void VulkanRHI::CreateSurface(Window* window)
    {
#ifdef SURGE_PLATFORM_WINDOWS
        PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR;

        // Getting the vkCreateWin32SurfaceKHR function pointer and assert if it doesnt exist
        vkCreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(mInstance, "vkCreateWin32SurfaceKHR");		
        SG_ASSERT(vkCreateWin32SurfaceKHR, "[Win32] Vulkan instance missing VK_KHR_win32_surface extension");

        VkWin32SurfaceCreateInfoKHR sci;
        memset(&sci, 0, sizeof(sci)); // Clear the info
        sci.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        sci.hinstance = GetModuleHandle(nullptr);
        sci.hwnd = static_cast<HWND>(window->GetNativeWindowHandle());

        vkCreateWin32SurfaceKHR(mInstance, &sci, nullptr, &mSurface);
#elif defined(SURGE_PLATFORM_ANDROID)
        PFN_vkCreateAndroidSurfaceKHR vkCreateAndroidSurfaceKHR = (PFN_vkCreateAndroidSurfaceKHR)vkGetInstanceProcAddr(mInstance, "vkCreateAndroidSurfaceKHR");
        if (!vkCreateAndroidSurfaceKHR)
            SG_ASSERT_INTERNAL("[Android] Vulkan instance missing VK_KHR_android_surface extension");

        VkAndroidSurfaceCreateInfoKHR sci{};
        memset(&sci, 0, sizeof(sci)); // Clear the info
        sci.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
        sci.window = static_cast<ANativeWindow*>(window->GetNativeWindowHandle());

        vkCreateAndroidSurfaceKHR(mInstance, &sci, nullptr, &mSurface);
#else
        SG_ASSERT_INTERNAL("Surge is currently Windows/Android Only! :(");
#endif
    }

    void VulkanRHI::DestroySurface()
    {
        if (mSurface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
            mSurface = VK_NULL_HANDLE;
        }

    }

    void VulkanRHI::CreateSwapchainFramebuffers()
    {
        Uint imageCount = mSwapchain.GetImageCount();
        mSwapchainFramebuffers.resize(imageCount);

        // Create framebuffer for each swapchain image view
        for (Uint i = 0; i < imageCount; i++)
        {
            VkImageView attachment = mSwapchain.GetFrame(i).View;

            VkFramebufferCreateInfo fbInfo = {};
            fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fbInfo.renderPass = mRenderPass;
            fbInfo.attachmentCount = 1;
            fbInfo.pAttachments = &attachment;
            fbInfo.width = mSwapchain.GetWidth();
            fbInfo.height = mSwapchain.GetHeight();
            fbInfo.layers = 1;

            VK_CALL(vkCreateFramebuffer(mDevice.GetDevice(), &fbInfo, nullptr, &mSwapchainFramebuffers[i]));
            SET_VK_DEBUG_NAME(*this, VK_OBJECT_TYPE_FRAMEBUFFER, (uint64_t)mSwapchainFramebuffers[i], "Swapchain Framebuffer");
        }
    }

    void VulkanRHI::DestroySwapchainFramebuffers()
    {
        for (auto& fb : mSwapchainFramebuffers)
        {
            if (fb != VK_NULL_HANDLE)
                vkDestroyFramebuffer(mDevice.GetDevice(), fb, nullptr);
        }
        mSwapchainFramebuffers.clear();
    }

    void VulkanRHI::CreateSwapchainRenderpass()
    {
        VkDevice device = mDevice.GetDevice();
        // Proudly stolen from Sascha Willems' Vulkan examples:
        VkAttachmentDescription attachment = {};
        attachment.format = mSwapchain.GetDimensions().Format;               // Backbuffer format
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;                          // Not multisampled
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;                   // When ending the frame, we want tiles to be written out
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // After the render pass is complete, we will transition to PRESENT_SRC_KHR layout.
        
        // We have one subpass. This subpass has one color attachment.
        // While executing this subpass, the attachment will be in attachment optimal layout.
        VkAttachmentReference colorRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

        // We will end up with two transitions.
        // The first one happens right before we start subpass #0, where
        // UNDEFINED is transitioned into COLOR_ATTACHMENT_OPTIMAL.
        // The final layout in the render pass attachment states PRESENT_SRC_KHR, so we
        // will get a final transition from COLOR_ATTACHMENT_OPTIMAL to PRESENT_SRC_KHR.
        VkSubpassDescription subpass
        {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorRef,
        };

        // Create a dependency to external events.
        // We need to wait for the WSI semaphore to signal.
        // Only pipeline stages which depend on COLOR_ATTACHMENT_OUTPUT_BIT will
        // actually wait for the semaphore, so we must also wait for that pipeline stage.
        VkSubpassDependency dependency{
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

        // Since we changed the image layout, we need to make the memory visible to
        // color attachment to modify.
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        // Finally, create the renderpass.
        VkRenderPassCreateInfo rp_info{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &attachment,
            .subpassCount = 1,
            .pSubpasses = &subpass,
            .dependencyCount = 1,
            .pDependencies = &dependency };

        VK_CALL(vkCreateRenderPass(device, &rp_info, nullptr, &mRenderPass));
        SET_VK_DEBUG_NAME(*this, VK_OBJECT_TYPE_RENDER_PASS, (uint64_t)mRenderPass, "Swapchain Renderpass");
    }

    void VulkanRHI::DestroySwapchainRenderpass()
    {
        if (mRenderPass != VK_NULL_HANDLE)
            vkDestroyRenderPass(mDevice.GetDevice(), mRenderPass, nullptr);
    }

    void VulkanRHI::ResizeInternal()
    {
        WaitIdle();
        DestroySwapchainFramebuffers();
        mSwapchain.Resize(*this, 0, 0);
        CreateSwapchainFramebuffers();
    }

    void VulkanRHI::CreateDescriptorPools()
    {
        VkDescriptorPoolSize poolSizes[] =
        { {VK_DESCRIPTOR_TYPE_SAMPLER, 10000},
         {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10000},
         {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 10000},
         {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10000},
         {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 10000},
         {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 10000},
         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10000},
         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10000},
         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10000},
         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 10000},
         {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 10000} };

        VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 100 * (sizeof(poolSizes) / sizeof(VkDescriptorPoolSize));
        poolInfo.poolSizeCount = (Uint)(sizeof(poolSizes) / sizeof(VkDescriptorPoolSize));
        poolInfo.pPoolSizes = poolSizes;

        mVkDescriptorPools.resize(RHISettings::FRAMES_IN_FLIGHT);
        for(auto& descriptorPool : mVkDescriptorPools)
        {
            VK_CALL(vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &descriptorPool));
            SET_VK_DEBUG_NAME(*this, VK_OBJECT_TYPE_DESCRIPTOR_POOL, (uint64_t)descriptorPool, "DescriptorPool");
        }

        mNonResetableVkDescriptorPools.resize(RHISettings::FRAMES_IN_FLIGHT);
        for(auto& descriptorPool : mNonResetableVkDescriptorPools)
        {
            VK_CALL(vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &descriptorPool));
            SET_VK_DEBUG_NAME(*this, VK_OBJECT_TYPE_DESCRIPTOR_POOL, (uint64_t)descriptorPool, "NonResetable DescriptorPool");
        }
    }

    Vector<const char*> VulkanRHI::GetRequiredInstanceExtensions()
    {
        Vector<const char*> requiredInstanceExtensions;
        requiredInstanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(SURGE_PLATFORM_ANDROID)
        requiredInstanceExtensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(SURGE_PLATFORM_WINDOWS)
        requiredInstanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(SURGE_PLATFORM_APPLE)
        requiredInstanceExtensions.push_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
#else
#pragma error "Platform not supported"
#endif
        ENABLE_IF_VK_VALIDATION(mDebugger.AddValidationExtensions(requiredInstanceExtensions));

        Uint instanceExtensionCount = 0;
        VK_CALL(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr));
        Vector<VkExtensionProperties> availableInstanceExtensions(instanceExtensionCount);
        VK_CALL(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, availableInstanceExtensions.data()));

        Vector<String> missingExtensions = ValidateExtensions(requiredInstanceExtensions, availableInstanceExtensions);
        if (!missingExtensions.empty())
        {
            for(const auto& ext : missingExtensions)
            {
                Log<Severity::Error>("Missing extension: {}", ext);
                if (ext == VK_EXT_DEBUG_UTILS_EXTENSION_NAME)
                    continue; // Lower android devices doesnt have VK_EXT_debug_utils, they have VK_EXT_debug_report instead, but we can still run without debug utils

                throw std::runtime_error("Required instance extensions are missing!");
            }
        }

        return requiredInstanceExtensions;
    }

    Vector<const char*> VulkanRHI::GetRequiredInstanceLayers()
    {
        Vector<const char*> instanceLayers;
        ENABLE_IF_VK_VALIDATION(mDebugger.AddValidationLayers(instanceLayers));
        return instanceLayers;
    }
}
