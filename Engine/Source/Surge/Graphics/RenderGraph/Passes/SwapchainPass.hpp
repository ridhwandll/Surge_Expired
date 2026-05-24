// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RenderGraph/RenderPass.hpp"

namespace Surge
{
    class GraphicsRHI;
    class SwapchainPass : public RenderPass
    {
    public:
        SwapchainPass() { mName = "SwapchainPass"; mGroup = PassGroup::SWAPCHAIN; }
        virtual ~SwapchainPass() = default;
    
        virtual void Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard) override;
        virtual void Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard) override;
        virtual void Resize(Uint width, Uint height, FrameBlackboard& blackBoard) override;
        virtual void OnImGuiRender(FrameBlackboard& blackBoard) override;
        virtual void Shutdown() override;

    private:
        FrameContext mCurrentFrameCtx;
        GraphicsRHI* mRHI;
        DescriptorSetHandle mFrameDescriptorSet;

        BufferHandle mLightUBOs[RHISettings::FRAMES_IN_FLIGHT];
        PipelineHandle m3DPipeline;
    };
}
