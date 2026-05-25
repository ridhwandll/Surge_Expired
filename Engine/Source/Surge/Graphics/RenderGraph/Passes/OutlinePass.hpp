// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RenderGraph/RenderPass.hpp"

namespace Surge
{
    class GraphicsRHI;
    class OutlinePass : public RenderPass
    {
    public:
        OutlinePass() { mName = "OutlinePass"; mGroup = PassGroup::MAIN_SCENE; }
        virtual ~OutlinePass() = default;
    
        virtual void Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard) override;
        virtual void Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard) override;
        virtual void Resize(Uint width, Uint height, FrameBlackboard& blackBoard) override;
        virtual void OnImGuiRender(FrameBlackboard& blackBoard) override;
        virtual void Shutdown() override;

    private:
        FrameContext mCurrentFrameCtx;
        GraphicsRHI* mRHI;

        PipelineHandle mStencilWritePipeline;
        PipelineHandle mOutlineDrawPipeline;
    };
}
