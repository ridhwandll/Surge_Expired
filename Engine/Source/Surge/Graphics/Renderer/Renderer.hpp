// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Memory.hpp"
#include "Surge/ECS/Components.hpp"
#include "Lights.hpp"
#include "Surge/Graphics/RHI/RHI.hpp"

#define BASE_SHADER_PATH "Engine/Assets/Shaders" //Sadkek, we don't have an asset manager yet

#include <volk.h>
#include <vk_mem_alloc.h>

namespace Surge
{
    class Scene;
    struct RendererData
    {
        // Camera
        glm::vec3 CameraPosition;
        glm::mat4 ViewMatrix;
        glm::mat4 ProjectionMatrix;
        glm::mat4 ViewProjection;
    };

    class EditorCamera;
    class Renderer
    {
    public:
		static constexpr Uint MAX_QUADS_TOTAL = 100000; // 100k quads total, across all(10) batches
		static constexpr Uint MAX_QUADS_PER_BATCH = 10000; // 10k quads in 1 batch

		struct QuadVertex
		{
			glm::vec3 Position;
            Uint Color;
			glm::vec2 UV;
			Uint TextureIndex; // Bindless index
		};

		struct QuadPushConstants
		{
			glm::mat4 ViewProj;
		};

    public:
        Renderer() = default;
        ~Renderer() = default;

        void Initialize();
        void Shutdown();

        void BeginFrame(const RuntimeCamera& camera, const glm::mat4& transform);
        void BeginFrame(const EditorCamera& camera);
        void EndFrame();
		void Submit(const glm::mat4& transform, const glm::vec4& color, TextureHandle texture = TextureHandle::Invalid());
		void OnWindowResize(Uint width, Uint height);

		const Scope<GraphicsRHI>& GetRHI() const { return mRHI; }
		void AddImGuiRenderCallback(std::function<void()> callback) { if (callback) { mImGuiRenderCallbacks.push_back(callback); } }
        RendererData* GetData() { return mData.get(); }

        // Renderer 2D methods TODO: move this to Renderer2D class
		int GetQuadCount() const { return mTotalQuadCount; }
		int GetvertexCount() const { return mTotalVertexCount; }
    private:
		void FlushBatch();
        void OnImGuiRender();
    private:
        struct BatchData
        {
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
		Vector<std::function<void()>> mImGuiRenderCallbacks;
		Vector<QuadVertex> mVertexData;

		// Renderer 2D data TODO: move this to Renderer2D class
		TextureHandle mWhiteTexture;
		SamplerHandle mQuadSampler;
		BatchData mCurrentBatch;
        Uint mTotalVertexCount = 0;
        Uint mTotalQuadCount = 0;
        Uint mCurrentFrameVertexOffset = 0;
		bool mMaxQuadCountReached = false;

        PipelineHandle mPipeline;
        BufferHandle mVertexBuffers[RHI_FRAMES_IN_FLIGHT];
        BufferHandle mIndexBuffer;   
        TextureHandle mOffscreenColor;
		FramebufferHandle mOffscreenFramebuffer;
        glm::vec4 mClearColor = { 0.1f, 0.1f, 0.1f, 1.0f };

        Scope<GraphicsRHI> mRHI;
        Scope<RendererData> mData;
    };
} // namespace Surge