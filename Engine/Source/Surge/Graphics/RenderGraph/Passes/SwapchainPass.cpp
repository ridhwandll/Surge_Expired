// Copyright (c) - SurgeTechnologies - All rights reserved
#include "SwapchainPass.hpp"
#include "Surge/Core/Core.hpp"

namespace Surge
{
    void SwapchainPass::Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard)
    {
        // We need this here(before the early exit below) because it will transition the blackBoard.FinalImage
        // to SAMPLED thus ImGui::Image can read it.
        mImageReads.push_back(blackBoard.FinalImage);

        if(!RHISettings::RENDER_TO_SWAPCHAIN)
            return;

        mRHI = rhi;

        PipelineDesc fsDesc = {};
        fsDesc.Shader_ = Core::GetRenderer()->GetShaderManager().Get("Present.glsl");
        fsDesc.DebugName = "PresentPipeline";
        fsDesc.Raster.Topo = Topology::TRIANGLE_LIST;
        fsDesc.Raster.Polygon = PolygonMode::FILL;
        fsDesc.Raster.Cull = CullMode::NONE;
        fsDesc.Blend.Enable = false;
        fsDesc.Depth.TestEnable = false;
        fsDesc.Depth.WriteEnable = false;
        fsDesc.Stencil.Enable = false;
        fsDesc.TargetSwapchain = true;
        mPresentPipeline = mRHI->CreatePipeline(fsDesc);

        mPresentSet = mRHI->CreateDescriptorSet(mPresentPipeline, DescriptorSetSlot::ZERO, DescriptorUpdateFrequency::DYNAMIC, "Present Set");
    }

    void SwapchainPass::Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard)
    {
        if(!RHISettings::RENDER_TO_SWAPCHAIN)
            return;

        mRHI->CmdBindPipeline(ctx, mPresentPipeline);

        DescriptorWrite write;
        write.Binding = 0;
        write.Type = DescriptorType::TEXTURE;
        write.Texture = blackBoard.FinalImage;
        write.Sampler = blackBoard.DefaultSampler;
        mRHI->UpdateDescriptorSet(mPresentSet, &write, 1, ctx.FrameIndex);

        mRHI->CmdBindDescriptorSet(ctx, mPresentPipeline, mPresentSet, DescriptorSetSlot::ZERO);
        mRHI->CmdDraw(ctx, 3, 1, 0, 0);
    }

    void SwapchainPass::Resize(Uint width, Uint height, FrameBlackboard& blackBoard)
    {
    }

    void SwapchainPass::OnImGuiRender(FrameBlackboard& blackBoard)
    {

    }

    void SwapchainPass::Shutdown(FrameBlackboard& blackBoard)
    {
        if(!RHISettings::RENDER_TO_SWAPCHAIN)
            return;

        mRHI->DestroyPipeline(mPresentPipeline);
        mRHI->DestroyDescriptorSet(mPresentSet);
    }
}
