// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Core/Core.hpp"
#include "Surge/Graphics/RenderGraph/Passes/OutlinePass.hpp"

namespace Surge
{

    void OutlinePass::Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard)
    {
        mRHI = rhi;

        PipelineDesc desc = {};
        desc.Shader_ = Core::GetRenderer()->GetShaderManager().Get("OutlineStencilWrite.glsl");
        desc.Raster.Topo = Topology::TRIANGLE_LIST;
        desc.Raster.Polygon = PolygonMode::FILL;
        desc.Raster.Cull = CullMode::NONE;
        desc.Blend.Enable = false;
        desc.Depth.TestEnable = false;
        desc.DebugName = "StencilWrite";
        desc.TargetFramebuffer = blackBoard.OffscreenFramebuffer;
        desc.TargetSwapchain = false;

        desc.Stencil.Enable = true;
        desc.Stencil.Front.Fail = StencilOp::KEEP;
        desc.Stencil.Front.DepthFail = StencilOp::KEEP;
        desc.Stencil.Front.Pass = StencilOp::REPLACE;       // write 1 on every visible pixel
        desc.Stencil.Front.CompareOp_ = CompareOp::ALWAYS;  // always pass stencil test
        desc.Stencil.Front.Reference = 1;
        desc.Stencil.Front.WriteMask = 0xFF;
        desc.Stencil.Front.CompareMask = 0xFF;

        desc.Stencil.Back.Fail = StencilOp::KEEP;
        desc.Stencil.Back.DepthFail = StencilOp::KEEP;
        desc.Stencil.Back.Pass = StencilOp::KEEP;
        desc.Stencil.Back.CompareOp_ = CompareOp::NEVER;
        desc.Stencil.Back.Reference = 1;
        desc.Stencil.Back.WriteMask = 0xFF;
        desc.Stencil.Back.CompareMask = 0xFF;
        mStencilWritePipeline = rhi->CreatePipeline(desc);

        // Outline Draw
        // Renders selected objects slightly enlarged
        // Only draws where stencil = 0 (i.e. outside the original silhouette)
        PipelineDesc outlineDrawDesc = {};
        outlineDrawDesc.Shader_ = Core::GetRenderer()->GetShaderManager().Get("Outline.glsl");
        outlineDrawDesc.DebugName = "Outline";
        outlineDrawDesc.TargetFramebuffer = blackBoard.OffscreenFramebuffer;
        outlineDrawDesc.Raster.Cull = CullMode::FRONT; // Cull front faces to see back of scaled mesh
        outlineDrawDesc.Blend.Enable = false;
        outlineDrawDesc.Depth.TestEnable = true;
        outlineDrawDesc.Depth.WriteEnable = false;
        outlineDrawDesc.Depth.Op = CompareOp::LESS_OR_EQUAL;

        outlineDrawDesc.Stencil.Enable = true;
        outlineDrawDesc.Stencil.Back.Pass = StencilOp::KEEP;
        outlineDrawDesc.Stencil.Back.Fail = StencilOp::KEEP;
        outlineDrawDesc.Stencil.Back.DepthFail = StencilOp::KEEP;

        // (Rid) Outline understanding
        // It looks at a pixel. It asks: "Is this pixel already covered by the main object?" (Is stencil == 1?)
        // Stencils are always set to 1 in the Geometry Pass pipeline
        // If Yes, it throws the pixel away. If No, it lets the fragment shader color it (creating the border edge of outline)
        outlineDrawDesc.Stencil.Back.CompareOp_ = CompareOp::NOT_EQUAL;

        outlineDrawDesc.Stencil.Back.Reference = 1;
        outlineDrawDesc.Stencil.Back.CompareMask = 0xFF; // Compare everything

        // Also ensure the stencil buffer is protected (WriteMask = 0x00) so nothing can write it
        outlineDrawDesc.Stencil.Back.WriteMask = 0x00;

        outlineDrawDesc.Stencil.Front = outlineDrawDesc.Stencil.Back;
        mOutlineDrawPipeline = rhi->CreatePipeline(outlineDrawDesc);

        mImageWrites.push_back(blackBoard.FinalImage);
    }

    void OutlinePass::Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard)
    {
        //Log<Severity::Debug>("OutlinePass::Execute OutlineList size: {}", blackBoard.OutlineList.size());

        if(blackBoard.OutlineList.empty())
            return;

        // (Rid)TODO: Stencil write pipeline
        mRHI->CmdBindPipeline(ctx, mStencilWritePipeline);
        for(const OutlineSubmitCmd& cmd : blackBoard.OutlineList)
        {
            const Mesh& mesh = *cmd.Mesh_;
            mRHI->CmdBindVertexBuffer(ctx, mesh.GetVertexBuffer());
            mRHI->CmdBindIndexBuffer(ctx, mesh.GetIndexBuffer());

            for(const Submesh& submesh : mesh.GetSubmeshes())
            {
                glm::mat4 modelViewProjection;
                modelViewProjection = blackBoard.ViewProjection * cmd.Transform * submesh.Transform;
                mRHI->CmdPushConstants(ctx, mOutlineDrawPipeline, ShaderType::VERTEX | ShaderType::FRAGMENT, 0, sizeof(glm::mat4), &modelViewProjection);
                mRHI->CmdDrawIndexed(ctx, submesh.IndexCount, 1, submesh.BaseIndex, submesh.BaseVertex, 0);
            }
        }

        // Render each selected mesh scaled up, draw outline color only where stencil = 0
        mRHI->CmdBindPipeline(ctx, mOutlineDrawPipeline);
        for(const OutlineSubmitCmd& cmd : blackBoard.OutlineList)
        {
            const Mesh& mesh = *cmd.Mesh_;
            mRHI->CmdBindVertexBuffer(ctx, mesh.GetVertexBuffer());
            mRHI->CmdBindIndexBuffer(ctx, mesh.GetIndexBuffer());

            for(const Submesh& submesh : mesh.GetSubmeshes())
            {
                struct OutlinePushConstants
                {
                    glm::mat4 ModelViewProjection;
                    glm::vec4 ColorThickness;
                };

                OutlinePushConstants pc = {};
                pc.ModelViewProjection = blackBoard.ViewProjection * cmd.Transform * submesh.Transform;
                pc.ColorThickness = glm::vec4(cmd.Color, cmd.Thickness);
                mRHI->CmdPushConstants(ctx, mOutlineDrawPipeline, ShaderType::VERTEX | ShaderType::FRAGMENT, 0, sizeof(OutlinePushConstants), &pc);
                mRHI->CmdDrawIndexed(ctx, submesh.IndexCount, 1, submesh.BaseIndex, submesh.BaseVertex, 0);
            }
        }
    }

    void OutlinePass::Resize(Uint width, Uint height, FrameBlackboard& blackBoard)
    {

    }

    void OutlinePass::OnImGuiRender(FrameBlackboard& blackBoard)
    {

    }

    void OutlinePass::Shutdown()
    {
        mRHI->DestroyPipeline(mOutlineDrawPipeline);
        mRHI->DestroyPipeline(mStencilWritePipeline);
    }

}
