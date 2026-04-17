// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Core/RendererConfig.hpp"

namespace Surge
{
    namespace
    {
        static RendererCaps sGlobalCaps;
    }

    namespace RendererConfig
    {
        RendererBackend SelectBackend()
        {
#if defined(SURGE_WINDOWS) || defined(SURGE_LINUX)
            return RendererBackend::Vulkan;
#elif defined(SURGE_APPLE)
            return RendererBackend::Metal;
#else
            return RendererBackend::Vulkan; // Fallback
#endif
        }

        const RendererCaps& GetCaps()
        {
            return sGlobalCaps;
        }

        void SetCaps(const RendererCaps& caps)
        {
            sGlobalCaps = caps;
        }

    } // namespace RendererConfig

} // namespace Surge
