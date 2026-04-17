// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RenderGraph/RenderGraphBuilder.hpp"
#include "Surge/Graphics/RenderGraph/RenderGraphPass.hpp"
#include "Surge/Graphics/RenderGraph/RenderGraphResource.hpp"
#include "Surge/Graphics/RHI/RHICommandBuffer.hpp"
#include <functional>

// ---------------------------------------------------------------------------
// RenderGraph.hpp
//   Frame-level pass + resource management.
//
//   Usage each frame:
//     1. graph.Reset()                       – clear all passes and transient resources
//     2. graph.AddPass(name, setupFn, execFn) – register passes (any order)
//     3. graph.Compile(width, height)         – sort, cull, allocate resources
//     4. graph.Execute(cmdBuf)                – record commands in topological order
//
//   Resource rules:
//     - Passes that produce resources not read by any other pass are culled.
//     - Transient resources are allocated by the graph; persistent resources
//       are imported via ImportTexture / ImportBuffer and are never freed by it.
//     - Physical resource allocation requires a valid GetRHIDevice(); if the
//       device is null the graph gracefully skips allocation (dry-run mode).
// ---------------------------------------------------------------------------

namespace Surge
{
    class SURGE_API RenderGraph
    {
    public:
        RenderGraph()  = default;
        ~RenderGraph() { Reset(); }

        SURGE_DISABLE_COPY_AND_MOVE(RenderGraph);

        // ── Frame lifecycle ───────────────────────────────────────────────────
        // Discards all passes and releases transient resources from the previous frame.
        void Reset();

        // Register a pass.  setupFn is called immediately to declare resource usage.
        void AddPass(const char* name,
                     std::function<void(RenderGraphBuilder&)> setupFn,
                     PassExecuteFunc executeFn);

        // Topological sort, culling, and transient resource allocation.
        // swapchainWidth/Height are used to resolve RGTextureDesc with width/height == 0.
        void Compile(uint32_t swapchainWidth, uint32_t swapchainHeight);

        // Record commands by calling pass execute functions in topological order.
        void Execute(RHI::CommandBufferHandle cmdBuffer);

        // ── Resource import ────────────────────────────────────────────────────
        // Import an externally-managed texture/buffer; the graph will not destroy it.
        RGTextureHandle ImportTexture(const char* name, RHI::TextureHandle physical,
                                      const RGTextureDesc& desc);
        RGBufferHandle  ImportBuffer(const char* name, RHI::BufferHandle physical,
                                     const RGBufferDesc& desc);

        // ── Resource accessors (post-Compile) ─────────────────────────────────
        RHI::TextureHandle GetTexture(RGTextureHandle handle) const;
        RHI::BufferHandle  GetBuffer(RGBufferHandle handle) const;

        // ── Internal (used by RenderGraphBuilder / PassResources) ─────────────
        RGTextureHandle RegisterTexture(const RGTextureDesc& desc);
        RGBufferHandle  RegisterBuffer(const RGBufferDesc& desc);

    private:
        void TopologicalSort();
        void CullPasses();
        void AllocateTransientResources(uint32_t width, uint32_t height);
        void FreeTransientResources();

    private:
        Vector<RenderGraphPassNode> mPasses;
        Vector<RGTextureNode>       mTextures;
        Vector<RGBufferNode>        mBuffers;
        Vector<uint32_t>            mSortedPassIndices;
        bool                        mCompiled = false;

        // Cached at Compile() time for use by Execute()
        RHI::CommandBufferHandle    mActiveCmdBuf = {};
    };

} // namespace Surge
