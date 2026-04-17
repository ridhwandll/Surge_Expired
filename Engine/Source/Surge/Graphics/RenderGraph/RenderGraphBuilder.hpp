// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RenderGraph/RenderGraphPass.hpp"
#include "Surge/Graphics/RenderGraph/RenderGraphResource.hpp"

// ---------------------------------------------------------------------------
// RenderGraphBuilder.hpp
//   Fluent builder API used inside the pass setup callback.
//   Passed to the lambda supplied to RenderGraph::AddPass(); the builder
//   records resource declarations into a RenderGraphPassDesc and returns
//   typed logical handles back to the caller.
//
//   Example usage:
//     graph.AddPass("GeometryPass",
//         [&](RenderGraphBuilder& builder) {
//             colorTex = builder.Create(colorDesc);
//             depthTex = builder.Create(depthDesc);
//             shadowTex = builder.Read(importedShadowTex);
//             builder.WriteColor(colorTex);
//             builder.WriteDepth(depthTex);
//         },
//         [&](RHI::CommandBufferHandle cmd, const PassResources& res) { ... });
// ---------------------------------------------------------------------------

namespace Surge
{
    class RenderGraph; // forward

    class SURGE_API RenderGraphBuilder
    {
    public:
        // ── Resource creation ──────────────────────────────────────────────
        // Creates a NEW transient texture owned by this pass.
        RGTextureHandle Create(const RGTextureDesc& desc);
        // Creates a NEW transient buffer owned by this pass.
        RGBufferHandle  Create(const RGBufferDesc& desc);

        // ── Declare read access ────────────────────────────────────────────
        // Declares that this pass reads the given texture (adds an edge to the DAG).
        RGTextureHandle Read(RGTextureHandle handle);
        // Declares that this pass reads the given buffer.
        RGBufferHandle  Read(RGBufferHandle handle);

        // ── Declare write access ──────────────────────────────────────────
        // Declares that this pass writes to the given texture as a colour attachment.
        RGTextureHandle WriteColor(RGTextureHandle handle);
        // Declares that this pass writes to the given texture as a depth attachment.
        RGTextureHandle WriteDepth(RGTextureHandle handle);
        // Declares that this pass writes to the given buffer (compute / UAV).
        RGBufferHandle  WriteBuffer(RGBufferHandle handle);

    private:
        friend class RenderGraph;
        RenderGraphBuilder(RenderGraph& graph, RenderGraphPassDesc& desc);

        RenderGraph&        mGraph;
        RenderGraphPassDesc& mPassDesc;
    };

} // namespace Surge
