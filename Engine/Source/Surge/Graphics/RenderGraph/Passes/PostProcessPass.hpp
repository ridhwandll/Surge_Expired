// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RenderGraph/RenderPass.hpp"

namespace Surge
{
   /*
    * PostProcessPass:
    * Reads : Blackboard.MainPassColorImage, BlackBoard.MainPassDepthImage, BlackBoard.OutlineMask
    * Writes: Blackboard.FinalImage
    */

    class GraphicsRHI;
    class PostProcessPass : public RenderPass
    {
    public:
        PostProcessPass() { mName = "PostProcessPass"; mGroup = PassGroup::POST_PROCESS; }
        virtual ~PostProcessPass() = default;
    
        void Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard) override;
        void Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard) override;
        void Resize(Uint width, Uint height, FrameBlackboard& blackBoard) override;
        void OnImGuiRender(FrameBlackboard& blackBoard) override;
        void Shutdown(FrameBlackboard& blackBoard) override;
    private:
        GraphicsRHI* mRHI;
        DescriptorSetHandle mPostProcessDescriptorSet;
        PipelineHandle mFullscreenPipeline;
    };
}
