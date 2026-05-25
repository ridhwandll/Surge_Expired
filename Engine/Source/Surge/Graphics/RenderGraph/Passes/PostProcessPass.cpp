// Copyright (c) - SurgeTechnologies - All rights reserved
#include "PostProcessPass.hpp"
#include "Surge/Core/Core.hpp"

namespace Surge
{
    void PostProcessPass::Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard)
    {
        mRHI = rhi;

        PipelineDesc fsDesc = {};
        fsDesc.Shader_ = Core::GetRenderer()->GetShaderManager().Get("Fullscreen.glsl");
        fsDesc.DebugName = "FullscreenPipeline";
        fsDesc.Raster.Topo = Topology::TRIANGLE_LIST;
        fsDesc.Raster.Polygon = PolygonMode::FILL;
        fsDesc.Raster.Cull = CullMode::NONE;
        
        fsDesc.Blend.Enable = false;
        fsDesc.Depth.TestEnable = false;
        fsDesc.Depth.WriteEnable = false;
        fsDesc.Stencil.Enable = false;

        glm::uvec2 size = Core::GetWindow()->GetSize();

        ImageDesc colorDesc = {};
        colorDesc.Width = size.x;
        colorDesc.Height = size.y;
        colorDesc.Format = ImageFormat::B10G11R11_UFLOAT_PACK32;
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
        mFullScreenSet = mRHI->CreateDescriptorSet(mFullscreenPipeline, DescriptorSetSlot::ZERO, DescriptorUpdateFrequency::DYNAMIC, "SceneInputs");

        mImageReads.push_back(blackBoard.MainPassColorImage);
        mImageReads.push_back(blackBoard.MainPassDepthImage);
        mImageWrites.push_back(blackBoard.FinalImage);
    }

    // Called inside beginSwapchainREnderpass and endSwapchainRenderpass :(( what to do.
    void PostProcessPass::Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard)
    {
        mRHI->CmdBindPipeline(ctx, mFullscreenPipeline);

        std::array<DescriptorWrite, 2> writes;
        writes[0].Binding = 0;
        writes[0].Type = DescriptorType::TEXTURE;
        writes[0].Texture = blackBoard.MainPassColorImage;
        writes[0].Sampler = blackBoard.DefaultSampler;
        writes[1].Binding = 1;
        writes[1].Type = DescriptorType::TEXTURE;
        writes[1].Texture = blackBoard.MainPassDepthImage;
        writes[1].Sampler = blackBoard.DefaultSampler;
        mRHI->UpdateDescriptorSet(mFullScreenSet, writes.data(), writes.size(), ctx.FrameIndex);

        mRHI->CmdBindDescriptorSet(ctx, mFullscreenPipeline, mFullScreenSet, DescriptorSetSlot::ZERO);

        struct FullscreenPushConstants
        {
            glm::vec4 ColorThickness;
            glm::vec2 ScreenResolution;
            glm::vec2 CameraPlanes;
        };
        FullscreenPushConstants pc = {};
        pc.ColorThickness = glm::vec4(1.0f, 0.6f, 0.1f, 3.0f);
        pc.ScreenResolution = glm::vec2(static_cast<float>(ctx.Width), static_cast<float>(ctx.Height));
        pc.CameraPlanes = blackBoard.CameraNearFarPlane;
        mRHI->CmdPushConstants(ctx, mFullscreenPipeline, ShaderType::FRAGMENT | ShaderType::VERTEX, 0, sizeof(FullscreenPushConstants), &pc);
        mRHI->CmdDraw(ctx, 3, 1, 0, 0);
    }

    void PostProcessPass::Resize(Uint width, Uint height, FrameBlackboard& blackBoard)
    {

    }

    void PostProcessPass::OnImGuiRender(FrameBlackboard& blackBoard)
    {

    }

    void PostProcessPass::Shutdown(FrameBlackboard& blackBoard)
    {
        if(!blackBoard.PostProcessFramebuffer.IsNull())
        {
            mRHI->DestroyImage(blackBoard.FinalImage);
            mRHI->DestroyFramebuffer(blackBoard.PostProcessFramebuffer);
        }

        mRHI->DestroyDescriptorSet(mFullScreenSet);
        mRHI->DestroyPipeline(mFullscreenPipeline);
    }
}
