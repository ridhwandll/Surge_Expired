// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Memory.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanDiagnostics.hpp"
#include <volk.h>

namespace Surge
{
    class Image2D;
    class SURGE_API VulkanImGuiContext
    {
    public:
        VulkanImGuiContext() = default;
        ~VulkanImGuiContext() = default;

    private:
        void Initialize(void* vulkanRenderContext);
        void Destroy();

        void BeginFrame();
        void Render(); // Must be called inside a "Graphics" command buffer
        void EndFrame();

        void* AddImage(const Ref<Image2D>& image2d) const;
        void SetDarkThemeColors();
        void* GetContext() { return mImGuiContext; }

    private:
        VkDescriptorPool mImguiPool;
        void* mVulkanRenderContext = nullptr;
        void* mImGuiContext;

    private:
        friend class VulkanRenderContext;
    };

} // namespace Surge
