// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RenderGraph/RenderPass.hpp"

namespace Surge
{
    class GraphicsRHI;
    class GeometryPass : public RenderPass
    {
    public:
        GeometryPass() { mName = "GeometryPass"; mGroup = PassGroup::MAIN_SCENE; }
        virtual ~GeometryPass() = default;
    
        virtual void Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard) override;
        virtual void Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard) override;
        virtual void Resize(Uint width, Uint height, FrameBlackboard& blackBoard) override;
        virtual void OnImGuiRender(FrameBlackboard& blackBoard) override;
        virtual void Shutdown(FrameBlackboard& blackBoard) override;
    public:
        static constexpr Uint MAX_LIGHTS = 256;

    private:
        struct PushConstantData
        {
            glm::mat4 Transform;
            Uint LightCount;
        };

    private:
        FrameContext mCurrentFrameCtx;
        GraphicsRHI* mRHI;
        DescriptorSetHandle mFrameDescriptorSet;

        BufferHandle mLightUBOs[RHISettings::FRAMES_IN_FLIGHT];
        PipelineHandle m3DPipeline;
    };
}
