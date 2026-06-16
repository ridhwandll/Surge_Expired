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
        mImageWrites.push_back(blackBoard.FinalImage);
    }

    void PostProcessPass::Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard)
    {
        mRHI->CmdBindPipeline(ctx, mFullscreenPipeline);

        //TODO: remove this from here!! Never do it every frame
        std::array<DescriptorWrite, 3> writes;
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
        mRHI->UpdateDescriptorSet(mPostProcessDescriptorSet, writes.data(), writes.size(), ctx.FrameIndex);
        mRHI->CmdBindDescriptorSet(ctx, mFullscreenPipeline, mPostProcessDescriptorSet, DescriptorSetSlot::ZERO);

        struct PostProcessPushConstants
        {
            glm::vec4 ColorThickness;
            glm::vec2 ScreenResolution;
            VignetteGrainConfig VignetteGrain;
            glm::vec2 CameraNearFar;
            int EnableFXAA;
        };
        PostProcessPushConstants pc = {};
        pc.ColorThickness = glm::vec4(blackBoard.OutlineColor,  blackBoard.OutlineThickness);
        pc.ScreenResolution = glm::vec2(static_cast<float>(ctx.Width), static_cast<float>(ctx.Height));
        pc.VignetteGrain = blackBoard.VignetteGrain;
        pc.CameraNearFar = blackBoard.CameraNearFarPlane;
        pc.EnableFXAA = blackBoard.EnableFXAA;
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

        ImGui::Checkbox("Enable FXAA", &blackBoard.EnableFXAA);
        ImGui::TextUnformatted("Enabling FXAA on mobile is not recommended!");

        ImGui::SliderFloat("Vignette Intensity", &blackBoard.VignetteGrain.Intensity, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Vignette Softness", &blackBoard.VignetteGrain.Softness, 0.01f, 1.0f, "%.2f");
        ImGui::SliderFloat("Grain Intensity", &blackBoard.VignetteGrain.Grain, 0.0f, 0.15f, "%.3f");
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
