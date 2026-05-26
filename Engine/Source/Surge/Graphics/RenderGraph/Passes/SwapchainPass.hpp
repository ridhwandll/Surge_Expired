// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RenderGraph/RenderPass.hpp"

namespace Surge
{
    /*
    * SwapchainPass:
    * Reads : blackBoard.FinalImage
    * 
    * if RHISettings::RENDER_TO_SWAPCHAIN == TRUE
    *    Writes: Swapchain
    * else
    *    Writes: Nothing
    */

    class GraphicsRHI;
    class SwapchainPass : public RenderPass
    {
    public:
        SwapchainPass() { mName = "SwapchainPass"; mGroup = PassGroup::SWAPCHAIN; }
        virtual ~SwapchainPass() = default;

        void Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard) override;
        void Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard) override;
        void Resize(Uint width, Uint height, FrameBlackboard& blackBoard) override;
        void OnImGuiRender(FrameBlackboard& blackBoard) override;
        void Shutdown(FrameBlackboard& blackBoard) override;
    private:
        GraphicsRHI* mRHI;
        PipelineHandle mPresentPipeline;
        DescriptorSetHandle mPresentSet;
    };
}
