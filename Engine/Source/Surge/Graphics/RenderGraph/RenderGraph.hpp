// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RHI/RHIFrameContext.hpp"
#include "RenderPass.hpp"
#include "FrameBlackboard.hpp"
#include "ResourceRegistry.hpp"

namespace Surge
{
    class GraphicsRHI;

    class RenderGraph 
    {
    public:
        void Setup(GraphicsRHI* rhi);

        // Derives execution order, barriers, LoadOp/StoreOp
        void Compile();

        // Called every frame
        void Execute(FrameContext& ctx);

        void OnImGuiRender();
        void ClearLists() { mBlackboard.ClearLists(); }

        // Recreates size-dependent resources, calls Resize() on all passses
        void Resize(Uint width, Uint height);

        // Destroys all passses and registry resources
        void Shutdown();

        template<typename T, typename... Args>
        T* AddPass(Args&&... args)
        {
            static_assert(std::is_base_of_v<RenderPass, T>, "T must derive from RenderPass");
            auto node = CreateScope<T>(std::forward<Args>(args)...);
            T* raw = node.get();
            mPasses.push_back(std::move(node));
            return raw;
        }

        FrameBlackboard& GetBlackboard() { return mBlackboard; }
        const FrameBlackboard& GetBlackboard() const { return mBlackboard; }
        //const CompiledGraph& GetCompiledGraph() const { return mCompiledGraph; } // For visualizer
    private:
        // Topological sort within a group based on ImageReads/ImageWrites
        void SortByDependencies(Vector<RenderPass*>& nodes);

        // Derives VkImageMemoryBarriers needed between two groups
        // void DeriveBarriers(ExecutionGroup& producer, ExecutionGroup& consumer);

    private:
        Vector<Scope<RenderPass>> mPasses;
        FrameBlackboard mBlackboard;
        ResourceRegistry mRegistry;
        GraphicsRHI* mRHI = nullptr;
    };
}
