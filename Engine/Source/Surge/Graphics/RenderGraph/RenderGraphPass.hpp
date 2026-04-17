// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RenderGraph/RenderGraphResource.hpp"
#include "Surge/Graphics/RHI/RHIResources.hpp"
#include <functional>

// ---------------------------------------------------------------------------
// RenderGraphPass.hpp
//   Describes a single render graph pass (a node in the DAG).
//
//   Passes declare READS and WRITES to logical RGTexture/RGBuffer resources.
//   The RenderGraph uses these declarations to:
//     1. Build the dependency DAG.
//     2. Cull unreferenced passes.
//     3. Schedule barrier insertion.
//     4. Allocate / alias transient resources.
//
//   The execute callback receives a PassResources helper that resolves
//   logical handles to physical RHI handles at execution time.
// ---------------------------------------------------------------------------

namespace Surge
{
    // ── PassResources ─────────────────────────────────────────────────────────
    // Passed to every pass execute callback so it can look up the physical
    // RHI handles that correspond to the logical resource handles declared
    // during the setup phase.

    class RenderGraph; // forward

    class SURGE_API PassResources
    {
    public:
        explicit PassResources(const RenderGraph& graph) : mGraph(graph) {}

        // Resolve a logical texture handle to the physical RHI handle.
        RHI::TextureHandle GetTexture(RGTextureHandle handle) const;
        // Resolve a logical buffer handle to the physical RHI handle.
        RHI::BufferHandle  GetBuffer(RGBufferHandle handle) const;

    private:
        const RenderGraph& mGraph;
    };

    // ── Pass execute signature ────────────────────────────────────────────────

    using PassExecuteFunc = std::function<void(RHI::CommandBufferHandle, const PassResources&)>;

    // ── Pass node (internal representation) ──────────────────────────────────

    struct RenderGraphPassDesc
    {
        const char* Name = nullptr;

        // Resources declared by the pass
        Vector<RGTextureHandle> TextureReads;
        Vector<RGBufferHandle>  BufferReads;
        Vector<RGTextureHandle> ColorWrites;
        RGTextureHandle         DepthWrite  = RGTextureHandle::Null();
        Vector<RGBufferHandle>  BufferWrites;

        PassExecuteFunc Execute;
    };

    struct RenderGraphPassNode
    {
        RenderGraphPassDesc Desc;
        Vector<uint32_t>    DependsOn; // indices into RenderGraph::mPasses
        bool                Culled = false;
    };

} // namespace Surge
