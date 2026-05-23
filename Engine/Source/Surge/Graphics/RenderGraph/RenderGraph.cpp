// Copyright (c) - SurgeTechnologies - All rights reserved
#include "RenderGraph.hpp"
#include "Surge/Graphics/RHI/RHI.hpp"

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
        // TODO
    }

    void RenderGraph::Execute(FrameContext& ctx)
    {
        for(Scope<RenderPass>& pass : mPasses)
            pass->Execute(ctx, mBlackboard);

        // TODO: ClearList should be here, not in a separate function
        //mBlackboard.ClearLists();
    }

    void RenderGraph::OnImGuiRender()
    {
        ImGui::Begin("Render Graph");
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

    void RenderGraph::SortByDependencies(Vector<RenderPass*>& nodes)
    {

    }

}
