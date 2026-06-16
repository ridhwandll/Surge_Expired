// Copyright (c) - SurgeTechnologies - All rights reserved
#include "OutlinePass.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"

namespace Surge
{
    void OutlinePass::Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard)
    {
        mRHI = rhi;

        glm::uvec2 size = Core::GetWindow()->GetSize();

        ImageDesc colorDesc = {};
        colorDesc.Width = size.x;
        colorDesc.Height = size.y;
        colorDesc.Format = ImageFormat::R8_UNORM;
        colorDesc.Usage = ImageUsage::COLOR_ATTACHMENT | ImageUsage::SAMPLED;
        colorDesc.DebugName = "OutlineMask";
        colorDesc.Sampler = blackBoard.DefaultSampler;
        colorDesc.GenerateImGuiID = true;
        blackBoard.OutlineMask = mRHI->CreateImage(colorDesc);

        FramebufferAttachment colorAttachment = {};
        colorAttachment.Handle = blackBoard.OutlineMask;
        colorAttachment.Load = LoadOp::CLEAR;
        colorAttachment.Store = StoreOp::STORE;

        FramebufferDesc fbDesc = {};
        fbDesc.ColorAttachments[0] = colorAttachment;
        fbDesc.ColorAttachmentCount = 1;
        fbDesc.HasDepth = false;
        fbDesc.Width = size.x;
        fbDesc.Height = size.y;
        fbDesc.DebugName = "OutlineFramebuffer";
        blackBoard.OutlineFramebuffer = mRHI->CreateFramebuffer(fbDesc);

        PipelineDesc pd = {};
        pd.Shader_ = Core::GetRenderer()->GetShaderManager().Get("OutlineMask.glsl");
        pd.DebugName = "Outline";
        pd.Raster.Topo = Topology::TRIANGLE_LIST;
        pd.Raster.Polygon = PolygonMode::FILL;
        pd.Raster.Cull = CullMode::NONE;
        pd.Blend.Enable = false;
        pd.Depth.TestEnable = false;
        pd.Depth.WriteEnable = false;
        pd.Stencil.Enable = false;
        pd.TargetSwapchain = false;
        pd.TargetFramebuffer = blackBoard.OutlineFramebuffer;
        mOutlineMaskPipeline = rhi->CreatePipeline(pd);

        mImageWrites.push_back(blackBoard.OutlineMask);
    }

    void OutlinePass::Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard)
    {
        mRHI->CmdBindPipeline(ctx, mOutlineMaskPipeline);

        for(const OutlineSubmitCmd& cmd : blackBoard.OutlineList)
        {
            const Mesh& mesh = *cmd.Mesh_;
            mRHI->CmdBindVertexBuffer(ctx, mesh.GetVertexBuffer());
            mRHI->CmdBindIndexBuffer(ctx, mesh.GetIndexBuffer());
            const Vector<Submesh>& submeshes = mesh.GetSubmeshes();

            // Dont draw outline if there are too many submeshes
            if (submeshes.size() > 669)
                continue;

            for(const Submesh& submesh : submeshes)
            {
                glm::mat4 modelViewProjection;
                modelViewProjection = blackBoard.ViewProjection * cmd.Transform * submesh.Transform;
                mRHI->CmdPushConstants(ctx, mOutlineMaskPipeline, ShaderType::VERTEX | ShaderType::FRAGMENT, 0, sizeof(glm::mat4), &modelViewProjection);
                mRHI->CmdDrawIndexed(ctx, submesh.IndexCount, 1, submesh.BaseIndex, submesh.BaseVertex, 0);
            }
        }
    }

    void OutlinePass::Resize(Uint width, Uint height, FrameBlackboard& blackBoard)
    {
        Core::AddFrameEndCallback([this, width, height, blackBoard]() { mRHI->ResizeFramebuffer(blackBoard.OutlineFramebuffer, width, height); }); // (Player)
    }

    void OutlinePass::OnImGuiRender(FrameBlackboard&) {}

    void OutlinePass::Shutdown(FrameBlackboard& blackBoard)
    {
        mRHI->DestroyImage(blackBoard.OutlineMask);
        mRHI->DestroyFramebuffer(blackBoard.OutlineFramebuffer);
        mRHI->DestroyPipeline(mOutlineMaskPipeline);
    }
}
