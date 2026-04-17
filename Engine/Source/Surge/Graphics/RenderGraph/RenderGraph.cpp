// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RenderGraph/RenderGraph.hpp"
#include "Surge/Graphics/RHI/RHIDevice.hpp"

namespace Surge
{
    // ────────────────────────────────────────────────────────────────────────────
    // PassResources
    // ────────────────────────────────────────────────────────────────────────────

    RHI::TextureHandle PassResources::GetTexture(RGTextureHandle handle) const
    {
        return mGraph.GetTexture(handle);
    }

    RHI::BufferHandle PassResources::GetBuffer(RGBufferHandle handle) const
    {
        return mGraph.GetBuffer(handle);
    }

    // ────────────────────────────────────────────────────────────────────────────
    // RenderGraphBuilder
    // ────────────────────────────────────────────────────────────────────────────

    RenderGraphBuilder::RenderGraphBuilder(RenderGraph& graph, RenderGraphPassDesc& desc)
        : mGraph(graph), mPassDesc(desc)
    {
    }

    RGTextureHandle RenderGraphBuilder::Create(const RGTextureDesc& desc)
    {
        RGTextureHandle h = mGraph.RegisterTexture(desc);
        return h;
    }

    RGBufferHandle RenderGraphBuilder::Create(const RGBufferDesc& desc)
    {
        return mGraph.RegisterBuffer(desc);
    }

    RGTextureHandle RenderGraphBuilder::Read(RGTextureHandle handle)
    {
        if (handle.IsValid())
        {
            // Increment reference count to prevent culling
            if (handle.id < static_cast<uint32_t>(mGraph.mTextures.size()))
                mGraph.mTextures[handle.id].RefCount++;
            mPassDesc.TextureReads.push_back(handle);
        }
        return handle;
    }

    RGBufferHandle RenderGraphBuilder::Read(RGBufferHandle handle)
    {
        if (handle.IsValid())
        {
            if (handle.id < static_cast<uint32_t>(mGraph.mBuffers.size()))
                mGraph.mBuffers[handle.id].RefCount++;
            mPassDesc.BufferReads.push_back(handle);
        }
        return handle;
    }

    RGTextureHandle RenderGraphBuilder::WriteColor(RGTextureHandle handle)
    {
        if (handle.IsValid())
        {
            uint32_t passIdx = static_cast<uint32_t>(mGraph.mPasses.size());
            if (handle.id < static_cast<uint32_t>(mGraph.mTextures.size()))
                mGraph.mTextures[handle.id].WrittenByPass = passIdx;
            mPassDesc.ColorWrites.push_back(handle);
        }
        return handle;
    }

    RGTextureHandle RenderGraphBuilder::WriteDepth(RGTextureHandle handle)
    {
        if (handle.IsValid())
        {
            uint32_t passIdx = static_cast<uint32_t>(mGraph.mPasses.size());
            if (handle.id < static_cast<uint32_t>(mGraph.mTextures.size()))
                mGraph.mTextures[handle.id].WrittenByPass = passIdx;
            mPassDesc.DepthWrite = handle;
        }
        return handle;
    }

    RGBufferHandle RenderGraphBuilder::WriteBuffer(RGBufferHandle handle)
    {
        if (handle.IsValid())
        {
            uint32_t passIdx = static_cast<uint32_t>(mGraph.mPasses.size());
            if (handle.id < static_cast<uint32_t>(mGraph.mBuffers.size()))
                mGraph.mBuffers[handle.id].WrittenByPass = passIdx;
            mPassDesc.BufferWrites.push_back(handle);
        }
        return handle;
    }

    // ────────────────────────────────────────────────────────────────────────────
    // RenderGraph
    // ────────────────────────────────────────────────────────────────────────────

    void RenderGraph::Reset()
    {
        FreeTransientResources();
        mPasses.clear();
        mTextures.clear();
        mBuffers.clear();
        mSortedPassIndices.clear();
        mCompiled = false;
    }

    RGTextureHandle RenderGraph::RegisterTexture(const RGTextureDesc& desc)
    {
        RGTextureNode node;
        node.Desc     = desc;
        node.Imported = false;
        mTextures.push_back(std::move(node));
        return RGTextureHandle{static_cast<uint32_t>(mTextures.size() - 1)};
    }

    RGBufferHandle RenderGraph::RegisterBuffer(const RGBufferDesc& desc)
    {
        RGBufferNode node;
        node.Desc     = desc;
        node.Imported = false;
        mBuffers.push_back(std::move(node));
        return RGBufferHandle{static_cast<uint32_t>(mBuffers.size() - 1)};
    }

    void RenderGraph::AddPass(const char* name,
                               std::function<void(RenderGraphBuilder&)> setupFn,
                               PassExecuteFunc executeFn)
    {
        RenderGraphPassNode passNode;
        passNode.Desc.Name    = name;
        passNode.Desc.Execute = std::move(executeFn);

        // Call setup to let the pass declare resource usage.
        // The builder writes into passNode.Desc; resource nodes are already in mTextures/mBuffers
        // because Create() calls RegisterTexture/RegisterBuffer immediately.
        RenderGraphBuilder builder(*this, passNode.Desc);
        setupFn(builder);

        mPasses.push_back(std::move(passNode));
    }

    RGTextureHandle RenderGraph::ImportTexture(const char* name, RHI::TextureHandle physical,
                                                const RGTextureDesc& desc)
    {
        RGTextureNode node;
        node.Desc           = desc;
        node.Desc.Name      = name;
        node.Desc.Lifetime  = ResourceLifetime::Persistent;
        node.PhysicalHandle = physical;
        node.Imported       = true;
        mTextures.push_back(std::move(node));
        return RGTextureHandle{static_cast<uint32_t>(mTextures.size() - 1)};
    }

    RGBufferHandle RenderGraph::ImportBuffer(const char* name, RHI::BufferHandle physical,
                                              const RGBufferDesc& desc)
    {
        RGBufferNode node;
        node.Desc           = desc;
        node.Desc.Name      = name;
        node.Desc.Lifetime  = ResourceLifetime::Persistent;
        node.PhysicalHandle = physical;
        node.Imported       = true;
        mBuffers.push_back(std::move(node));
        return RGBufferHandle{static_cast<uint32_t>(mBuffers.size() - 1)};
    }

    void RenderGraph::TopologicalSort()
    {
        const uint32_t count = static_cast<uint32_t>(mPasses.size());
        mSortedPassIndices.clear();
        mSortedPassIndices.reserve(count);

        // Build dependency edges: pass B depends on pass A if B reads a texture written by A.
        for (uint32_t i = 0; i < count; ++i)
        {
            auto& passNode = mPasses[i];
            passNode.DependsOn.clear();

            for (const RGTextureHandle& rh : passNode.Desc.TextureReads)
            {
                if (!rh.IsValid() || rh.id >= static_cast<uint32_t>(mTextures.size()))
                    continue;
                const uint32_t producerPass = mTextures[rh.id].WrittenByPass;
                if (producerPass != UINT32_MAX && producerPass != i)
                    passNode.DependsOn.push_back(producerPass);
            }
            for (const RGBufferHandle& rh : passNode.Desc.BufferReads)
            {
                if (!rh.IsValid() || rh.id >= static_cast<uint32_t>(mBuffers.size()))
                    continue;
                const uint32_t producerPass = mBuffers[rh.id].WrittenByPass;
                if (producerPass != UINT32_MAX && producerPass != i)
                    passNode.DependsOn.push_back(producerPass);
            }
        }

        // Kahn's algorithm – topological sort on the pass DAG.
        // Edge A -> B means: pass A must execute before pass B (B depends on A).
        // 'successors[A]' holds all passes that depend on A.
        // 'inDeg[B]'      counts how many predecessors B has.
        Vector<Vector<uint32_t>> successors(count);
        Vector<uint32_t> inDeg(count, 0);
        for (uint32_t i = 0; i < count; ++i)
        {
            for (uint32_t dep : mPasses[i].DependsOn)
            {
                successors[dep].push_back(i);
                inDeg[i]++;
            }
        }

        Deque<uint32_t> queue;
        for (uint32_t i = 0; i < count; ++i)
            if (inDeg[i] == 0) queue.push_back(i);

        while (!queue.empty())
        {
            const uint32_t cur = queue.front();
            queue.pop_front();
            mSortedPassIndices.push_back(cur);
            for (uint32_t succ : successors[cur])
            {
                if (--inDeg[succ] == 0)
                    queue.push_back(succ);
            }
        }

        // If sizes differ there is a cycle (should not happen with valid render graphs)
        SG_ASSERT(mSortedPassIndices.size() == count,
                  "RenderGraph: detected cycle in pass dependency graph!");
    }

    void RenderGraph::CullPasses()
    {
        // Any pass whose colour/depth/buffer outputs have zero readers is culled.
        // Walk in reverse topological order and propagate ref-counts backwards.
        for (int32_t i = static_cast<int32_t>(mSortedPassIndices.size()) - 1; i >= 0; --i)
        {
            const uint32_t passIdx = mSortedPassIndices[static_cast<uint32_t>(i)];
            auto& pass = mPasses[passIdx];

            bool hasRefedOutput = false;
            for (const RGTextureHandle& wh : pass.Desc.ColorWrites)
                if (wh.IsValid() && wh.id < static_cast<uint32_t>(mTextures.size()))
                    if (mTextures[wh.id].RefCount > 0) hasRefedOutput = true;
            if (pass.Desc.DepthWrite.IsValid() &&
                pass.Desc.DepthWrite.id < static_cast<uint32_t>(mTextures.size()))
                if (mTextures[pass.Desc.DepthWrite.id].RefCount > 0) hasRefedOutput = true;
            for (const RGBufferHandle& wh : pass.Desc.BufferWrites)
                if (wh.IsValid() && wh.id < static_cast<uint32_t>(mBuffers.size()))
                    if (mBuffers[wh.id].RefCount > 0) hasRefedOutput = true;

            // A pass is culled only if it produces outputs AND none are referenced.
            // A pass with no declared outputs (e.g. a pure side-effect pass) is never culled.
            const bool hasAnyOutputs = !pass.Desc.ColorWrites.empty()
                                    || pass.Desc.DepthWrite.IsValid()
                                    || !pass.Desc.BufferWrites.empty();
            if (!hasRefedOutput && hasAnyOutputs)
            {
                pass.Culled = true;
                continue;
            }

            // Propagate: this pass is live, so its inputs are referenced
            for (const RGTextureHandle& rh : pass.Desc.TextureReads)
                if (rh.IsValid() && rh.id < static_cast<uint32_t>(mTextures.size()))
                    mTextures[rh.id].RefCount++;
            for (const RGBufferHandle& rh : pass.Desc.BufferReads)
                if (rh.IsValid() && rh.id < static_cast<uint32_t>(mBuffers.size()))
                    mBuffers[rh.id].RefCount++;
        }
    }

    void RenderGraph::AllocateTransientResources(uint32_t width, uint32_t height)
    {
        RHI::RHIDevice* dev = nullptr;
        // GetRHIDevice() asserts on null; guard with a direct pointer check
        // by reading through the static function pointer we stored in RHIDevice.cpp.
        // For safety: attempt to get the device; if unavailable, skip allocation.
        // (The device will be null before the new RHI backend is wired up.)
        {
            // We test if sDevice is non-null by trying a no-op call approach.
            // A cleaner alternative would be a HasRHIDevice() query; for now
            // we rely on the compile step being a no-op if no device is present.
            dev = nullptr;
            // TODO: add RHI::HasRHIDevice() helper to avoid assertion
        }
        (void)dev;

        for (auto& texNode : mTextures)
        {
            if (texNode.Imported || texNode.PhysicalHandle.IsValid())
                continue;
            if (texNode.Desc.Lifetime != ResourceLifetime::Transient)
                continue;

            RHI::TextureDesc tdesc;
            tdesc.Width  = texNode.Desc.Width  == 0 ? width  : texNode.Desc.Width;
            tdesc.Height = texNode.Desc.Height == 0 ? height : texNode.Desc.Height;
            tdesc.Format = texNode.Desc.Format;
            tdesc.Usage  = texNode.Desc.Usage;
            tdesc.Mips   = 1;

            // Physical allocation deferred until the RHI device is registered.
            // texNode.PhysicalHandle = RHI::GetRHIDevice()->createTexture(tdesc);
            (void)tdesc;
        }

        for (auto& bufNode : mBuffers)
        {
            if (bufNode.Imported || bufNode.PhysicalHandle.IsValid())
                continue;
            if (bufNode.Desc.Lifetime != ResourceLifetime::Transient)
                continue;

            RHI::BufferDesc bdesc;
            bdesc.Size        = bufNode.Desc.Size;
            bdesc.Usage       = bufNode.Desc.Usage;
            bdesc.MemoryUsage = bufNode.Desc.MemUsage;

            // bufNode.PhysicalHandle = RHI::GetRHIDevice()->createBuffer(bdesc);
            (void)bdesc;
        }
    }

    void RenderGraph::FreeTransientResources()
    {
        // Only free transient resources that we actually allocated.
        // Since AllocateTransientResources is currently a no-op (device not wired),
        // there is nothing to free yet.  This will be completed when the RHI device
        // is registered.
        for (auto& texNode : mTextures)
        {
            if (!texNode.Imported && texNode.PhysicalHandle.IsValid()
                && texNode.Desc.Lifetime == ResourceLifetime::Transient)
            {
                // RHI::GetRHIDevice()->destroyTexture(texNode.PhysicalHandle);
                texNode.PhysicalHandle = RHI::TextureHandle{};
            }
        }
        for (auto& bufNode : mBuffers)
        {
            if (!bufNode.Imported && bufNode.PhysicalHandle.IsValid()
                && bufNode.Desc.Lifetime == ResourceLifetime::Transient)
            {
                // RHI::GetRHIDevice()->destroyBuffer(bufNode.PhysicalHandle);
                bufNode.PhysicalHandle = RHI::BufferHandle{};
            }
        }
    }

    void RenderGraph::Compile(uint32_t swapchainWidth, uint32_t swapchainHeight)
    {
        TopologicalSort();
        CullPasses();
        AllocateTransientResources(swapchainWidth, swapchainHeight);
        mCompiled = true;
    }

    void RenderGraph::Execute(RHI::CommandBufferHandle cmdBuffer)
    {
        SG_ASSERT(mCompiled, "RenderGraph::Execute called before Compile!");

        PassResources resources(*this);

        for (const uint32_t passIdx : mSortedPassIndices)
        {
            const auto& pass = mPasses[passIdx];
            if (pass.Culled)
                continue;
            if (!pass.Desc.Execute)
                continue;

            // TODO: Insert pipeline barriers for texture layout transitions here
            //       once the RHI barrier API is finalised.

            pass.Desc.Execute(cmdBuffer, resources);
        }
    }

    RHI::TextureHandle RenderGraph::GetTexture(RGTextureHandle handle) const
    {
        SG_ASSERT(handle.IsValid() && handle.id < static_cast<uint32_t>(mTextures.size()),
                  "RenderGraph::GetTexture – invalid handle");
        return mTextures[handle.id].PhysicalHandle;
    }

    RHI::BufferHandle RenderGraph::GetBuffer(RGBufferHandle handle) const
    {
        SG_ASSERT(handle.IsValid() && handle.id < static_cast<uint32_t>(mBuffers.size()),
                  "RenderGraph::GetBuffer – invalid handle");
        return mBuffers[handle.id].PhysicalHandle;
    }

} // namespace Surge
