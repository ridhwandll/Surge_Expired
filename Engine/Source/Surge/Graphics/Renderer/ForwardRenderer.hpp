// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RenderGraph/RenderGraph.hpp"
#include "Surge/Graphics/Renderer/Lights.hpp"
#include "Surge/Graphics/Camera/EditorCamera.hpp"
#include "Surge/Graphics/Mesh.hpp"
#include "Surge/Graphics/Shader/ShaderSet.hpp"
#include "Surge/ECS/Components.hpp"
#include <glm/glm.hpp>

// ---------------------------------------------------------------------------
// ForwardRenderer.hpp
//   High-level scene renderer that replaces the monolithic Renderer.
//   Built on top of the RenderGraph and the new RHI layer.
//
//   Usage per frame:
//     renderer.BeginFrame(camera, transform);
//     renderer.SubmitMesh(mesh, transform);
//     renderer.SubmitPointLight(light, pos);
//     renderer.EndFrame();  // compiles + executes the render graph
// ---------------------------------------------------------------------------

namespace Surge
{
    // Per-frame camera matrices uploaded to the GPU.
    struct CameraData
    {
        glm::mat4 View;
        glm::mat4 Projection;
        glm::mat4 ViewProjection;
        glm::vec4 Position; // w = unused
    };

    class SURGE_API ForwardRenderer
    {
    public:
        ForwardRenderer()  = default;
        ~ForwardRenderer() = default;

        SURGE_DISABLE_COPY_AND_MOVE(ForwardRenderer);

        void Initialize();
        void Shutdown();

        // ── Frame ──────────────────────────────────────────────────────────
        void BeginFrame(const EditorCamera& camera);

        // Generic camera: provide a view matrix and a projection matrix.
        void BeginFrame(const glm::mat4& view, const glm::mat4& projection,
                        const glm::vec3& cameraWorldPos);

        void EndFrame();

        // ── Scene submission ───────────────────────────────────────────────
        void SubmitMesh(MeshComponent& meshComp, const glm::mat4& transform);
        void SubmitPointLight(const PointLight& light);
        void SetDirectionalLight(const DirectionalLight& light);

        // ── Viewport ──────────────────────────────────────────────────────
        void SetRenderArea(uint32_t width, uint32_t height);
        uint32_t GetRenderWidth()  const { return mRenderWidth; }
        uint32_t GetRenderHeight() const { return mRenderHeight; }

        // ── Shaders & default resources ────────────────────────────────────
        Ref<Shader>& GetShader(const String& name) { return mShaderSet.GetShader(name); }
        const Ref<Texture2D>& GetWhiteTexture() const { return mWhiteTexture; }

        // ── Output ────────────────────────────────────────────────────────
        // Returns the physical texture handle for the final colour output.
        // Valid after EndFrame(); invalid if the RHI device is not yet wired up.
        RHI::TextureHandle GetFinalColorTexture() const;

        // Returns the RenderGraph for inspection / custom pass injection.
        const RenderGraph& GetRenderGraph() const { return mRenderGraph; }

    private:
        void RegisterGeometryPass();
        void RegisterLightingPass();

    private:
        // ── State ──────────────────────────────────────────────────────────
        RenderGraph mRenderGraph;
        ShaderSet   mShaderSet;
        Ref<Texture2D> mWhiteTexture;

        uint32_t mRenderWidth  = 1280;
        uint32_t mRenderHeight = 720;

        // ── Per-frame data ─────────────────────────────────────────────────
        CameraData        mCameraData    = {};
        DirectionalLight  mDirLight      = {};
        Vector<PointLight> mPointLights;

        struct DrawCommand
        {
            MeshComponent* MeshComp  = nullptr;
            glm::mat4      Transform = glm::mat4(1.0f);
        };
        Vector<DrawCommand> mDrawList;

        // ── RenderGraph resource handles ────────────────────────────────────
        RGTextureHandle mHDRColorTarget;
        RGTextureHandle mDepthTarget;
        RGTextureHandle mFinalColorTarget;

        bool mInitialized = false;
        bool mCompiled    = false;
    };

} // namespace Surge
