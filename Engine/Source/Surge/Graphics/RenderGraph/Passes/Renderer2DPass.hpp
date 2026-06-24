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
        static constexpr Uint MAX_QUAD_BATCHES_PER_FRAME = 1000;
        static constexpr Uint MAX_TEX_SLOTS_PER_BATCH = 16; // TODO: Query from RHI caps

        // 1 draw call for ALL lines
        static constexpr Uint MAX_LINES_TOTAL = 300000;
        static constexpr Uint MAX_LINES_PER_BATCH = 300000;
        static constexpr Uint MAX_LINE_BATCHES_PER_FRAME = MAX_LINES_TOTAL / MAX_LINES_PER_BATCH; // = 1
    private:
        void FlushQuadBatch();
        void FlushLineBatch();

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
            Vector<QuadVertex> VertexData {};
            Uint VertexCount = 0;
            Uint QuadCount = 0;

            Uint TextureCount = 1; // (Rid) We Always start from 1, as slot 0 is reserved for the white texture
            std::array<ImageHandle, MAX_TEX_SLOTS_PER_BATCH> Textures {};

            constexpr static int MAX_TEX_IN_BATCH_REACHED = -1;
            int FindOrAssignTextureSlot(ImageHandle tex)
            {
                if (tex == ImageHandle::Invalid())
                    return 0;

                for(Uint i = 1; i < TextureCount; i++)
                {
                    if(Textures[i] == tex)
                        return (int)i;
                }

                if(TextureCount >= MAX_TEX_SLOTS_PER_BATCH)
                    return MAX_TEX_IN_BATCH_REACHED;

                Textures[TextureCount] = tex;
                return (int)TextureCount++;
            }

            void Reset()
            {
                VertexCount = 0;
                QuadCount = 0;
                TextureCount = 1; // (Rid) We Always start from 1, as slot 0 is reserved for the white texture (2)
                for(Uint i = 1; i < MAX_TEX_SLOTS_PER_BATCH; i++)
                    Textures[i] = ImageHandle::Invalid();
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

        GraphicsRHI* mRHI = nullptr;
        FrameContext mCurrentFrameCtx;

        // Quads
        Uint mTotalQuadlVertexCount = 0;
        Uint mTotalQuadCount = 0;
        Uint mCurrentFrameVertexOffset = 0;
        bool mMaxQuadCountReached = false;

        std::array<QuadDrawCmd, MAX_QUAD_BATCHES_PER_FRAME> mQuadDrawCommands {};
        Uint mQuadBatchCount = 0;
        Vector<DescriptorSetHandle> mTexDescriptorSets;

        QuadBatchData mCurrentQuadBatch;
        PipelineHandle m2DQuadPipeline;
        BufferHandle mQuadVB[RHISettings::FRAMES_IN_FLIGHT];
        BufferHandle mQuadIB;
        DescriptorSetHandle mFrameDescriptorSet;

        //Lines
        Uint mTotalLineVertexCount = 0;
        Uint mTotalLineCount = 0;
        Uint mCurrentLineVertexOffset = 0;
        bool mMaxLinesCountReached = false;

        std::array<LineDrawCmd, MAX_LINE_BATCHES_PER_FRAME> mLineDrawCommands {};
        Uint mLineBatchCount = 0;

        LineBatchData mCurrentLineBatch;
        PipelineHandle m2DLinePipeline;
        BufferHandle mLineVB[RHISettings::FRAMES_IN_FLIGHT];

        // Text
        PipelineHandle m2DTextPipeline;
        Vector<float> mLineLayoutCache;
    };
}
