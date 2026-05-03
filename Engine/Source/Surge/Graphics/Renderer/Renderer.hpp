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
        Scene* SceneContext;

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
        Renderer() = default;
        ~Renderer() = default;

        void Initialize();
        void Shutdown();

        void BeginFrame(const RuntimeCamera& camera, const glm::mat4& transform);
        void BeginFrame(const EditorCamera& camera);
        void EndFrame();
        void SetRenderArea(Uint width, Uint height);

        RendererData* GetData() { return mData.get(); }
        void SetSceneContext(Ref<Scene>& scene) { mData->SceneContext = scene.Raw(); }

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // RENDERER 2026
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    public:
		struct Vertex
		{
			glm::vec3 position;
			glm::vec3 color;
		};
    private:
        PipelineHandle mPipelineHandle;
        BufferHandle mVertexBuffer;        
        BufferHandle mIndexBuffer;

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    private:
        Scope<GraphicsRHI> mRHI;
        Scope<RendererData> mData;
    };
} // namespace Surge