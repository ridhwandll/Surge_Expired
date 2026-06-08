// Copyright (c) - SurgeTechnologies - All rights reserved
#include "SkyPass.hpp"
#include "Surge/Core/Core.hpp"
#include <glm/gtc/type_ptr.hpp>

namespace Surge
{
    void SkyPass::Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard)
    {
        mRHI = rhi;

        PipelineDesc skyDesc = {};
        skyDesc.Shader_ = Core::GetRenderer()->GetShaderManager().Get("PreethamSky.glsl");
        skyDesc.TargetFramebuffer = blackBoard.MainPassFramebuffer;
        skyDesc.Raster.Cull = CullMode::NONE;
        skyDesc.Depth.TestEnable = true;
        skyDesc.Depth.WriteEnable = false;   // Do not write depth in sky pass

        // (Rid) This will change probably if we reverse Z
        // Reference equal to 1, we set gl_Position.z = 1 in PreethamSky.glsl, which matches the clear value of depth
        // Thus PreethamSky.glsl will only output to the pixels where depth is 1.0(the clear value)
        skyDesc.Depth.Op = CompareOp::EQUAL;

        skyDesc.Blend.Enable = false;
        skyDesc.DebugName = "PreethamSky";
        mSkyPipeline = mRHI->CreatePipeline(skyDesc);

        mFrameDescriptorSet = mRHI->CreateDescriptorSet(mSkyPipeline, DescriptorSetSlot::ZERO, DescriptorUpdateFrequency::DYNAMIC, "PreethamSky Set");
        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
        {
            DescriptorWrite frameDescriptorWrite = {};
            frameDescriptorWrite.Binding = 0;
            frameDescriptorWrite.Type = DescriptorType::UNIFORM_BUFFER;
            frameDescriptorWrite.Buffer = blackBoard.FrameUBOs[i];
            mRHI->UpdateDescriptorSet(mFrameDescriptorSet, &frameDescriptorWrite, 1, i);
        }

        mImageWrites.push_back(blackBoard.MainPassColorImage);
    }

    void SkyPass::Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard)
    {
        if (!blackBoard.Env.HasEnvironment)
            return;

        mRHI->CmdBindPipeline(ctx, mSkyPipeline);
        mRHI->CmdBindDescriptorSet(ctx, mSkyPipeline, mFrameDescriptorSet, DescriptorSetSlot::ZERO);

        struct SkyPushConstants
        {
            glm::vec3 SunDirection;
            float Turbidity;
            float Exposure;
            float SunIntensity;
            int EnableSunDisk;
        };
        float radElevation = glm::radians(blackBoard.Env.Elevation);
        float radAzimuth = glm::radians(blackBoard.Env.Azimuth);

        SkyPushConstants pc;
        pc.SunDirection.x = cos(radElevation) * sin(radAzimuth);
        pc.SunDirection.y = sin(radElevation);
        pc.SunDirection.z = cos(radElevation) * cos(radAzimuth);
        pc.Turbidity = blackBoard.Env.Turbidity;
        pc.Exposure = blackBoard.Env.Exposure;
        pc.SunIntensity = blackBoard.Env.SunIntensity;
        pc.EnableSunDisk = blackBoard.Env.EnableSunDisk;
        mRHI->CmdPushConstants(ctx, mSkyPipeline, ShaderType::FRAGMENT | ShaderType::VERTEX, 0, sizeof(SkyPushConstants), &pc);
        mRHI->CmdDraw(ctx, 3, 1, 0, 0);
    }

    void SkyPass::Resize(Uint width, Uint height, FrameBlackboard& blackBoard)
    {
        // Resize handled by GeometryPass
    }

    void SkyPass::OnImGuiRender(FrameBlackboard& blackBoard)
    {
    }

    void SkyPass::Shutdown(FrameBlackboard& blackBoard)
    {
        mRHI->DestroyDescriptorSet(mFrameDescriptorSet);
        mRHI->DestroyPipeline(mSkyPipeline);
    }
}
