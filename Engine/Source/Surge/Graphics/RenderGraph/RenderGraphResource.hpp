// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RHI/RHIResources.hpp"
#include "Surge/Graphics/RHI/RHITypes.hpp"

// ---------------------------------------------------------------------------
// RenderGraphResource.hpp
//   Logical resource handles used inside the Render Graph.
//
//   RGTextureHandle / RGBufferHandle are NOT the same as the physical
//   RHI handles (TextureHandle / BufferHandle).  They represent SLOTS in the
//   render graph resource table; physical handles are filled in during
//   RenderGraph::Compile().
//
//   Resource lifetime:
//     Transient  – created and destroyed within one frame; the render graph
//                  may alias transient resources that do not overlap in time.
//     Persistent – imported from outside the graph (e.g. a shadow map that
//                  persists across frames).  The graph never destroys these.
// ---------------------------------------------------------------------------

namespace Surge
{
    enum class ResourceLifetime : uint8_t
    {
        Transient,   // per-frame; graph owns it
        Persistent   // imported; graph only reads/writes, never destroys
    };

    // ── Logical texture handle ───────────────────────────────────────────────

    struct RGTextureHandle
    {
        static constexpr uint32_t Invalid = UINT32_MAX;
        uint32_t id = Invalid;

        bool IsValid() const noexcept { return id != Invalid; }
        static RGTextureHandle Null() noexcept { return {}; }

        bool operator==(const RGTextureHandle& o) const noexcept { return id == o.id; }
        bool operator!=(const RGTextureHandle& o) const noexcept { return id != o.id; }
    };

    // ── Logical buffer handle ────────────────────────────────────────────────

    struct RGBufferHandle
    {
        static constexpr uint32_t Invalid = UINT32_MAX;
        uint32_t id = Invalid;

        bool IsValid() const noexcept { return id != Invalid; }
        static RGBufferHandle Null() noexcept { return {}; }

        bool operator==(const RGBufferHandle& o) const noexcept { return id == o.id; }
        bool operator!=(const RGBufferHandle& o) const noexcept { return id != o.id; }
    };

    // ── Logical texture descriptor ───────────────────────────────────────────
    // Width/Height == 0 means "same as swapchain" (resolved at compile time).

    struct RGTextureDesc
    {
        uint32_t               Width    = 0; // 0 = swapchain width
        uint32_t               Height   = 0; // 0 = swapchain height
        RHI::ImageFormat       Format   = RHI::ImageFormat::RGBA8;
        RHI::ImageUsage        Usage    = RHI::ImageUsage::Attachment | RHI::ImageUsage::Sampled;
        ResourceLifetime       Lifetime = ResourceLifetime::Transient;
        const char*            Name     = nullptr;
    };

    // ── Logical buffer descriptor ────────────────────────────────────────────

    struct RGBufferDesc
    {
        uint32_t             Size     = 0;
        RHI::BufferUsage     Usage    = RHI::BufferUsage::StorageBuffer;
        RHI::GPUMemoryUsage  MemUsage = RHI::GPUMemoryUsage::GPUOnly;
        ResourceLifetime     Lifetime = ResourceLifetime::Transient;
        const char*          Name     = nullptr;
    };

    // ── Internal resource nodes (stored inside RenderGraph) ─────────────────

    struct RGTextureNode
    {
        RGTextureDesc       Desc;
        RHI::TextureHandle  PhysicalHandle; // filled during Compile()
        bool                Imported = false;
        uint32_t            WrittenByPass = UINT32_MAX; // pass index that last wrote
        uint32_t            RefCount = 0; // passes that read this resource
    };

    struct RGBufferNode
    {
        RGBufferDesc       Desc;
        RHI::BufferHandle  PhysicalHandle; // filled during Compile()
        bool               Imported = false;
        uint32_t           WrittenByPass = UINT32_MAX;
        uint32_t           RefCount = 0;
    };

} // namespace Surge
