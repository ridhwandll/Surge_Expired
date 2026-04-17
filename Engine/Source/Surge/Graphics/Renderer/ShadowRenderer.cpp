// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Renderer/ShadowRenderer.hpp"

namespace Surge
{
    void ShadowRenderer::Initialize()
    {
        // Shadow-map resources are created each frame by RegisterPasses()
        // so there is nothing to allocate here.
    }

    void ShadowRenderer::Shutdown()
    {
        mShadowMapHandle = RGTextureHandle::Null();
    }

    void ShadowRenderer::SetCascadeCount(uint32_t count)
    {
        SG_ASSERT(count > 0 && count <= 4, "ShadowRenderer: cascade count must be between 1 and 4");
        mCascadeCount = count;
    }

    void ShadowRenderer::SetShadowResolution(uint32_t resolution)
    {
        SG_ASSERT(resolution > 0, "ShadowRenderer: resolution must be > 0");
        mResolution = resolution;
    }

    void ShadowRenderer::RegisterPasses(RenderGraph& graph,
                                         const DirectionalLight& dirLight,
                                         const glm::mat4& sceneViewProjection)
    {
        (void)dirLight;
        (void)sceneViewProjection;

        // Declare the shadow-map atlas as a transient depth texture.
        // Width = resolution * cascadeCount to pack all cascades side-by-side.
        RGTextureDesc shadowDesc;
        shadowDesc.Width    = mResolution * mCascadeCount;
        shadowDesc.Height   = mResolution;
        shadowDesc.Format   = RHI::ImageFormat::Depth32F;
        shadowDesc.Usage    = RHI::ImageUsage::Attachment | RHI::ImageUsage::Sampled;
        shadowDesc.Lifetime = ResourceLifetime::Transient;
        shadowDesc.Name     = "ShadowMap";

        graph.AddPass(
            "ShadowMapPass",
            [this, shadowDesc](RenderGraphBuilder& builder) {
                mShadowMapHandle = builder.WriteDepth(builder.Create(shadowDesc));
            },
            [this](RHI::CommandBufferHandle cmd, const PassResources& /*res*/) {
                (void)cmd;
                // TODO: render scene depth from light's perspective for each cascade
                // cascade matrices are uploaded via push constants or a UBO
            });
    }

} // namespace Surge
