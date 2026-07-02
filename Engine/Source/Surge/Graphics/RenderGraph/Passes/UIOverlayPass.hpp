// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RenderGraph/RenderPass.hpp"
#include "Renderer2DPass.hpp"

namespace Surge
{
    /*
    * UIOverlayPass:
    * Reads : Blackboard.FinalImage (Post-Processed SDR image)
    * Writes: Blackboard.UIOverlayFramebuffer
    */

    class GraphicsRHI;
    class UIOverlayPass : public RenderPass
    {
    public:
        UIOverlayPass() { mName = "UIOverlayPass"; mGroup = PassGroup::UI_OVERLAY; }
        virtual ~UIOverlayPass() = default;

        void Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard) override;
        void Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard) override;
        void Resize(Uint width, Uint height, FrameBlackboard& blackBoard) override;
        void OnImGuiRender(FrameBlackboard& blackBoard) override;
        void Shutdown(FrameBlackboard& blackBoard) override;

    public:
        // Lower limits specifically for UI
        static constexpr Uint MAX_UI_QUADS_TOTAL = 10000;
        static constexpr Uint MAX_UI_QUADS_PER_BATCH = 2500;
        static constexpr Uint MAX_UI_QUAD_BATCHES = 50;
        static constexpr Uint MAX_TEX_SLOTS_PER_BATCH = 16;

    private:
        void FlushQuadBatch();

    private:
        GraphicsRHI* mRHI;
        FrameContext mCurrentFrameCtx;

        // UI Orthographic Projection UBO
        BufferHandle mUIFrameUBOs[RHISettings::FRAMES_IN_FLIGHT];
        DescriptorSetHandle mUIFrameDescriptorSet;

        // Pipelines (Depth Disabled)
        PipelineHandle mUIQuadPipeline;
        PipelineHandle mUITextPipeline;

        // Batching State
        Uint mTotalQuadVertexCount = 0;
        Uint mTotalQuadCount = 0;
        Uint mCurrentFrameVertexOffset = 0;
        bool mMaxQuadCountReached = false;

        std::array<Renderer2DPass::QuadDrawCmd, MAX_UI_QUAD_BATCHES> mQuadDrawCommands {};
        Uint mQuadBatchCount = 0;
        Vector<DescriptorSetHandle> mTexDescriptorSets;

        Renderer2DPass::QuadBatchData mCurrentQuadBatch;
        BufferHandle mQuadVB[RHISettings::FRAMES_IN_FLIGHT];
        BufferHandle mQuadIB;

        Vector<float> mLineLayoutCache;
    };
}