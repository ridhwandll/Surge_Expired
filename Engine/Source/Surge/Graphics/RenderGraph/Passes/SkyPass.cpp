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
        float radElevation = glm::radians(blackBoard.Skybox.Elevation);
        float radAzimuth = glm::radians(blackBoard.Skybox.Azimuth);

        SkyPushConstants pc;
        pc.SunDirection.x = cos(radElevation) * sin(radAzimuth);
        pc.SunDirection.y = sin(radElevation);
        pc.SunDirection.z = cos(radElevation) * cos(radAzimuth);
        pc.Turbidity = blackBoard.Skybox.Turbidity;
        pc.Exposure = blackBoard.Skybox.Exposure;
        pc.SunIntensity = blackBoard.Skybox.SunIntensity;
        pc.EnableSunDisk = blackBoard.Skybox.EnableSunDisk;
        mRHI->CmdPushConstants(ctx, mSkyPipeline, ShaderType::FRAGMENT | ShaderType::VERTEX, 0, sizeof(SkyPushConstants), &pc);
        mRHI->CmdDraw(ctx, 3, 1, 0, 0);
    }

    void SkyPass::Resize(Uint width, Uint height, FrameBlackboard& blackBoard)
    {
        // Resize handled by GeometryPass
    }

    void SkyPass::OnImGuiRender(FrameBlackboard& blackBoard)
    {
        ImFont* boldFont = ImGui::GetIO().Fonts->Fonts[1];
        ImGui::PushFont(boldFont, 25.0f);
        ImGui::TextUnformatted("Sky Pass");
        ImGui::Separator();
        ImGui::PopFont();

        ImGui::Checkbox("Sun Disk", &blackBoard.Skybox.EnableSunDisk);
        ImGui::SliderFloat("Turbidity", &blackBoard.Skybox.Turbidity, 1.5f, 10.0f, "%.2f");
        ImGui::DragFloat("Exposure", &blackBoard.Skybox.Exposure, 0.001, 0.001f, 2.0f);
        ImGui::DragFloat("Sun Intensity", &blackBoard.Skybox.SunIntensity, 0.01, 1.0f, 10.0f);
        ImGui::DragFloat("Elevation", &blackBoard.Skybox.Elevation, 0.5f);
        ImGui::DragFloat("Azimuth", &blackBoard.Skybox.Azimuth, 0.5f);
    }

    void SkyPass::Shutdown(FrameBlackboard& blackBoard)
    {
        mRHI->DestroyDescriptorSet(mFrameDescriptorSet);
        mRHI->DestroyPipeline(mSkyPipeline);
    }
}
