// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RenderGraph/RenderGraph.hpp"
#include "Surge/Graphics/Renderer/Lights.hpp"
#include <glm/glm.hpp>

// ---------------------------------------------------------------------------
// ShadowRenderer.hpp
//   Manages shadow-map rendering passes registered into the RenderGraph.
//   Replaces ShadowMapProcedure.
//
//   Usage:
//     shadowRenderer.RegisterPasses(renderGraph, dirLight, cascadeMatrices);
//     RGTextureHandle shadowMap = shadowRenderer.GetShadowMapHandle();
//     // Then read shadowMap in the lighting pass.
// ---------------------------------------------------------------------------

namespace Surge
{
    class SURGE_API ShadowRenderer
    {
    public:
        ShadowRenderer()  = default;
        ~ShadowRenderer() = default;

        SURGE_DISABLE_COPY_AND_MOVE(ShadowRenderer);

        void Initialize();
        void Shutdown();

        // Register the shadow-map generation pass(es) into the supplied graph.
        // Must be called BEFORE graph.Compile().
        void RegisterPasses(RenderGraph& graph,
                            const DirectionalLight& dirLight,
                            const glm::mat4& sceneViewProjection);

        // Returns the logical handle for the shadow map atlas produced this frame.
        // Valid only after RegisterPasses() has been called.
        RGTextureHandle GetShadowMapHandle() const { return mShadowMapHandle; }

        // Configure shadow quality
        void SetCascadeCount(uint32_t count);
        void SetShadowResolution(uint32_t resolution);

        uint32_t GetCascadeCount()    const { return mCascadeCount; }
        uint32_t GetShadowResolution() const { return mResolution; }

    private:
        uint32_t        mCascadeCount    = 4;
        uint32_t        mResolution      = 2048;
        RGTextureHandle mShadowMapHandle = RGTextureHandle::Null();
    };

} // namespace Surge
