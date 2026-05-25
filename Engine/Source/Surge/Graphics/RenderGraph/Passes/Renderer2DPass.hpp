// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RenderGraph/RenderPass.hpp"

namespace Surge
{
    class GraphicsRHI;
    class Renderer2DPass : public RenderPass
    {
    public:
        Renderer2DPass() { mGroup = PassGroup::MAIN_SCENE; mName = "Renderer2DPass"; }
        virtual ~Renderer2DPass() = default;

        virtual void Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard) override;
        virtual void Execute(const FrameContext& ctx, const FrameBlackboard& blackboard) override;
        virtual void Resize(Uint width, Uint height, FrameBlackboard& blackBoard) override;
        virtual void Shutdown(FrameBlackboard& blackBoard) override;
        virtual void OnImGuiRender(FrameBlackboard& blackBoard) override;
    public:
        static constexpr Uint MAX_BATCHES_PER_FRAME = 10;
        static constexpr Uint MAX_QUADS_TOTAL = 100000;    // 100k quads total, across all(10) batches
        static constexpr Uint MAX_QUADS_PER_BATCH = 10000; // 10k quads in 1 batch

    private:
        void RegisterDrawcall();

    private:
        struct QuadDrawCmd
        {
            Uint VertexOffset = 0;
            Uint QuadCount = 0;
        };

        struct QuadVertex
        {
            glm::vec3 Position;
            Uint Color; // Packed glm::vec4 color
            glm::vec2 UV;
            Uint TextureIndex;
        };

        struct BatchData
        {
            Vector<QuadVertex> VertexData;
            Uint VertexCount = 0;
            Uint QuadCount = 0;
            void Reset()
            {
                VertexCount = 0;
                QuadCount = 0;
            }
        };
    private:
        GraphicsRHI* mRHI = nullptr;
        FrameContext mCurrentFrameCtx;
        BatchData mCurrentBatch;
        Vector<QuadDrawCmd> mDrawCommands; // We store the draw commands for each batch, and execute them all at the end of the frame in one go

        // TODO: Textures in Renderer2D
        //DescriptorSetHandle mTexDescriptorSets[MAX_BATCHES_PER_FRAME];

        Uint mCurrentBatchIndex = 0;
        Uint mTotalVertexCount = 0;
        Uint mTotalQuadCount = 0;
        Uint mCurrentFrameVertexOffset = 0;
        bool mMaxQuadCountReached = false;

        PipelineHandle m2DPipeline;
        BufferHandle mVertexBuffers[RHISettings::FRAMES_IN_FLIGHT];
        BufferHandle mIndexBuffer;
        DescriptorSetHandle mFrameDescriptorSet;

    };
}
