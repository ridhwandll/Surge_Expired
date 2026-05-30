// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RenderGraph/RenderPass.hpp"

namespace Surge
{
    /*
    * GeometryPass:
    * Reads : BlackBoard.ShadowPassImage
    * Writes: Blackboard.MainPassColorImage, BlackBoard.MainPassDepthImage
    */

    class GraphicsRHI;
    class GeometryPass : public RenderPass
    {
    public:
        GeometryPass() { mName = "GeometryPass"; mGroup = PassGroup::MAIN_SCENE; }
        virtual ~GeometryPass() = default;

        void Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard) override;
        void Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard) override;
        void Resize(Uint width, Uint height, FrameBlackboard& blackBoard) override;
        void OnImGuiRender(FrameBlackboard& blackBoard) override;
        void Shutdown(FrameBlackboard& blackBoard) override;
    public:
        static constexpr Uint MAX_LIGHTS = 256;

    private:
        struct PushConstantData
        {
            glm::mat4 Transform;
            Uint LightCount;
        };

    private:
        GraphicsRHI* mRHI;
        DescriptorSetHandle mFrameDescriptorSet;
        DescriptorSetHandle mShadowMapDescriptorSet;

        SamplerHandle mShadowSampler;
        BufferHandle mLightUBOs[RHISettings::FRAMES_IN_FLIGHT];
        PipelineHandle m3DPipeline;
    };
}
