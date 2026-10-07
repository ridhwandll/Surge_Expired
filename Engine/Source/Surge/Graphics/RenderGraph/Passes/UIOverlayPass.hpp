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
    *
    * Sprites and MSDF text go through one pipeline (UI.glsl) and are batched in Blackboard.UIDrawOrder,
    * so a panel drawn above a button also covers the button's label (hierarchy order is respected)
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

        // Packed into QuadVertex::TextureIndex, must match UI.glsl
        static constexpr Uint TEXT_GLYPH_FLAG = 0x80000000u;
        static constexpr Uint PX_RANGE_SHIFT = 8;

    private:
        void AppendSprite(const QuadSubmitCmd& quad);
        void AppendText(const TextSubmitCmd& text);
        bool PushQuad(const glm::vec4 (&positions)[4], const glm::vec2 (&uvs)[4], Uint packedColor, ImageHandle texture, Uint textureIndexFlags); // false once the limits are hit
        void FlushQuadBatch();

    private:
        GraphicsRHI* mRHI;
        FrameContext mCurrentFrameCtx;

        // Pipeline (Depth Disabled)
        PipelineHandle mUIPipeline;

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
        ImageHandle mWhiteImage;

        Vector<float> mLineLayoutCache;
    };
}
