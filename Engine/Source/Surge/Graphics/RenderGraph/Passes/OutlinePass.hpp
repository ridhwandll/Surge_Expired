// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RenderGraph/RenderPass.hpp"

namespace Surge
{
    /*
    * OutlinePass:
    * Reads : Nothing
    * Writes: Blackboard.OutlineMask
    */

    class GraphicsRHI;
    class OutlinePass : public RenderPass
    {
    public:
        OutlinePass() { mName = "OutlinePass"; mGroup = PassGroup::OUTLINE_MASK; }
        virtual ~OutlinePass() = default;
    
        void Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard) override;
        void Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard) override;
        void Resize(Uint width, Uint height, FrameBlackboard& blackBoard) override;
        void OnImGuiRender(FrameBlackboard& blackBoard) override;
        void Shutdown(FrameBlackboard& blackBoard) override;

    private:
        GraphicsRHI* mRHI;
        PipelineHandle mOutlineMaskPipeline;
    };
}
