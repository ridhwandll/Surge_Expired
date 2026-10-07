// Copyright (c) - SurgeTechnologies - All rights reserved
#include "PostProcessPass.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include <imgui.h>

namespace Surge
{
    void PostProcessPass::Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard)
    {
        mRHI = rhi;
        glm::uvec2 size = Core::GetWindow()->GetSize();

        PipelineDesc fsDesc = {};
        fsDesc.Shader_ = Core::GetRenderer()->GetShaderManager().Get("PostProcess.glsl");
        fsDesc.DebugName = "PostProcess";
        fsDesc.Raster.Topo = Topology::TRIANGLE_LIST;
        fsDesc.Raster.Polygon = PolygonMode::FILL;
        fsDesc.Raster.Cull = CullMode::NONE;
        fsDesc.Blend.Enable = false;
        fsDesc.Depth.TestEnable = false;
        fsDesc.Depth.WriteEnable = false;
        fsDesc.Stencil.Enable = false;

        ImageDesc colorDesc = {};
        colorDesc.Width = size.x;
        colorDesc.Height = size.y;
        colorDesc.Format = ImageFormat::RGBA8_UNORM;
        colorDesc.Usage = ImageUsage::COLOR_ATTACHMENT | ImageUsage::SAMPLED;
        colorDesc.DebugName = "FinalImage";
        colorDesc.Sampler = blackBoard.DefaultSampler;
        colorDesc.GenerateImGuiID = true;
        blackBoard.FinalImage = mRHI->CreateImage(colorDesc);

        FramebufferAttachment colorAttachment = {};
        colorAttachment.Handle = blackBoard.FinalImage;
        colorAttachment.Load = LoadOp::CLEAR;
        colorAttachment.Store = StoreOp::STORE;

        FramebufferDesc fbDesc = {};
        fbDesc.ColorAttachments[0] = colorAttachment;
        fbDesc.ColorAttachmentCount = 1;
        fbDesc.HasDepth = false;
        fbDesc.Width = size.x;
        fbDesc.Height = size.y;
        fbDesc.DebugName = "PostProcessFramebuffer";
        blackBoard.PostProcessFramebuffer = mRHI->CreateFramebuffer(fbDesc);
        fsDesc.TargetSwapchain = false;
        fsDesc.TargetFramebuffer = blackBoard.PostProcessFramebuffer;

        mFullscreenPipeline = rhi->CreatePipeline(fsDesc);
        mPostProcessDescriptorSet = mRHI->CreateDescriptorSet(mFullscreenPipeline, DescriptorSetSlot::ZERO, DescriptorUpdateFrequency::DYNAMIC, "SceneInputs");

        mImageReads.push_back(blackBoard.MainPassColorImage);
        mImageReads.push_back(blackBoard.MainPassDepthImage);
        mImageReads.push_back(blackBoard.OutlineMask);
        mImageReads.push_back(blackBoard.AOImage);
        mImageReads.push_back(blackBoard.AOLinearDepth);
        mImageReads.push_back(blackBoard.BloomImage);
        mImageWrites.push_back(blackBoard.FinalImage);
    }

    void PostProcessPass::Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard)
    {
        mRHI->CmdBindPipeline(ctx, mFullscreenPipeline);

        //TODO: remove this from here!! Never do it every frame
        std::array<DescriptorWrite, 6> writes;
        writes[0].Binding = 0;
        writes[0].Type = DescriptorType::TEXTURE;
        writes[0].Texture = blackBoard.MainPassColorImage;
        writes[0].Sampler = blackBoard.DefaultSampler;
        writes[1].Binding = 1;
        writes[1].Type = DescriptorType::TEXTURE;
        writes[1].Texture = blackBoard.OutlineMask;
        writes[1].Sampler = blackBoard.DefaultSampler;
        writes[2].Binding = 2;
        writes[2].Type = DescriptorType::TEXTURE;
        writes[2].Texture = blackBoard.MainPassDepthImage;
        writes[2].Sampler = blackBoard.DefaultSampler;
        writes[3].Binding = 3;
        writes[3].Type = DescriptorType::TEXTURE;
        writes[3].Texture = blackBoard.AOImage;
        writes[3].Sampler = blackBoard.PointClampSampler; // Gathered for the bilateral upsample, clamp avoids wrapping at screen edges
        writes[4].Binding = 4;
        writes[4].Type = DescriptorType::TEXTURE;
        writes[4].Texture = blackBoard.AOLinearDepth;
        writes[4].Sampler = blackBoard.PointClampSampler;
        writes[5].Binding = 5;
        writes[5].Type = DescriptorType::TEXTURE;
        writes[5].Texture = blackBoard.BloomImage;
        writes[5].Sampler = blackBoard.LinearClampSampler;
        mRHI->UpdateDescriptorSet(mPostProcessDescriptorSet, writes.data(), writes.size(), ctx.FrameIndex);
        mRHI->CmdBindDescriptorSet(ctx, mFullscreenPipeline, mPostProcessDescriptorSet, DescriptorSetSlot::ZERO);

        struct PostProcessPushConstants
        {
            glm::vec4 ColorThickness;
            glm::vec2 ScreenResolution;
            float VignetteIntensity;
            float VignetteSoftness;
            float Grain;
            int EnableFXAA;
            glm::vec2 CameraNearFar;
            glm::vec4 DepthParams; // x: P[2][2], y: P[3][2], z: 1 = perspective / 0 = orthographic
            glm::vec2 AOResolution;
            int AOMode; // 0 = off, 1 = on, 2 = AO only (debug)
            float _pad0;
            glm::vec4 BloomTint; // rgb: tint * intensity
            glm::vec2 BloomTexelSize;
            int BloomMode; // 0 = off, 1 = bilinear, 2 = 9 tap tent
            float _pad1;
        };
        static_assert(sizeof(PostProcessPushConstants) == 112, "PostProcessPushConstants must match the push_constant block in PostProcess.glsl");
        PostProcessPushConstants pc = {};
        pc.ColorThickness = glm::vec4(blackBoard.PostProcessSettings_.OutlineColor, blackBoard.PostProcessSettings_.OutlineThickness);
        pc.ScreenResolution = glm::vec2(static_cast<float>(ctx.Width), static_cast<float>(ctx.Height));
        pc.VignetteIntensity = blackBoard.PostProcessSettings_.VignetteIntensity;
        pc.VignetteSoftness = blackBoard.PostProcessSettings_.VignetteSoftness;
        pc.Grain = blackBoard.PostProcessSettings_.Grain;
        pc.CameraNearFar = blackBoard.CameraNearFarPlane;
        pc.EnableFXAA = blackBoard.PostProcessSettings_.EnableFXAA;

        const glm::mat4& proj = blackBoard.ProjectionMatrix;
        pc.DepthParams = glm::vec4(proj[2][2], proj[3][2], proj[2][3] != 0.0f ? 1.0f : 0.0f, 0.0f);
        const ImageDesc& aoDesc = mRHI->GetDesc(blackBoard.AOImage);
        pc.AOResolution = glm::vec2(static_cast<float>(aoDesc.Width), static_cast<float>(aoDesc.Height));
        pc.AOMode = blackBoard.AOSettings.Enable ? (blackBoard.PostProcessSettings_.ShowAOOnly ? 2 : 1) : 0;

        const BloomSettings& bloom = blackBoard.Bloom;
        const ImageDesc& bloomDesc = mRHI->GetDesc(blackBoard.BloomImage);
        pc.BloomTint = glm::vec4(bloom.Tint * bloom.Intensity, 0.0f);
        pc.BloomTexelSize = 1.0f / glm::vec2(static_cast<float>(bloomDesc.Width), static_cast<float>(bloomDesc.Height));
        pc.BloomMode = (bloom.Enable && bloom.Intensity > 0.0f) ? (bloom.HighQualityComposite ? 2 : 1) : 0;
        mRHI->CmdPushConstants(ctx, mFullscreenPipeline, ShaderType::FRAGMENT | ShaderType::VERTEX, 0, sizeof(PostProcessPushConstants), &pc);
        mRHI->CmdDraw(ctx, 3, 1, 0, 0);
    }

    void PostProcessPass::Resize(Uint width, Uint height, FrameBlackboard& blackBoard)
    {
        Core::AddFrameEndCallback([this, width, height, blackBoard]() { mRHI->ResizeFramebuffer(blackBoard.PostProcessFramebuffer, width, height); }); // (Player)
    }

    void PostProcessPass::OnImGuiRender(FrameBlackboard& blackBoard)
    {
        ImFont* boldFont = ImGui::GetIO().Fonts->Fonts[1];
        ImGui::PushFont(boldFont, 25.0f);
        ImGui::TextUnformatted("PostProcess Pass");
        ImGui::Separator();
        ImGui::PopFont();

        ImGui::Checkbox("Enable FXAA", &blackBoard.PostProcessSettings_.EnableFXAA);
        ImGui::TextUnformatted("Enabling FXAA on mobile is not recommended!");


        ImGui::SliderFloat("Vignette Intensity", &blackBoard.PostProcessSettings_.VignetteIntensity, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Vignette Softness", &blackBoard.PostProcessSettings_.VignetteSoftness, 0.01f, 1.0f, "%.2f");
        ImGui::SliderFloat("Grain Intensity", &blackBoard.PostProcessSettings_.Grain, 0.0f, 0.15f, "%.3f");
    }

    void PostProcessPass::Shutdown(FrameBlackboard& blackBoard)
    {
        if(!blackBoard.PostProcessFramebuffer.IsNull())
        {
            mRHI->DestroyImage(blackBoard.FinalImage);
            mRHI->DestroyFramebuffer(blackBoard.PostProcessFramebuffer);
        }

        mRHI->DestroyDescriptorSet(mPostProcessDescriptorSet);
        mRHI->DestroyPipeline(mFullscreenPipeline);
    }
}
