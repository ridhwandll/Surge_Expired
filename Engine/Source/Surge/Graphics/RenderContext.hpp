// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Window/Window.hpp"
#include "Interface/Image.hpp"

namespace Surge
{
    struct GPUInfo
    {
        String Name;
        int64_t DeviceScore;
        // TODO: Add more stuff?
    };

    struct GPUMemoryStats
    {
        GPUMemoryStats() = default;
        GPUMemoryStats(uint64_t used, uint64_t free) : Used(used), Free(free) {}

        uint64_t Used = 0;
        uint64_t Free = 0;
    };

    enum class SURGE_API GPUMemoryUsage
    {
        Unknown = 0,
        GPUOnly,
        CPUOnly,
        CPUToGPU,
        GPUToCPU,
        CPUCopy,
        GPULazilyAllocated
    };

    // NOTE(Rid):
    // The "Core" owns the RenderContext
    // RenderContext owns the API Instance, LogicalDevice, SwapChain, PhysicalDevice, MemoryAllocator etc.
    class SURGE_API RenderContext
    {
    public:
        RenderContext() = default;
        virtual ~RenderContext() = default;

        virtual void Initialize(Window* window, bool enableImGui = true) = 0;
        virtual void Shutdown() = 0;

        virtual void BeginFrame() = 0;
        virtual void EndFrame() = 0;

        virtual void OnResize() = 0;
        virtual Uint GetFrameIndex() const = 0;
        virtual void SetBlitSourceImage(const Ref<Image2D>& image) = 0;

        // Maybe move ImGui stuff somwhere else?
        virtual void RenderImGui() = 0;
        virtual void* GetImGuiTextureID(const Ref<Image2D>& image) const = 0;
        virtual void* GetImGuiContext() = 0;
        virtual GPUMemoryStats GetMemoryStatus() const = 0;
        virtual GPUInfo GetGPUInfo() const = 0;
    };

} // namespace Surge
