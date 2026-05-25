// Copyright (c) - SurgeTechnologies - All rights reserved
#include "RenderGraph.hpp"
#include "Surge/Graphics/RHI/RHI.hpp"
#include <queue>
#include <set>

namespace Surge
{
    void RenderGraph::Setup(GraphicsRHI* rhi)
    {
        mRHI = rhi;

        // Nodes must be set up in registration order
        // Producers must be registered before consumers so blackboard handles are valid
        for(auto& pass : mPasses)
            pass->Setup(rhi, mBlackboard);
    }

    void RenderGraph::Compile()
    {
        SCOPED_TIMER("RenderGraph::Compile");
        mCompiledGraph = {};

        ExecutionGroup mainGroup = { .Name = "Main Scene", .Type = PassGroup::MAIN_SCENE};
        ExecutionGroup swapchainGroup = { .Name = "Swapchain", .Type = PassGroup::SWAPCHAIN, .Passes = {}, .BarriersBeforeGroup = {}, .Framebuffer {}, .IsSwapchain = true };

        for(auto& pass : mPasses)
        {
            switch(pass->GetGroup())
            {
                case PassGroup::MAIN_SCENE: mainGroup.Passes.push_back(pass.get());       break;
                case PassGroup::SWAPCHAIN:  swapchainGroup.Passes.push_back(pass.get());  break;
            }
        }

        SortByDependencies(mainGroup.Passes);
        SortByDependencies(swapchainGroup.Passes);

        mainGroup.Framebuffer = mBlackboard.OffscreenFramebuffer;

        // (Rid) Derive barriers between groups
        // These hell for loop basically answers:
        // Does any pass in a group write an image that any pass in b group reads? If so, that image needs a barrier between them
        // MainScene -> Swapchain: FinalImage color -> shader read
        std::set<ImageHandle> barrierAdded;
        for(RenderPass* a : mainGroup.Passes)
        {
            for(RenderPass* b : swapchainGroup.Passes)
            {
                for(ImageHandle write : a->GetImageWrites())
                {
                    for(ImageHandle read : b->GetImageReads())
                    {
                        if(write == read && !barrierAdded.count(write))
                        {
                            swapchainGroup.BarriersBeforeGroup.push_back({ write, ImageUsage::COLOR_ATTACHMENT, ImageUsage::SAMPLED });
                            barrierAdded.insert(write);
                        }
                    }
                }
            }
        }

        // Build final groups
        mCompiledGraph.Groups.push_back(std::move(mainGroup));

        if(!swapchainGroup.Passes.empty())
            mCompiledGraph.Groups.push_back(std::move(swapchainGroup));

        mCompiledGraph.IsValid = true;
    }

    void RenderGraph::Execute(FrameContext& ctx)
    {
        for(Uint i = 0; i < mCompiledGraph.Groups.size(); i++)
        {
            const ExecutionGroup& group = mCompiledGraph.Groups[i];

            // Swapchain Blit (TODO: Is this okay?)
            if(i > 0 && mCompiledGraph.Groups[i - 1].Type == PassGroup::MAIN_SCENE && group.IsSwapchain)
            {
                if (RHISettings::BLIT_TO_SWAPCHAIN)
                    mRHI->CmdBlitToSwapchain(ctx, mBlackboard.FinalImage);
                else
                {
                    for(const ImageBarrier& barrier : group.BarriersBeforeGroup)
                        mRHI->CmdTransitionImageLayout(ctx, barrier.Handle, barrier.NewUsage);
                }
            }

            group.IsSwapchain ? mRHI->CmdBeginSwapchainRenderpass(ctx) : mRHI->CmdBeginRenderPass(ctx, group.Framebuffer, mBlackboard.ClearColor);

            for(RenderPass* pass : group.Passes)
            {
                if (pass->IsEnabled())
                    pass->Execute(ctx, mBlackboard);
            }

            group.IsSwapchain ? (OnImGuiRender(), mRHI->CmdEndSwapchainRenderpass(ctx)) : mRHI->CmdEndRenderPass(ctx, group.Framebuffer);
        }
    }

    void RenderGraph::OnImGuiRender()
    {
        for(auto& callback : mImGuiRenderCallbacks)
            callback();

        ImGui::Begin("Render Graph");

        int i = 67;
        for(const ExecutionGroup& group : mCompiledGraph.Groups)
        {
            ImGui::PushID(i);
            if (ImGui::TreeNode(group.Name.c_str()))
            {
                int j = 0;
                for(RenderPass* pass : group.Passes)
                {
                    ImGui::PushID(j);
                    if (ImGui::TreeNode(pass->GetName().c_str()))
                    {
                        auto& reads = pass->GetImageReads();
                        auto& writes = pass->GetImageWrites();

                        bool enabled = pass->IsEnabled();
                        if(ImGui::Checkbox("Enabled", &enabled))
                            pass->SetEnabled(enabled);

                        for(auto& imgRead : reads)
                        {
                            const ImageDesc& desc = mRHI->GetDesc(imgRead);
                            ImGui::Text("Image Read: %s", desc.DebugName.c_str());
                        }

                        for(auto& imgWrites : writes)
                        {
                            const ImageDesc& desc = mRHI->GetDesc(imgWrites);
                            ImGui::Text("Image Write: %s", desc.DebugName.c_str());
                        }

                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                    j++;
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
            i++;
        }

        mRHI->ShowMetricsWindow();

        for(Scope<RenderPass>& pass : mPasses)
            pass->OnImGuiRender(mBlackboard);
        ImGui::End();
    }

    void RenderGraph::Resize(Uint width, Uint height)
    {
        // Let each node resize its own private resources (framebuffers, pipelines if needed)
        for(auto& node : mPasses)
            node->Resize(width, height, mBlackboard);
    }

    void RenderGraph::Shutdown()
    {
        for(auto& pass : mPasses)
            pass->Shutdown();
    }

    void RenderGraph::SortByDependencies(Vector<RenderPass*>& passes)
    {
        SCOPED_TIMER("RenderGraph::SortByDependencies");

        if(passes.size() <= 1)
            return;

        // Stable bubble-up sort based on direct read/write dependencies
        bool changed = true;
        size_t iterations = 0;
        const size_t maxIterations = passes.size() * passes.size(); // To detect cycles

        while(changed)
        {
            changed = false;
            iterations++;

            if(iterations > maxIterations)
            {
                SG_ASSERT_INTERNAL("Cyclic dependency between passes!");
                return;
            }

            for(size_t i = 0; i < passes.size() - 1; ++i)
            {
                RenderPass* a = passes[i];
                RenderPass* b = passes[i + 1];

                // (Rid)Does pass A depend on pass B?
                // (Meaning B writes to something A reads, so B must go BEFORE A)
                bool aDependsOnB = false;
                for(ImageHandle write : b->GetImageWrites())
                {
                    for(ImageHandle read : a->GetImageReads())
                    {
                        if(write == read)
                        {
                            aDependsOnB = true;
                            break;
                        }
                    }
                    if(aDependsOnB)
                        break;
                }

                if(aDependsOnB)
                {
                    // If A depends on B, swap them so B comes first
                    std::swap(passes[i], passes[i + 1]);
                    changed = true;
                }
            }
        }
    }
}
