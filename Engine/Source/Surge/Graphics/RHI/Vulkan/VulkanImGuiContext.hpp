// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RHI/Vulkan/VulkanDiagnostics.hpp"
#include "Surge/Graphics/RHI/RHIResources.hpp"
#include <volk.h>

namespace Surge
{
    class SURGE_API VulkanImGuiContext
    {
    public:
        VulkanImGuiContext() = default;
        ~VulkanImGuiContext() = default;

        void Initialize(RHI::SwapchainHandle swapchain);
        void Destroy();

        void BeginFrame();
        void Render();
        void EndFrame();

        void* AddImage(const Ref<Image2D>& image2d) const;
        void* GetContext() { return mImGuiContext; }

    private:
        void SetDarkThemeColors();

    private:
        VkDescriptorPool mImguiPool = VK_NULL_HANDLE;
        void* mImGuiContext = nullptr;
        RHI::SwapchainHandle mSwapchain;
    };

} // namespace Surge
