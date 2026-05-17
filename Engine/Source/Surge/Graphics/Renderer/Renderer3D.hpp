// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Memory.hpp"
#include "Surge/Graphics/RHI/RHI.hpp"
#include "Surge/Graphics/Camera/RuntimeCamera.hpp"
#include "Surge/Graphics/Mesh/Mesh.hpp"
#include "Surge/Graphics/Renderer/Lights.hpp"
#include "Surge/ECS/Components.hpp"

namespace Surge
{
    class Scene;
    class EditorCamera;
    struct RendererData;

    class Renderer3D
    {
    public:
        static constexpr Uint MAX_LIGHTS = 256;
    public:
        Renderer3D() = default;
        ~Renderer3D() = default;

        TextureHandle GetFinalImage() const { return TextureHandle::Invalid(); /*TODO*/ }

    private:
        void Initialize(GraphicsRHI* rhi, RendererData* data);
        void Shutdown();

        // Called by Renderer, not meant to be called directly
        void BeginFrame(const FrameContext& frameCtx);
        void EndFrame();
        void SubmitMesh(const glm::mat4& transform, const Ref<Mesh>& mesh, const Ref<Material>& material);
        void SubmitLight(const LightComponent& light, const glm::vec3& position, const glm::vec3& rotation);
        void OnWindowResize(Uint width, Uint height);

        void OnImGuiRender();
    private:
        struct MeshDrawCmd
        {
            glm::mat4 Transform;
            Ref<Mesh> Mesh;
            Ref<Material> Material;
        };

    private:
        FrameContext mCurrentFrameCtx;
        GraphicsRHI* mRHI;
        RendererData* mData;

        PipelineHandle m3DPipeline;

        Vector<MeshDrawCmd> mMeshDrawCommands;

        struct LightData
        {
            GPULight Lights[MAX_LIGHTS];
            Uint Count = 0;
        };
        BufferHandle mGPULightBuffer = BufferHandle::Invalid();
        Uint mLightBufferIndex = UINT32_MAX;
        LightData mLightCPU = {};

        friend class Renderer;
    };

} // namespace Surge