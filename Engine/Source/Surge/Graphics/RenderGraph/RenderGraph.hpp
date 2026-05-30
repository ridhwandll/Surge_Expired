// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RHI/RHIFrameContext.hpp"
#include "Surge/Graphics/RenderGraph/RenderPass.hpp"
#include "Surge/Graphics/RenderGraph/FrameBlackboard.hpp"

namespace Surge
{
    class GraphicsRHI;

    struct ImageBarrier
    {
        ImageHandle Handle;
        ImageUsage NewUsage;
    };

    struct ExecutionGroup
    {
        String Name;
        PassGroup Type;
        Vector<RenderPass*> Passes;                                   // Sorted
        Vector<ImageBarrier> BarriersBeforeGroup;                     // Inserted before vkBeginRenderPass
        FramebufferHandle Framebuffer = FramebufferHandle::Invalid(); // Null if ManagesOwnExecution

        // (Rid) ManagesOwnExecution means this execution group will call vkBeginRenderPass on its Execute method
        // We primarily use it for CSM, is this a good design choice?
        bool ManagesOwnExecution = false;

        bool IsSwapchain = false;
    };

    struct CompiledGraph
    {
        bool IsValid = false;
        Vector<ExecutionGroup> Groups;
    };

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
        void OnWindowResize(Uint width, Uint height);
        void ForceResize(Uint width, Uint height);

        void ShowImGui(bool show) { mShowImGui = show; }

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

        void AddImGuiRenderCallback(std::function<void()> callback) { if(callback) { mImGuiRenderCallbacks.push_back(std::move(callback)); } }

        FrameBlackboard& GetBlackboard() { return mBlackboard; }
        const FrameBlackboard& GetBlackboard() const { return mBlackboard; }
        const CompiledGraph& GetCompiledGraph() const { return mCompiledGraph; } // For visualizer
    private:
        void SortByDependencies(Vector<RenderPass*>& passes); // Topological sort within a group based on ImageReads/ImageWrites
        void DeriveBarrierBetweenExecutionGroups(ExecutionGroup& writeGroup, ExecutionGroup& readGroup);
    private:
        bool mShowImGui = true;
        Vector<Scope<RenderPass>> mPasses;

        Vector<std::function<void()>> mImGuiRenderCallbacks;
        FrameBlackboard mBlackboard;
        CompiledGraph mCompiledGraph;
        GraphicsRHI* mRHI = nullptr;
    };
}
