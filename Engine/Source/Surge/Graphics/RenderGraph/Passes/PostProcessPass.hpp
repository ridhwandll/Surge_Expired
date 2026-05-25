// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RenderGraph/RenderPass.hpp"

namespace Surge
{
    class GraphicsRHI;
    class PostProcessPass : public RenderPass
    {
    public:
        PostProcessPass() { mName = "PostProcessPass"; mGroup = PassGroup::POST_PROCESS; }
        virtual ~PostProcessPass() = default;
    
        virtual void Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard) override;
        virtual void Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard) override;
        virtual void Resize(Uint width, Uint height, FrameBlackboard& blackBoard) override;
        virtual void OnImGuiRender(FrameBlackboard& blackBoard) override;
        virtual void Shutdown(FrameBlackboard& blackBoard) override;

    private:
        FrameContext mCurrentFrameCtx;
        GraphicsRHI* mRHI;
        DescriptorSetHandle mFullScreenSet;
        PipelineHandle mFullscreenPipeline;
    };
}
