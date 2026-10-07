// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RenderGraph/RenderPass.hpp"

namespace Surge
{
    /*
    * AmbientOcclusionPass (GTAO):
    * Reads : BlackBoard.MainPassDepthImage
    * Writes: BlackBoard.AOImage, BlackBoard.AOLinearDepth
    *
    * Internally runs up to 4 fullscreen passes at AO resolution (manages its own renderpasses, like ShadowPass):
    * LinearDepth -> GTAO -> Blur(Horizontal) -> Blur(Vertical)
    * Controlled by BlackBoard.AOSettings (quality presets, resolution scale, on/off)
    */

    class GraphicsRHI;
    class AmbientOcclusionPass : public RenderPass
    {
    public:
        AmbientOcclusionPass() { mName = "AmbientOcclusionPass"; mGroup = PassGroup::AMBIENT_OCCLUSION; }
        virtual ~AmbientOcclusionPass() = default;

        void Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard) override;
        void Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard) override;
        void Resize(Uint width, Uint height, FrameBlackboard& blackBoard) override;
        void OnImGuiRender(FrameBlackboard& blackBoard) override;
        void Shutdown(FrameBlackboard& blackBoard) override;

    private:
        glm::uvec2 ComputeAOSize(float resolutionScale) const;
        void RecreateTargets(float resolutionScale);
        void UpdateDescriptorSets(Uint frameIndex);
        void FullscreenPass(const FrameContext& ctx, FramebufferHandle framebuffer, PipelineHandle pipeline, DescriptorSetHandle set, const void* pushConstants, Uint pushConstantsSize);

    private:
        GraphicsRHI* mRHI = nullptr;

        // Owned by GeometryPass/AmbientOcclusionPass, handles stay stable across resizes
        ImageHandle mSceneDepth;
        ImageHandle mLinearDepth;
        ImageHandle mAO;
        ImageHandle mAOBlur; // Ping-pong target for the separable blur
        SamplerHandle mPointClampSampler;

        FramebufferHandle mLinearDepthFramebuffer;
        FramebufferHandle mAOFramebuffer;
        FramebufferHandle mBlurHFramebuffer; // mAO     -> mAOBlur
        FramebufferHandle mBlurVFramebuffer; // mAOBlur -> mAO

        PipelineHandle mLinearDepthPipeline;
        PipelineHandle mGTAOPipeline;
        PipelineHandle mBlurPipeline; // Shared by both blur directions (compatible renderpasses)

        DescriptorSetHandle mLinearDepthSet;
        DescriptorSetHandle mGTAOSet;
        DescriptorSetHandle mBlurHSet;
        DescriptorSetHandle mBlurVSet;

        glm::uvec2 mFullSize = { 1, 1 };
        glm::uvec2 mAOSize = { 1, 1 };
        bool mRecreatePending = false;
        bool mDescriptorsDirty[RHISettings::FRAMES_IN_FLIGHT] = {};
    };
}
