// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Memory.hpp"
#include "Surge/Graphics/RHI/RHI.hpp"
#include "Surge/Graphics/Camera/RuntimeCamera.hpp"
#include "Surge/Graphics/RHI/RHISettings.hpp"

namespace Surge
{
    class Scene;
    class EditorCamera;
    struct RendererData;
    struct RHISettings;
    class Renderer2D
    {
    public:
        static constexpr Uint MAX_QUADS_TOTAL = 100000;    // 100k quads total, across all(10) batches
        static constexpr Uint MAX_QUADS_PER_BATCH = 10000; // 10k quads in 1 batch

        struct QuadVertex
        {
            glm::vec3 Position;
            Uint Color; // Packed glm::vec4 color
            glm::vec2 UV;
            Uint TextureIndex; // Bindless index
        };

    public:
        Renderer2D() = default;
        ~Renderer2D() = default;

        int GetQuadCount() const { return mTotalQuadCount; }
        int GetvertexCount() const { return mTotalVertexCount; }

    private:
        void Initialize(GraphicsRHI* rhi, RendererData* data);
        void Shutdown();

        // Called by Renderer, not meant to be called directly
        void BeginFrame(const FrameContext& frameCtx);
        void Submit(const glm::mat4& transform, const glm::vec4& color, TextureHandle texture = TextureHandle::Invalid());
        void EndFrame();

        void WriteToGPUBuffer();

        void OnWindowResize(Uint width, Uint height);
        void OnImGuiRender();
    private:
        struct QuadDrawCmd
        {
            Uint VertexOffset = 0;
            Uint QuadCount = 0;
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
        FrameContext mCurrentFrameCtx;
        BatchData mCurrentBatch;
        Vector<QuadDrawCmd> mDrawCommands; // We store the draw commands for each batch, and execute them all at the end of the frame in one go

        Uint mTotalVertexCount = 0;
        Uint mTotalQuadCount = 0;
        Uint mCurrentFrameVertexOffset = 0;
        bool mMaxQuadCountReached = false;

        PipelineHandle m2DPipeline;
        BufferHandle mVertexBuffers[RHISettings::FRAMES_IN_FLIGHT];
        BufferHandle mIndexBuffer;   

        GraphicsRHI* mRHI;
        RendererData* mData;

        friend class Renderer;
    };
} // namespace Surge