// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RenderGraph/RenderPass.hpp"

namespace Surge
{
    /*
    * Renderer2DPass:
    * Reads : Nothing
    * Writes: Blackboard.MainPassColorImage
    */

    class GraphicsRHI;
    class Renderer2DPass : public RenderPass
    {
    public:
        Renderer2DPass() { mGroup = PassGroup::MAIN_SCENE; mName = "Renderer2DPass"; }
        virtual ~Renderer2DPass() = default;

        void Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard) override;
        void Execute(const FrameContext& ctx, const FrameBlackboard& blackboard) override;
        void Resize(Uint width, Uint height, FrameBlackboard& blackBoard) override;
        void Shutdown(FrameBlackboard& blackBoard) override;
        void OnImGuiRender(FrameBlackboard& blackBoard) override;
    public:
        static constexpr Uint MAX_QUADS_TOTAL = 100000;    // 100k quads total, across all(10) batches
        static constexpr Uint MAX_QUADS_PER_BATCH = 10000; // 10k quads in 1 batch

        // 1 draw call for ALL lines
        static constexpr Uint MAX_LINES_TOTAL = 300000;
        static constexpr Uint MAX_LINES_PER_BATCH = 300000;
    private:
        void RegisterQuadDrawcall();
        void RegisterLineDrawcall();

    private:
        //Quads
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
        struct QuadBatchData
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

        // Lines
        struct LineDrawCmd
        {
            Uint VertexOffset;
            Uint VertexCount;
        };
        struct LineVertex
        {
            glm::vec3 Position;
            glm::vec4 Color;
        };
        struct LineBatchData
        {
            Vector<LineVertex> VertexData;
            Uint VertexCount = 0;
            Uint LineCount = 0;
            void Reset()
            {
                VertexCount = 0;
                LineCount = 0;
            }
        };
    private:
        static constexpr Uint MAX_QUAD_BATCHES = MAX_QUADS_TOTAL / MAX_QUADS_PER_BATCH; // = 10
        static constexpr Uint MAX_LINE_BATCHES = MAX_LINES_TOTAL / MAX_LINES_PER_BATCH; // = 1

        GraphicsRHI* mRHI = nullptr;
        FrameContext mCurrentFrameCtx;

        // TODO: Textures in Renderer2D
        //DescriptorSetHandle mTexDescriptorSets[MAX_BATCHES_PER_FRAME];

        // Quads
        Uint mTotalQuadlVertexCount = 0;
        Uint mTotalQuadCount = 0;
        Uint mCurrentFrameVertexOffset = 0;
        bool mMaxQuadCountReached = false;

        std::array<QuadDrawCmd, MAX_QUAD_BATCHES> mQuadDrawCommands {};
        Uint mQuadDrawCommandCount = 0;

        QuadBatchData mCurrentQuadBatch;
        PipelineHandle m2DPipeline;
        BufferHandle mQuadVB[RHISettings::FRAMES_IN_FLIGHT];
        BufferHandle mQuadIB;
        DescriptorSetHandle mFrameDescriptorSet;

        //Lines
        Uint mTotalLineVertexCount = 0;
        Uint mTotalLineCount = 0;
        Uint mCurrentLineVertexOffset = 0;
        bool mMaxLinesCountReached = false;

        std::array<LineDrawCmd, MAX_LINE_BATCHES> mLineDrawCommands {};
        Uint mLineDrawCommandCount = 0;

        LineBatchData mCurrentLineBatch;
        PipelineHandle m2DLinePipeline;
        BufferHandle mLineVB[RHISettings::FRAMES_IN_FLIGHT];
    };
}
