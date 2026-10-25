// Copyright (c) - SurgeTechnologies - All rights reserved
#include "SwapchainPass.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"

namespace Surge
{
    void SwapchainPass::Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard)
    {
        mRHI = rhi;

        // Editor: FinalImage is shown by ImGui inside the swapchain pass, reading it here makes the graph transition it to SAMPLED
        // Player: PostProcess + UI render straight into the swapchain render pass, there is nothing left to copy
        if(!RHISettings::RENDER_TO_SWAPCHAIN)
            mImageReads.push_back(blackBoard.FinalImage);
    }

    void SwapchainPass::Execute([[maybe_unused]] const FrameContext& ctx, [[maybe_unused]] const FrameBlackboard& blackBoard)
    {
        // Intentionally empty: the swapchain group still begins/ends its render pass at RenderGraph.cpp, ImGui draws into it
    }

    void SwapchainPass::Shutdown(FrameBlackboard&)
    {
    }
}
