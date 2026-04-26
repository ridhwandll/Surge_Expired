// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Abstraction/Vulkan/VulkanRenderContext.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanDiagnostics.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanImage.hpp"

// clang-format off
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
// clang-format on

namespace Surge
{
    void VulkanRenderContext::Initialize(Window* window, bool enableImGui)
    {
        SURGE_PROFILE_FUNC("VulkanRenderContext::Initialize()");
        VK_CALL(volkInitialize());
        mImGuiEnabled = enableImGui;

        /// VkApplicationInfo ///
        VkApplicationInfo appInfo {VK_STRUCTURE_TYPE_APPLICATION_INFO};
        appInfo.pApplicationName = "SurgeProtector";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "Surge Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);

        // Choose the highest supported Vulkan instance version (cap at 1.2)
        {
            uint32_t instanceVersion = VK_API_VERSION_1_0;
            if (vkEnumerateInstanceVersion)
                vkEnumerateInstanceVersion(&instanceVersion);

            if (instanceVersion >= VK_API_VERSION_1_2)
                appInfo.apiVersion = VK_API_VERSION_1_2;
            else if (instanceVersion >= VK_API_VERSION_1_1)
                appInfo.apiVersion = VK_API_VERSION_1_1;
            else
                appInfo.apiVersion = VK_API_VERSION_1_0;
        }

        /// VkInstanceCreateInfo ///
        Vector<const char*> instanceExtensions = GetRequiredInstanceExtensions();
        Vector<const char*> instanceLayers = GetRequiredInstanceLayers();

        VkInstanceCreateInfo createInfo {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledLayerCount = static_cast<Uint>(instanceLayers.size());
        createInfo.ppEnabledLayerNames = instanceLayers.data();
        createInfo.enabledExtensionCount = static_cast<Uint>(instanceExtensions.size());
        createInfo.ppEnabledExtensionNames = instanceExtensions.data();
        createInfo.pNext = nullptr;

        ENABLE_IF_VK_VALIDATION(mVulkanDiagnostics.Create(createInfo));
        VK_CALL(vkCreateInstance(&createInfo, nullptr, &mVulkanInstance));
        ENABLE_IF_VK_VALIDATION(mVulkanDiagnostics.StartDiagnostics(mVulkanInstance));
        volkLoadInstance(mVulkanInstance);

        mDevice.Initialize(mVulkanInstance);
        mSwapChain.Initialize(window);
        mMemoryAllocator.Initialize(mVulkanInstance, mDevice);

        if (mImGuiEnabled)
            mImGuiContext.Initialize(this);

        CreateDescriptorPools();

        // Fill In GPUInfo
        mGPUInfo.Name = mDevice.GetProperties().vk10Properties.properties.deviceName;
        mGPUInfo.DeviceScore = mDevice.GetDeviceScore();
    }

    void VulkanRenderContext::BeginFrame()
    {
        SURGE_PROFILE_FUNC("VulkanRenderContext::BeginFrame()");

        mSwapChain.BeginFrame();
        if (mImGuiEnabled)
            mImGuiContext.BeginFrame();

        // Reset the descriptor pool
        VK_CALL(vkResetDescriptorPool(mDevice.GetLogicalDevice(), mDescriptorPools[mSwapChain.GetCurrentFrameIndex()], 0));
    }

    void VulkanRenderContext::EndFrame()
    {
        SURGE_PROFILE_FUNC("VulkanRenderContext::EndFrame()");
        VkImage blitSrcImage = VK_NULL_HANDLE;
        VkExtent2D blitSrcExtent = {};
        if (mBlitSourceImage)
        {
            Ref<VulkanImage2D> vkImage = mBlitSourceImage.As<VulkanImage2D>();
            blitSrcImage = vkImage->GetVulkanImage();
            blitSrcExtent = {vkImage->GetWidth(), vkImage->GetHeight()};
            mBlitSourceImage = nullptr;
        }

        mSwapChain.EndFrame(blitSrcImage, blitSrcExtent); // Present
        if (mImGuiEnabled)
            mImGuiContext.EndFrame();
    }

    void VulkanRenderContext::Shutdown()
    {
        SURGE_PROFILE_FUNC("VulkanRenderContext::Shutdown()");
        VkDevice device = mDevice.GetLogicalDevice();
        vkDeviceWaitIdle(device);

        if (mImGuiEnabled)
            mImGuiContext.Destroy();

        // Destroy the descriptor pools
        for (VkDescriptorPool& pool : mDescriptorPools)
            vkDestroyDescriptorPool(device, pool, nullptr);
        for (VkDescriptorPool& pool : mNonResetableDescriptorPools)
            vkDestroyDescriptorPool(device, pool, nullptr);

        mMemoryAllocator.Destroy();
        mSwapChain.Destroy();
        ENABLE_IF_VK_VALIDATION(mVulkanDiagnostics.EndDiagnostics(mVulkanInstance));
        mDevice.Destroy();
        vkDestroyInstance(mVulkanInstance, nullptr);
    }

    void VulkanRenderContext::OnResize()
    {
        mSwapChain.Resize();
    }

    void VulkanRenderContext::RenderImGui()
    {
        if (!mImGuiEnabled)
            return;

        SURGE_PROFILE_FUNC("VulkanRenderContext::RenderImGui()");
        mImGuiContext.Render();
    }

    void* VulkanRenderContext::GetImGuiTextureID(const Ref<Image2D>& image) const
    {
        if (mImGuiEnabled)
        {
            return mImGuiContext.AddImage(image);
        }
        return nullptr;
    }

    Vector<const char*> VulkanRenderContext::GetRequiredInstanceExtensions()
    {
        Vector<const char*> instanceExtensions;
        instanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#ifdef SURGE_PLATFORM_WINDOWS
        instanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(SURGE_PLATFORM_ANDROID)
        instanceExtensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#endif
        ENABLE_IF_VK_VALIDATION(mVulkanDiagnostics.AddValidationExtensions(instanceExtensions));
        return instanceExtensions;
    }

    Vector<const char*> VulkanRenderContext::GetRequiredInstanceLayers()
    {
        Vector<const char*> instanceLayers;
        ENABLE_IF_VK_VALIDATION(mVulkanDiagnostics.AddValidationLayers(instanceLayers));
        return instanceLayers;
    }

    void VulkanRenderContext::CreateDescriptorPools()
    {
        VkDescriptorPoolSize poolSizes[] =
            {{VK_DESCRIPTOR_TYPE_SAMPLER, 10000},
             {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10000},
             {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 10000},
             {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10000},
             {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 10000},
             {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 10000},
             {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10000},
             {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10000},
             {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10000},
             {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 10000},
             {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 10000}};

        VkDescriptorPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 100 * (sizeof(poolSizes) / sizeof(VkDescriptorPoolSize));
        poolInfo.poolSizeCount = (Uint)(sizeof(poolSizes) / sizeof(VkDescriptorPoolSize));
        poolInfo.pPoolSizes = poolSizes;

        mDescriptorPools.resize(FRAMES_IN_FLIGHT);
        for (auto& descriptorPool : mDescriptorPools)
        {
            VK_CALL(vkCreateDescriptorPool(mDevice.GetLogicalDevice(), &poolInfo, nullptr, &descriptorPool));
            SET_VK_OBJECT_DEBUGNAME(descriptorPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL, "DescriptorPool");
        }

        mNonResetableDescriptorPools.resize(FRAMES_IN_FLIGHT);
        for (auto& descriptorPool : mNonResetableDescriptorPools)
        {
            VK_CALL(vkCreateDescriptorPool(mDevice.GetLogicalDevice(), &poolInfo, nullptr, &descriptorPool));
            SET_VK_OBJECT_DEBUGNAME(descriptorPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL, "NonResetable DescriptorPool");
        }
    }

} // namespace Surge
