// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"

namespace Surge
{
    enum class RendererBackend
    {
        None = 0,
        Vulkan,
        Metal,
        OpenGL
    };

    // Capabilities queried from the selected GPU/backend and stored once at init.
    struct RendererCaps
    {
        bool     MeshShaders        = false;
        bool     Bindless           = false;
        bool     Raytracing         = false;
        bool     Sync2              = false; // VK_KHR_synchronization2
        uint32_t MaxFramesInFlight  = 3;
        uint32_t MaxSamplerAnisotropy = 1;
        uint32_t MaxColorAttachments  = 8;
        uint32_t MaxPushConstantSize  = 128;
        String   DeviceName;
        int64_t  DeviceScore        = 0;
    };

    // Renderer configuration helpers.
    // SelectBackend() reads compile-time platform macros.
    // GetCaps()/SetCaps() provide a single global RendererCaps instance.
    namespace RendererConfig
    {
        SURGE_API RendererBackend  SelectBackend();
        SURGE_API const RendererCaps& GetCaps();
        SURGE_API void             SetCaps(const RendererCaps& caps);
    } // namespace RendererConfig

} // namespace Surge
