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
        struct PushConstantData
        {
            glm::mat4 Transform;
            Uint LightCount;
        };

        struct Data
        {
            // FrameUBO
            DescriptorSetHandle FrameDescriptorSet;
            BufferHandle FrameUBO;
            BufferHandle LightUBO;
        };

    public:
        Renderer3D() = default;
        ~Renderer3D() = default;

        ImageHandle GetFinalImage() const { return ImageHandle::Invalid(); /*TODO*/ }

    private:
        void Initialize(GraphicsRHI* rhi, RendererData* data);
        void Shutdown();

        // Called by Renderer, not meant to be called directly
        void BeginFrame(const FrameContext& frameCtx, Uint submitCount = 0);
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
        Data m3DData;

        Vector<MeshDrawCmd> mMeshDrawCommands;

        Uint mLightBufferIndex = UINT32_MAX;
        Vector<Light> mLightCPU = {};
        PipelineHandle m3DPipeline;

        friend class Renderer;
    };

} // namespace Surge