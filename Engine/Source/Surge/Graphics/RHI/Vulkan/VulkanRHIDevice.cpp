// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RHI/Vulkan/VulkanRHIDevice.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHICommandBuffer.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHISwapchain.hpp"
#include <volk.h>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

namespace Surge::RHI
{
    VulkanRHIDevice gVulkanRHIDevice;

    // ── Buffer operations ────────────────────────────────────────────────────

    static VkBufferUsageFlags BufferUsageToVk(BufferUsage usage)
    {
        VkBufferUsageFlags flags = 0;
        if (usage & BufferUsage::VertexBuffer)   flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        if (usage & BufferUsage::IndexBuffer)    flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if (usage & BufferUsage::UniformBuffer)  flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        if (usage & BufferUsage::StorageBuffer)  flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        if (usage & BufferUsage::TransferSrc)    flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        if (usage & BufferUsage::TransferDst)    flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        if (usage & BufferUsage::IndirectBuffer) flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
        return flags;
    }

    static VmaMemoryUsage GPUMemUsageToVma(GPUMemoryUsage usage)
    {
        switch (usage)
        {
            case GPUMemoryUsage::GPUOnly:            return VMA_MEMORY_USAGE_GPU_ONLY;
            case GPUMemoryUsage::CPUOnly:            return VMA_MEMORY_USAGE_CPU_ONLY;
            case GPUMemoryUsage::CPUToGPU:           return VMA_MEMORY_USAGE_CPU_TO_GPU;
            case GPUMemoryUsage::GPUToCPU:           return VMA_MEMORY_USAGE_GPU_TO_CPU;
            case GPUMemoryUsage::CPUCopy:            return VMA_MEMORY_USAGE_CPU_COPY;
            case GPUMemoryUsage::GPULazilyAllocated: return VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED;
            default:                                  return VMA_MEMORY_USAGE_UNKNOWN;
        }
    }

    static BufferHandle sCreateBuffer(const BufferDesc& desc)
    {
        auto& reg = GetVulkanRegistry();

        VkBufferCreateInfo bufInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufInfo.size        = desc.Size;
        bufInfo.usage       = BufferUsageToVk(desc.Usage);
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = GPUMemUsageToVma(desc.MemoryUsage);
        if (desc.MemoryUsage == GPUMemoryUsage::CPUToGPU || desc.MemoryUsage == GPUMemoryUsage::CPUOnly)
            allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VkBuffer      vkBuf;
        VmaAllocation allocation;
        vmaCreateBuffer(gVulkanRHIDevice.GetVMAAllocator(), &bufInfo, &allocInfo, &vkBuf, &allocation, nullptr);

        VulkanBuffer vb;
        vb.Buffer     = vkBuf;
        vb.Allocation = allocation;
        vb.Size       = desc.Size;
        vb.Usage      = desc.Usage;
        vb.MemUsage   = desc.MemoryUsage;

        return reg.Buffers.Create(std::move(vb));
    }

    static void sDestroyBuffer(BufferHandle handle)
    {
        auto& reg = GetVulkanRegistry();
        auto& vb  = reg.GetBuffer(handle);
        vmaDestroyBuffer(gVulkanRHIDevice.GetVMAAllocator(), vb.Buffer, vb.Allocation);
        reg.Buffers.Destroy(handle);
    }

    static void sUploadBuffer(BufferHandle handle, const void* data, size_t size, size_t offset)
    {
        auto& vb   = GetVulkanRegistry().GetBuffer(handle);
        void* mapped = nullptr;
        vmaMapMemory(gVulkanRHIDevice.GetVMAAllocator(), vb.Allocation, &mapped);
        memcpy(static_cast<uint8_t*>(mapped) + offset, data, size);
        vmaUnmapMemory(gVulkanRHIDevice.GetVMAAllocator(), vb.Allocation);
    }

    static void* sMapBuffer(BufferHandle handle)
    {
        auto& vb   = GetVulkanRegistry().GetBuffer(handle);
        void* ptr  = nullptr;
        vmaMapMemory(gVulkanRHIDevice.GetVMAAllocator(), vb.Allocation, &ptr);
        return ptr;
    }

    static void sUnmapBuffer(BufferHandle handle)
    {
        auto& vb = GetVulkanRegistry().GetBuffer(handle);
        vmaUnmapMemory(gVulkanRHIDevice.GetVMAAllocator(), vb.Allocation);
    }

    // ── Shader operations ─────────────────────────────────────────────────────

    static ShaderHandle sCreateShader(const ShaderDesc& desc)
    {
        VkShaderModuleCreateInfo info = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        info.codeSize = desc.SpirvSize;
        info.pCode    = desc.SpirvData;

        VkShaderModule module = VK_NULL_HANDLE;
        vkCreateShaderModule(gVulkanRHIDevice.GetLogicalDevice(), &info, nullptr, &module);

        VulkanShader vs;
        vs.Module = module;
        vs.Stage  = desc.Stage;
        return GetVulkanRegistry().Shaders.Create(std::move(vs));
    }

    static void sDestroyShader(ShaderHandle handle)
    {
        auto& reg = GetVulkanRegistry();
        vkDestroyShaderModule(gVulkanRHIDevice.GetLogicalDevice(), reg.GetShader(handle).Module, nullptr);
        reg.Shaders.Destroy(handle);
    }

    // ── Render pass ───────────────────────────────────────────────────────────

    static VkFormat RHIFormatToVk(ImageFormat fmt)
    {
        switch (fmt)
        {
            case ImageFormat::R8:              return VK_FORMAT_R8_UNORM;
            case ImageFormat::R16F:            return VK_FORMAT_R16_SFLOAT;
            case ImageFormat::R32F:            return VK_FORMAT_R32_SFLOAT;
            case ImageFormat::RG8:             return VK_FORMAT_R8G8_UNORM;
            case ImageFormat::RG16F:           return VK_FORMAT_R16G16_SFLOAT;
            case ImageFormat::RG32F:           return VK_FORMAT_R32G32_SFLOAT;
            case ImageFormat::RGBA8:           return VK_FORMAT_R8G8B8A8_UNORM;
            case ImageFormat::BGRA8:           return VK_FORMAT_B8G8R8A8_UNORM;
            case ImageFormat::RGBA16F:         return VK_FORMAT_R16G16B16A16_SFLOAT;
            case ImageFormat::RGBA32F:         return VK_FORMAT_R32G32B32A32_SFLOAT;
            case ImageFormat::Depth16:         return VK_FORMAT_D16_UNORM;
            case ImageFormat::Depth32F:        return VK_FORMAT_D32_SFLOAT;
            case ImageFormat::Depth24Stencil8: return VK_FORMAT_D24_UNORM_S8_UINT;
            default:                           return VK_FORMAT_UNDEFINED;
        }
    }

    static VkAttachmentLoadOp  LoadOpToVk(LoadOp  op) { return op  == LoadOp::Load   ? VK_ATTACHMENT_LOAD_OP_LOAD  : (op  == LoadOp::Clear  ? VK_ATTACHMENT_LOAD_OP_CLEAR  : VK_ATTACHMENT_LOAD_OP_DONT_CARE); }
    static VkAttachmentStoreOp StoreOpToVk(StoreOp op) { return op == StoreOp::Store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE; }

    static RenderPassHandle sCreateRenderPass(const RenderPassDesc& desc)
    {
        Vector<VkAttachmentDescription> attachments;
        Vector<VkAttachmentReference>   colorRefs;

        for (uint32_t i = 0; i < desc.ColorAttachmentCount; ++i)
        {
            const AttachmentDesc& a = desc.ColorAttachments[i];
            VkAttachmentDescription ad = {};
            ad.format         = RHIFormatToVk(a.Format);
            ad.samples        = VK_SAMPLE_COUNT_1_BIT;
            ad.loadOp         = LoadOpToVk(a.LoadOp_);
            ad.storeOp        = StoreOpToVk(a.StoreOp_);
            ad.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            ad.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            ad.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
            ad.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachments.push_back(ad);

            VkAttachmentReference ref = {};
            ref.attachment = static_cast<uint32_t>(attachments.size() - 1);
            ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorRefs.push_back(ref);
        }

        VkAttachmentReference depthRef = {};
        if (desc.HasDepth)
        {
            const AttachmentDesc& d = desc.DepthAttachment;
            VkAttachmentDescription ad = {};
            ad.format         = RHIFormatToVk(d.Format);
            ad.samples        = VK_SAMPLE_COUNT_1_BIT;
            ad.loadOp         = LoadOpToVk(d.LoadOp_);
            ad.storeOp        = StoreOpToVk(d.StoreOp_);
            ad.stencilLoadOp  = LoadOpToVk(d.StencilLoad);
            ad.stencilStoreOp = StoreOpToVk(d.StencilStore);
            ad.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
            ad.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            attachments.push_back(ad);

            depthRef.attachment = static_cast<uint32_t>(attachments.size() - 1);
            depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = static_cast<uint32_t>(colorRefs.size());
        subpass.pColorAttachments    = colorRefs.empty() ? nullptr : colorRefs.data();
        subpass.pDepthStencilAttachment = desc.HasDepth ? &depthRef : nullptr;

        VkSubpassDependency dependency = {};
        dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass    = 0;
        dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo rpInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        rpInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        rpInfo.pAttachments    = attachments.empty() ? nullptr : attachments.data();
        rpInfo.subpassCount    = 1;
        rpInfo.pSubpasses      = &subpass;
        rpInfo.dependencyCount = 1;
        rpInfo.pDependencies   = &dependency;

        VkRenderPass vkRP = VK_NULL_HANDLE;
        vkCreateRenderPass(gVulkanRHIDevice.GetLogicalDevice(), &rpInfo, nullptr, &vkRP);

        VulkanRenderPass rp;
        rp.RenderPass = vkRP;
        return GetVulkanRegistry().RenderPasses.Create(std::move(rp));
    }

    static void sDestroyRenderPass(RenderPassHandle handle)
    {
        auto& reg = GetVulkanRegistry();
        vkDestroyRenderPass(gVulkanRHIDevice.GetLogicalDevice(), reg.GetRenderPass(handle).RenderPass, nullptr);
        reg.RenderPasses.Destroy(handle);
    }

    // ── Framebuffer ───────────────────────────────────────────────────────────

    static FramebufferHandle sCreateFramebuffer(const FramebufferDesc& desc)
    {
        auto& reg = GetVulkanRegistry();
        VkRenderPass vkRP = reg.GetRenderPass(desc.RenderPass).RenderPass;

        Vector<VkImageView> views;
        for (uint32_t i = 0; i < desc.ColorAttachmentCount; ++i)
            views.push_back(reg.GetTexture(desc.ColorAttachments[i]).ImageView);
        if (desc.HasDepth)
            views.push_back(reg.GetTexture(desc.DepthAttachment).ImageView);

        VkFramebufferCreateInfo fbInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fbInfo.renderPass      = vkRP;
        fbInfo.attachmentCount = static_cast<uint32_t>(views.size());
        fbInfo.pAttachments    = views.empty() ? nullptr : views.data();
        fbInfo.width           = desc.Width;
        fbInfo.height          = desc.Height;
        fbInfo.layers          = 1;

        VkFramebuffer vkFB = VK_NULL_HANDLE;
        vkCreateFramebuffer(gVulkanRHIDevice.GetLogicalDevice(), &fbInfo, nullptr, &vkFB);

        VulkanFramebuffer fb;
        fb.Framebuffer = vkFB;
        fb.RenderPass  = vkRP;
        fb.Width       = desc.Width;
        fb.Height      = desc.Height;
        return reg.Framebuffers.Create(std::move(fb));
    }

    static void sDestroyFramebuffer(FramebufferHandle handle)
    {
        auto& reg = GetVulkanRegistry();
        vkDestroyFramebuffer(gVulkanRHIDevice.GetLogicalDevice(), reg.GetFramebuffer(handle).Framebuffer, nullptr);
        reg.Framebuffers.Destroy(handle);
    }

    // ── Immediate submit ──────────────────────────────────────────────────────

    void VulkanRHIDevice::InstantSubmit(QueueType queue, const std::function<void(VkCommandBuffer)>& fn)
    {
        VkCommandPool pool = VK_NULL_HANDLE;
        VkQueue       vkQ  = VK_NULL_HANDLE;
        switch (queue)
        {
            case QueueType::Compute:  pool = mImmediateComputePool;  vkQ = mComputeQueue;  break;
            case QueueType::Transfer: pool = mImmediateTransferPool; vkQ = mTransferQueue; break;
            default:                  pool = mImmediateGraphicsPool; vkQ = mGraphicsQueue; break;
        }

        VkCommandBufferAllocateInfo ai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.commandPool        = pool;
        ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;

        VkCommandBuffer cmd = VK_NULL_HANDLE;
        vkAllocateCommandBuffers(mDevice, &ai, &cmd);

        VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &bi);

        fn(cmd);

        vkEndCommandBuffer(cmd);

        VkFenceCreateInfo fi = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        VkFence fence = VK_NULL_HANDLE;
        vkCreateFence(mDevice, &fi, nullptr, &fence);

        VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1;
        si.pCommandBuffers    = &cmd;
        vkQueueSubmit(vkQ, 1, &si, fence);
        vkWaitForFences(mDevice, 1, &fence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(mDevice, fence, nullptr);
        vkFreeCommandBuffers(mDevice, pool, 1, &cmd);
    }

    // ── Descriptor pools ─────────────────────────────────────────────────────

    void VulkanRHIDevice::CreateDescriptorPools()
    {
        VkDescriptorPoolSize poolSizes[] = {
            {VK_DESCRIPTOR_TYPE_SAMPLER,                10000},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10000},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          10000},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          10000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   10000},
            {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   10000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         10000},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         10000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10000},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 10000},
            {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       10000},
        };

        VkDescriptorPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets       = 1000 * (uint32_t)(sizeof(poolSizes) / sizeof(poolSizes[0]));
        poolInfo.poolSizeCount = (uint32_t)(sizeof(poolSizes) / sizeof(poolSizes[0]));
        poolInfo.pPoolSizes    = poolSizes;

        for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
        {
            vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &mDescriptorPools[i]);
            vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &mPersistentDescriptorPools[i]);
        }
    }

    void VulkanRHIDevice::ResetDescriptorPool(uint32_t frameIndex)
    {
        if (frameIndex < FRAMES_IN_FLIGHT)
            vkResetDescriptorPool(mDevice, mDescriptorPools[frameIndex], 0);
    }

    // ── Dispatch table ─────────────────────────────────────────────────────────
    void VulkanRHIDevice::FillDispatchTable()
    {
        mDispatchTable.createBuffer     = sCreateBuffer;
        mDispatchTable.destroyBuffer    = sDestroyBuffer;
        mDispatchTable.uploadBuffer     = sUploadBuffer;
        mDispatchTable.mapBuffer        = sMapBuffer;
        mDispatchTable.unmapBuffer      = sUnmapBuffer;

        mDispatchTable.createShader     = sCreateShader;
        mDispatchTable.destroyShader    = sDestroyShader;

        mDispatchTable.createRenderPass   = sCreateRenderPass;
        mDispatchTable.destroyRenderPass  = sDestroyRenderPass;
        mDispatchTable.createFramebuffer  = sCreateFramebuffer;
        mDispatchTable.destroyFramebuffer = sDestroyFramebuffer;

        // Command buffer operations (implemented in VulkanRHICommandBuffer.cpp)
        mDispatchTable.allocCommandBuffer  = vkRHIAllocCommandBuffer;
        mDispatchTable.freeCommandBuffer   = vkRHIFreeCommandBuffer;
        mDispatchTable.beginCommandBuffer  = vkRHIBeginCommandBuffer;
        mDispatchTable.endCommandBuffer    = vkRHIEndCommandBuffer;
        mDispatchTable.submitCommandBuffer = vkRHISubmitCommandBuffer;

        mDispatchTable.cmdBeginRenderPass      = vkRHICmdBeginRenderPass;
        mDispatchTable.cmdEndRenderPass        = vkRHICmdEndRenderPass;
        mDispatchTable.cmdBindGraphicsPipeline = vkRHICmdBindGraphicsPipeline;
        mDispatchTable.cmdBindComputePipeline  = vkRHICmdBindComputePipeline;
        mDispatchTable.cmdBindVertexBuffer     = vkRHICmdBindVertexBuffer;
        mDispatchTable.cmdBindIndexBuffer      = vkRHICmdBindIndexBuffer;
        mDispatchTable.cmdDrawIndexed          = vkRHICmdDrawIndexed;
        mDispatchTable.cmdDraw                 = vkRHICmdDraw;
        mDispatchTable.cmdDispatch             = vkRHICmdDispatch;
        mDispatchTable.cmdSetViewport          = vkRHICmdSetViewport;
        mDispatchTable.cmdSetScissor           = vkRHICmdSetScissor;
        mDispatchTable.cmdPushConstants        = vkRHICmdPushConstants;
        mDispatchTable.cmdCopyBuffer           = vkRHICmdCopyBuffer;

        // Swapchain operations (implemented in VulkanRHISwapchain.cpp)
        mDispatchTable.createSwapchain         = vkRHICreateSwapchain;
        mDispatchTable.destroySwapchain        = vkRHIDestroySwapchain;
        mDispatchTable.swapchainBeginFrame     = vkRHISwapchainBeginFrame;
        mDispatchTable.swapchainEndFrame       = vkRHISwapchainEndFrame;
        mDispatchTable.swapchainResize         = vkRHISwapchainResize;
        mDispatchTable.swapchainGetFrameIndex  = vkRHISwapchainGetFrameIndex;
        mDispatchTable.swapchainGetImageCount  = vkRHISwapchainGetImageCount;
    }

    // ── Instance creation ─────────────────────────────────────────────────────

    void VulkanRHIDevice::CreateInstance()
    {
        VK_CALL(volkInitialize());

        VkApplicationInfo appInfo  = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
        appInfo.pApplicationName   = "Surge";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName        = "Surge Engine";
        appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion         = VK_API_VERSION_1_2;

        Vector<const char*> extensions;
        extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(SURGE_WINDOWS)
        extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif

        VkInstanceCreateInfo ci = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        ci.pApplicationInfo        = &appInfo;
        ci.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
        ci.ppEnabledExtensionNames = extensions.data();

        VK_CALL(vkCreateInstance(&ci, nullptr, &mInstance));
        volkLoadInstance(mInstance);
    }

    void VulkanRHIDevice::SelectPhysicalDevice()
    {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(mInstance, &count, nullptr);
        SG_ASSERT(count > 0, "VulkanRHIDevice: no Vulkan-capable GPU found");

        Vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(mInstance, &count, devices.data());

        // Prefer discrete GPU; fall back to first available.
        mPhysicalDevice = devices[0];
        for (VkPhysicalDevice pd : devices)
        {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(pd, &props);
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                mPhysicalDevice = pd;
                break;
            }
        }
    }

    void VulkanRHIDevice::CreateLogicalDevice()
    {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueFamilyCount, nullptr);
        Vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilyCount; ++i)
        {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) mGraphicsFamily = i;
            if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)  mComputeFamily  = i;
            if (queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT) mTransferFamily = i;
        }

        float priority = 1.0f;
        Vector<VkDeviceQueueCreateInfo> queueCIs;
        for (uint32_t family : {mGraphicsFamily, mComputeFamily, mTransferFamily})
        {
            bool duplicate = false;
            for (auto& ci : queueCIs)
                if (ci.queueFamilyIndex == family) { duplicate = true; break; }
            if (!duplicate)
            {
                VkDeviceQueueCreateInfo qi = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
                qi.queueFamilyIndex = family;
                qi.queueCount       = 1;
                qi.pQueuePriorities = &priority;
                queueCIs.push_back(qi);
            }
        }

        Vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        VkDeviceCreateInfo ci   = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        ci.queueCreateInfoCount = static_cast<uint32_t>(queueCIs.size());
        ci.pQueueCreateInfos    = queueCIs.data();
        ci.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size());
        ci.ppEnabledExtensionNames = deviceExtensions.data();

        VK_CALL(vkCreateDevice(mPhysicalDevice, &ci, nullptr, &mDevice));

        vkGetDeviceQueue(mDevice, mGraphicsFamily, 0, &mGraphicsQueue);
        vkGetDeviceQueue(mDevice, mComputeFamily,  0, &mComputeQueue);
        vkGetDeviceQueue(mDevice, mTransferFamily, 0, &mTransferQueue);

        // Command pools for InstantSubmit
        auto makePool = [&](uint32_t family, VkCommandPool& out)
        {
            VkCommandPoolCreateInfo ci = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
            ci.queueFamilyIndex = family;
            ci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            vkCreateCommandPool(mDevice, &ci, nullptr, &out);
        };
        makePool(mGraphicsFamily, mImmediateGraphicsPool);
        makePool(mComputeFamily,  mImmediateComputePool);
        makePool(mTransferFamily, mImmediateTransferPool);
    }

    void VulkanRHIDevice::CreateVMAAllocator()
    {
        VmaAllocatorCreateInfo ai = {};
        ai.vulkanApiVersion = VK_API_VERSION_1_2;
        ai.physicalDevice   = mPhysicalDevice;
        ai.device           = mDevice;
        ai.instance         = mInstance;
        VK_CALL(vmaCreateAllocator(&ai, &mVMAAllocator));
    }

    void VulkanRHIDevice::Initialize(void* windowHandle, uint32_t width, uint32_t height)
    {
        // windowHandle, width, and height are forwarded to the swapchain
        // once it is created via rhiCreateSwapchain() with a SwapchainDesc.
        // They are stored here for convenience if the device needs to recreate
        // the swapchain internally (e.g. on resize).
        mInitialWindowHandle = windowHandle;
        mInitialWidth        = width;
        mInitialHeight       = height;

        CreateInstance();
        SelectPhysicalDevice();
        CreateLogicalDevice();
        CreateVMAAllocator();
        CreateDescriptorPools();
        FillDispatchTable();
        SetRHIDevice(&mDispatchTable);
    }

    void VulkanRHIDevice::Shutdown()
    {
        if (mDevice != VK_NULL_HANDLE)
            vkDeviceWaitIdle(mDevice);

        for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
        {
            if (mDescriptorPools[i])           { vkDestroyDescriptorPool(mDevice, mDescriptorPools[i], nullptr);           mDescriptorPools[i] = VK_NULL_HANDLE; }
            if (mPersistentDescriptorPools[i]) { vkDestroyDescriptorPool(mDevice, mPersistentDescriptorPools[i], nullptr); mPersistentDescriptorPools[i] = VK_NULL_HANDLE; }
        }

        if (mImmediateGraphicsPool) { vkDestroyCommandPool(mDevice, mImmediateGraphicsPool, nullptr);  mImmediateGraphicsPool  = VK_NULL_HANDLE; }
        if (mImmediateComputePool)  { vkDestroyCommandPool(mDevice, mImmediateComputePool,  nullptr);  mImmediateComputePool   = VK_NULL_HANDLE; }
        if (mImmediateTransferPool) { vkDestroyCommandPool(mDevice, mImmediateTransferPool, nullptr);  mImmediateTransferPool  = VK_NULL_HANDLE; }

        if (mVMAAllocator)
        {
            vmaDestroyAllocator(mVMAAllocator);
            mVMAAllocator = nullptr;
        }

        if (mDevice != VK_NULL_HANDLE)
        {
            vkDestroyDevice(mDevice, nullptr);
            mDevice = VK_NULL_HANDLE;
        }

        if (mInstance != VK_NULL_HANDLE)
        {
            vkDestroyInstance(mInstance, nullptr);
            mInstance = VK_NULL_HANDLE;
        }

        if (GetRHIDevice() == &mDispatchTable)
            SetRHIDevice(nullptr);
    }

} // namespace Surge::RHI
