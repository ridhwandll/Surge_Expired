// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RHI/RHIResources.hpp"

// ---------------------------------------------------------------------------
// UIRenderer.hpp
//   Thin wrapper that bridges the new RHI layer with ImGui rendering.
//   The underlying ImGui context is still owned by the legacy
//   VulkanRenderContext; this class exposes a clean interface for future
//   migration when the legacy context is removed.
// ---------------------------------------------------------------------------

namespace Surge
{
    class SURGE_API UIRenderer
    {
    public:
        UIRenderer()  = default;
        ~UIRenderer() = default;

        SURGE_DISABLE_COPY_AND_MOVE(UIRenderer);

        void Initialize();
        void Shutdown();

        // Call at the start of the UI recording phase (before ImGui::NewFrame)
        void BeginFrame();
        // Call after all ImGui::* calls, before present
        void EndFrame();

        // Map a RHI texture to an ImGui texture ID.
        // Returns nullptr if ImGui is not available.
        void* GetTextureID(RHI::TextureHandle handle);
    };

} // namespace Surge
