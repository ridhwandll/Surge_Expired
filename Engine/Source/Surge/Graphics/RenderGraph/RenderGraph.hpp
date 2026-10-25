// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Scope.hpp"
#include "Surge/Graphics/RHI/RHIFrameContext.hpp"
#include "Surge/Graphics/RenderGraph/RenderPass.hpp"
#include "Surge/Graphics/RenderGraph/FrameBlackboard.hpp"
#include <functional>

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
        void Setup(const RenderGraphContext& context); // Hands the context to every pass, then calls their Setup

        // Derives execution order, barriers, LoadOp/StoreOp
        void Compile();

        // Called every frame
        void Execute(FrameContext& ctx);

        void ClearLists() { mBlackboard.ClearLists(); }

        // Recreates size-dependent resources, calls Resize() on all passses
        void OnWindowResize(Uint width, Uint height);
        void ForceResize(Uint width, Uint height);

        // Disabled: only the swapchain group runs (Editor: the scene is drawn by the Renderer's views, main graph just presents ImGui)
        void SetSceneEnabled(bool enabled) { mSceneEnabled = enabled; }
        bool IsSceneEnabled() const { return mSceneEnabled; }

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

        // Run inside the swapchain pass (ImGui frame), the debug UI (Editor's Render Graph/Renderer panels) lives in these
        void AddImGuiRenderCallback(std::function<void()> callback);

        FrameBlackboard& GetBlackboard() { return mBlackboard; }
        const FrameBlackboard& GetBlackboard() const { return mBlackboard; }
        const CompiledGraph& GetCompiledGraph() const { return mCompiledGraph; } // For visualizer
        const Vector<Scope<RenderPass>>& GetPasses() const { return mPasses; }   // Registration order
        GraphicsRHI* GetRHI() const { return mRHI; }
        const RenderGraphContext& GetContext() const { return mContext; }
    private:
        void SortByDependencies(Vector<RenderPass*>& passes); // Topological sort within a group based on ImageReads/ImageWrites
        void DeriveBarriers(Vector<ExecutionGroup*>& orderedGroups); // Inserts a SAMPLED transition before the first group that reads an image written by an earlier group
        void RenderImGuiCallbacks();
    private:
        bool mImGuiEnabled = false;
        bool mSceneEnabled = true;
        Vector<Scope<RenderPass>> mPasses;

        Vector<std::function<void()>> mImGuiRenderCallbacks;
        FrameBlackboard mBlackboard;
        CompiledGraph mCompiledGraph;
        RenderGraphContext mContext;
        GraphicsRHI* mRHI = nullptr;
    };
}
