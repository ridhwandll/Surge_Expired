// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Core.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanDevice.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanDiagnostics.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanImGuiContext.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanSwapChain.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanMemoryAllocator.hpp"
#include "Surge/Graphics/RenderContext.hpp"
#include <volk.h>

#define SURGE_GET_VULKAN_CONTEXT(renderContext) renderContext = static_cast<::Surge::VulkanRenderContext*>(::Surge::Core::GetRenderContext())

namespace Surge
{
    class SURGE_API VulkanRenderContext : public RenderContext
    {
    public:
        virtual void Initialize(Window* window, bool enableImGui = true) override;
        virtual void BeginFrame() override;
        virtual void EndFrame() override;
        virtual void Shutdown() override;
        virtual void OnResize() override;
        virtual void RenderImGui() override;
        virtual void SetBlitSourceImage(const Ref<Image2D>& image) override { mBlitSourceImage = image; }

        Uint GetFrameIndex() const override { return mSwapChain.GetCurrentFrameIndex(); }
        virtual GPUMemoryStats GetMemoryStatus() const override { return mMemoryAllocator.GetStats(); };
        virtual GPUInfo GetGPUInfo() const override { return mGPUInfo; }

        VkInstance GetInstance() const { return mVulkanInstance; }
        VulkanDevice* GetDevice() { return &mDevice; }
        VulkanSwapChain* GetSwapChain() { return &mSwapChain; }
        VulkanMemoryAllocator* GetMemoryAllocator() { return &mMemoryAllocator; }

        const Vector<VkDescriptorPool>& GetDescriptorPools() const { return mDescriptorPools; }
        const Vector<VkDescriptorPool>& GetNonResetableDescriptorPools() const { return mNonResetableDescriptorPools; }

        virtual void* GetImGuiTextureID(const Ref<Image2D>& image) const;
        virtual void* GetImGuiContext() { return mImGuiContext.GetContext(); }

    private:
        Vector<const char*> GetRequiredInstanceExtensions();
        Vector<const char*> GetRequiredInstanceLayers();
        void CreateDescriptorPools();

    private:
        VkInstance mVulkanInstance = VK_NULL_HANDLE;
        VulkanDiagnostics mVulkanDiagnostics {};
        VulkanDevice mDevice {};
        VulkanSwapChain mSwapChain {};
        VulkanMemoryAllocator mMemoryAllocator {};
        VulkanImGuiContext mImGuiContext;
        Ref<Image2D> mBlitSourceImage;
        bool mImGuiEnabled;

        // Descriptor Pools
        Vector<VkDescriptorPool> mDescriptorPools;
        Vector<VkDescriptorPool> mNonResetableDescriptorPools;

        GPUInfo mGPUInfo;
        friend class SURGE_API VulkanImGuiContext;
        friend class SURGE_API VulkanDevice;
    };

    template <typename T>
    FORCEINLINE void SetDebugVkResourceName(T handle, VkObjectType type, const char* name)
    {
        static_assert(sizeof(T) == sizeof(uint64_t), "Invalid handle");
        VkDebugUtilsObjectNameInfoEXT info {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        info.objectType = type;
        info.objectHandle = reinterpret_cast<uint64_t>(handle);
        info.pObjectName = name;

        VulkanRenderContext* renderContext;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        vkSetDebugUtilsObjectNameEXT(renderContext->GetDevice()->GetLogicalDevice(), &info);
    }

#if SURGE_DEBUG
#define SET_VK_OBJECT_DEBUGNAME(handle, type, name) SetDebugVkResourceName(handle, type, name)
#else
#define SET_VK_OBJECT_DEBUGNAME(handle, type, name)
#endif
} // namespace Surge