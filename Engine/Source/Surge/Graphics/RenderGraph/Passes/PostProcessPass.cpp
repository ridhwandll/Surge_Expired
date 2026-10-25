// Copyright (c) - SurgeTechnologies - All rights reserved
#include "PostProcessPass.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include <imgui.h>

namespace Surge
{
    void PostProcessPass::Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard)
    {
        mRHI = rhi;
        glm::uvec2 size = mContext.WindowSize;

        PipelineDesc fsDesc = {};
        fsDesc.Shader_ = mContext.Shaders->Get("PostProcess.glsl");
        fsDesc.DebugName = "PostProcess";
        fsDesc.Raster.Topo = Topology::TRIANGLE_LIST;
        fsDesc.Raster.Polygon = PolygonMode::FILL;
        fsDesc.Raster.Cull = CullMode::NONE;
        fsDesc.Blend.Enable = false;
        fsDesc.Depth.TestEnable = false;
        fsDesc.Depth.WriteEnable = false;
        fsDesc.Stencil.Enable = false;

        if(RHISettings::RENDER_TO_SWAPCHAIN)
        {
            // Tonemap + post straight into the swapchain image (the UI is drawn on top in the same render pass)
            // Saves a full-screen RGBA8 write + read and the separate present copy, the biggest bandwidth cost on tile based GPUs
            fsDesc.TargetSwapchain = true;
        }
        else
        {
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
        }

        mFullscreenPipeline = rhi->CreatePipeline(fsDesc);
        mPostProcessDescriptorSet = mRHI->CreateDescriptorSet(mFullscreenPipeline, DescriptorSetSlot::ZERO, DescriptorUpdateFrequency::DYNAMIC, "SceneInputs");

        mImageReads.push_back(blackBoard.MainPassColorImage);
        mImageReads.push_back(blackBoard.MainPassDepthImage);
        mImageReads.push_back(blackBoard.AOImage);
        mImageReads.push_back(blackBoard.AOLinearDepth);
        mImageReads.push_back(blackBoard.BloomImage);
        if(!RHISettings::RENDER_TO_SWAPCHAIN)
            mImageWrites.push_back(blackBoard.FinalImage);
    }

    void PostProcessPass::Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard)
    {
        mRHI->CmdBindPipeline(ctx, mFullscreenPipeline);

        //TODO: remove this from here!! Never do it every frame
        std::array<DescriptorWrite, 5> writes;
        writes[0].Binding = 0;
        writes[0].Type = DescriptorType::TEXTURE;
        writes[0].Texture = blackBoard.MainPassColorImage;
        writes[0].Sampler = blackBoard.LinearClampSampler; // Bilinear upscale when the scene renders below output resolution
        writes[1].Binding = 1;
        writes[1].Type = DescriptorType::TEXTURE;
        writes[1].Texture = blackBoard.MainPassDepthImage;
        writes[1].Sampler = blackBoard.DefaultSampler;
        writes[2].Binding = 2;
        writes[2].Type = DescriptorType::TEXTURE;
        writes[2].Texture = blackBoard.AOImage;
        writes[2].Sampler = blackBoard.PointClampSampler; // Gathered for the bilateral upsample, clamp avoids wrapping at screen edges
        writes[3].Binding = 3;
        writes[3].Type = DescriptorType::TEXTURE;
        writes[3].Texture = blackBoard.AOLinearDepth;
        writes[3].Sampler = blackBoard.PointClampSampler;
        writes[4].Binding = 4;
        writes[4].Type = DescriptorType::TEXTURE;
        writes[4].Texture = blackBoard.BloomImage;
        writes[4].Sampler = blackBoard.LinearClampSampler;
        mRHI->UpdateDescriptorSet(mPostProcessDescriptorSet, writes.data(), writes.size(), ctx.FrameIndex);
        mRHI->CmdBindDescriptorSet(ctx, mFullscreenPipeline, mPostProcessDescriptorSet, DescriptorSetSlot::ZERO);

        // Same order as the push_constant block in PostProcess.glsl: vec4s, then vec2s, then scalars, no padding on either side
        struct PostProcessPushConstants
        {
            glm::vec4 DepthParams; // x: P[2][2], y: P[3][2], z: 1 = perspective / 0 = orthographic
            glm::vec4 BloomTint; // rgb: tint * intensity
            glm::vec2 ScreenResolution;
            glm::vec2 CameraNearFar;
            glm::vec2 AOResolution;
            glm::vec2 BloomTexelSize;
            float VignetteIntensity;
            float VignetteSoftness;
            float Grain;
            int AOMode; // 0 = off, 1 = on, 2 = AO only (debug)
            int BloomMode; // 0 = off, 1 = bilinear, 2 = 9 tap tent
            int SurfaceRotation; // Quarter turns, GraphicsRHI::GetSurfaceRotation() when drawing into the swapchain
        };
        static_assert(sizeof(PostProcessPushConstants) == 88, "PostProcessPushConstants must match the push_constant block in PostProcess.glsl");
        PostProcessPushConstants pc = {};
        // Size of the scene image, it is smaller than the output with a render scale < 1
        const ImageDesc& sceneDesc = mRHI->GetDesc(blackBoard.MainPassColorImage);
        pc.ScreenResolution = glm::vec2(static_cast<float>(sceneDesc.Width), static_cast<float>(sceneDesc.Height));
        pc.VignetteIntensity = blackBoard.PostProcessSettings_.VignetteIntensity;
        pc.VignetteSoftness = blackBoard.PostProcessSettings_.VignetteSoftness;
        pc.Grain = blackBoard.PostProcessSettings_.Grain;
        pc.CameraNearFar = blackBoard.CameraNearFarPlane;

        const glm::mat4& proj = blackBoard.ProjectionMatrix;
        pc.DepthParams = glm::vec4(proj[2][2], proj[3][2], proj[2][3] != 0.0f ? 1.0f : 0.0f, 0.0f);
        const ImageDesc& aoDesc = mRHI->GetDesc(blackBoard.AOImage);
        pc.AOResolution = glm::vec2(static_cast<float>(aoDesc.Width), static_cast<float>(aoDesc.Height));
        pc.AOMode = blackBoard.RunsAO() ? (blackBoard.PostProcessSettings_.ShowAOOnly ? 2 : 1) : 0;
        pc.SurfaceRotation = RHISettings::RENDER_TO_SWAPCHAIN ? static_cast<int>(mRHI->GetSurfaceRotation()) : 0;

        const BloomSettings& bloom = blackBoard.Bloom;
        const ImageDesc& bloomDesc = mRHI->GetDesc(blackBoard.BloomImage);
        pc.BloomTint = glm::vec4(bloom.Tint * bloom.Intensity, 0.0f);
        pc.BloomTexelSize = 1.0f / glm::vec2(static_cast<float>(bloomDesc.Width), static_cast<float>(bloomDesc.Height));
        pc.BloomMode = blackBoard.RunsBloom() ? (bloom.HighQualityComposite ? 2 : 1) : 0;
        mRHI->CmdPushConstants(ctx, mFullscreenPipeline, ShaderType::FRAGMENT | ShaderType::VERTEX, 0, sizeof(PostProcessPushConstants), &pc);
        mRHI->CmdDraw(ctx, 3, 1, 0, 0);
    }

    void PostProcessPass::Resize(Uint width, Uint height, FrameBlackboard& blackBoard)
    {
        if(RHISettings::RENDER_TO_SWAPCHAIN)
            return; // The swapchain is resized by the RHI

        const FramebufferHandle framebuffer = blackBoard.PostProcessFramebuffer;
        mContext.DeferToFrameEnd([this, width, height, framebuffer]() { mRHI->ResizeFramebuffer(framebuffer, width, height); });
    }

    void PostProcessPass::RenderStatsViaImGui(FrameBlackboard&) {}

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
