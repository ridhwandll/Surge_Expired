// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RenderGraph/RenderPass.hpp"

namespace Surge
{
    /*
    * SkyPass:
    * Reads : Nothing
    * Writes: BlackBoard.MainPassColorImage
    */

    class GraphicsRHI;
    class SkyPass : public RenderPass
    {
    public:
        SkyPass() { mName = "SkyPass"; mGroup = PassGroup::MAIN_SCENE; }
        virtual ~SkyPass() = default;
    
        void Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard) override;
        void Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard) override;
        void Resize(Uint width, Uint height, FrameBlackboard& blackBoard) override;
        void OnImGuiRender(FrameBlackboard& blackBoard) override;
        void Shutdown(FrameBlackboard& blackBoard) override;

    private:
        GraphicsRHI* mRHI;
        DescriptorSetHandle mFrameDescriptorSet;
        PipelineHandle mSkyPipeline;
    };
}
