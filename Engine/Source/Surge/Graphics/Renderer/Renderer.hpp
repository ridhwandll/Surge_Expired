// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Memory.hpp"
#include "Surge/ECS/Components.hpp"
#include "Lights.hpp"
#include "Surge/Graphics/RHI/RHI.hpp"

#define FRAMES_IN_FLIGHT 2
#define BASE_SHADER_PATH "Engine/Assets/Shaders" //Sadkek, we don't have an asset manager yet

#include <volk.h>
#include <vk_mem_alloc.h>

namespace Surge
{
    class SURGE_API Scene;
    struct RendererData
    {
        // Camera
        glm::vec3 CameraPosition;
        glm::mat4 ViewMatrix;
        glm::mat4 ProjectionMatrix;
        glm::mat4 ViewProjection;
    };

    class EditorCamera;
    class SURGE_API Renderer
    {
    public:
		static constexpr Uint MAX_QUADS = 10000;
		static constexpr Uint MAX_VERTICES = MAX_QUADS * 4;
		static constexpr Uint MAX_INDICES = MAX_QUADS * 6;

		struct QuadVertex
		{
			glm::vec3 Position;
			glm::vec4 Color;
			glm::vec2 UV;
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
		void Submit(const glm::mat4& transform, const glm::vec4& color);

        RendererData* GetData() { return mData.get(); }
    private:
		Vector<QuadVertex> mVertexData;
		int mVertexCount = 0;
        int mQuadCount = 0;

        PipelineHandle mPipeline;
        BufferHandle mVertexBuffer;        
        BufferHandle mIndexBuffer;   

        Scope<GraphicsRHI> mRHI;
        Scope<RendererData> mData;
    };
} // namespace Surge