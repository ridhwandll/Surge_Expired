// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RenderGraph/RenderPass.hpp"

#define MAX_CASCADE_COUNT 3
namespace Surge
{
    /*
    * ShadowPass:
    * Reads : Nothing
    * Writes: Blackboard.ShadowPassFramebuffer
    */

    class GraphicsRHI;
    class ShadowPass : public RenderPass
    {
    public:
        ShadowPass() { mName = "ShadowPass"; mGroup = PassGroup::SHADOW; }
        virtual ~ShadowPass() = default;

        void Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard) override;
        void Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard) override;
        void Resize(Uint width, Uint height, FrameBlackboard& blackBoard) override;
        void OnImGuiRender(FrameBlackboard& blackBoard) override;
        void Shutdown(FrameBlackboard& blackBoard) override;
    private:
        void CalculateCascades(const FrameBlackboard& bb);
    private:
        struct PushConstantData
        {
            glm::mat4 Transform;
            Uint LightCount;
        };

    private:
        GraphicsRHI* mRHI;
        PipelineHandle mShadowPipeline;

        float mCascadeSplitLambda = 0.91f;
        std::array<float, MAX_CASCADE_COUNT> mCascadeSplits = {};
        std::array<glm::mat4, MAX_CASCADE_COUNT> mLightViewProjections = {};
        std::array<float, MAX_CASCADE_COUNT> mCascadeSplitDepths = {};
        float mShadowMapResolution;


    };
}
