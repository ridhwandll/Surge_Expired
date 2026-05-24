// Copyright (c) - SurgeTechnologies - All rights reserved
#include "SwapchainPass.hpp"
#include "Surge/Core/Core.hpp"

namespace Surge
{
    void SwapchainPass::Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard)
    {
        mImageReads.push_back(blackBoard.FinalImage);
    }

    void SwapchainPass::Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard)
    {

    }

    void SwapchainPass::Resize(Uint width, Uint height, FrameBlackboard& blackBoard)
    {

    }

    void SwapchainPass::OnImGuiRender(FrameBlackboard& blackBoard)
    {

    }

    void SwapchainPass::Shutdown()
    {

    }

}
