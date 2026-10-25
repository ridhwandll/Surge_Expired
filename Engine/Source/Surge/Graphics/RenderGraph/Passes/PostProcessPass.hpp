// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RenderGraph/RenderPass.hpp"

namespace Surge
{
   /*
    * PostProcessPass:
    * Reads : Blackboard.MainPassColorImage, BlackBoard.MainPassDepthImage, BlackBoard.AOImage, BlackBoard.AOLinearDepth, BlackBoard.BloomImage
    * Writes: Blackboard.FinalImage
    */

    class GraphicsRHI;
    class PostProcessPass : public RenderPass
    {
    public:
        // Player (RHISettings::RENDER_TO_SWAPCHAIN): renders straight into the swapchain render pass, no FinalImage round trip
        PostProcessPass() { mName = "PostProcessPass"; mGroup = RHISettings::RENDER_TO_SWAPCHAIN ? PassGroup::SWAPCHAIN : PassGroup::POST_PROCESS; }
        virtual ~PostProcessPass() = default;
    
        void Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard) override;
        void Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard) override;
        void Resize(Uint width, Uint height, FrameBlackboard& blackBoard) override;
        void RenderStatsViaImGui(FrameBlackboard& blackBoard) override;
        void Shutdown(FrameBlackboard& blackBoard) override;
    private:
        GraphicsRHI* mRHI;
        DescriptorSetHandle mPostProcessDescriptorSet;
        PipelineHandle mFullscreenPipeline;
    };
}
