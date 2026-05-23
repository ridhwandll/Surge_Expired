// Copyright (c) - SurgeTechnologies - All rights reserved
#include "GeometryPass.hpp"
#include "Surge/Core/Core.hpp"

namespace Surge
{
    void GeometryPass::Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard)
    {
        mRHI = rhi;

        DepthDesc depth;
        depth.TestEnable = true;
        depth.WriteEnable = true;
        depth.Op = CompareOp::LESS;

        PipelineDesc desc = {};
        desc.Shader_ = Core::GetRenderer()->GetShaderManager().Get("Renderer3D.glsl");
        desc.Raster.Topo = Topology::TRIANGLE_LIST;
        desc.Raster.Polygon = PolygonMode::FILL;
        desc.Raster.Cull = CullMode::BACK;
        desc.Blend.Enable = false;
        desc.Depth = depth;
        desc.DebugName = "Renderer3D Pipeline";
        desc.TargetFramebuffer = blackBoard.OffscreenFramebuffer;
        desc.TargetSwapchain = false;
        m3DPipeline = mRHI->CreatePipeline(desc);

        BufferDesc gpuLightDesc = {};
        gpuLightDesc.Usage = BufferUsage::STORAGE;
        gpuLightDesc.HostVisible = true;
        gpuLightDesc.DebugName = "Renderer3D_Lights";
        gpuLightDesc.Size = sizeof(LightUBOData) * MAX_LIGHTS;

        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
            mLightUBOs[i] = mRHI->CreateBuffer(gpuLightDesc);

        mFrameDescriptorSet = mRHI->CreateDescriptorSet(m3DPipeline, DescriptorSetSlot::ZERO, DescriptorUpdateFrequency::DYNAMIC, "3D_FrameData [Set0]");
        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
        {
            DescriptorWrite frameDescriptorWrite[2] = {};
            frameDescriptorWrite[0].Binding = 0;
            frameDescriptorWrite[0].Type = DescriptorType::UNIFORM_BUFFER;
            frameDescriptorWrite[0].Buffer = blackBoard.FrameUBOs[i];

            frameDescriptorWrite[1].Binding = 1;
            frameDescriptorWrite[1].Type = DescriptorType::STORAGE_BUFFER;
            frameDescriptorWrite[1].Buffer = mLightUBOs[i];
            frameDescriptorWrite[1].BufferRange = sizeof(LightUBOData);
            mRHI->UpdateDescriptorSet(mFrameDescriptorSet, frameDescriptorWrite, 2, i);
        }

        // TODO: Remove
        blackBoard.MaterialPipeline = m3DPipeline;
    }

    void GeometryPass::Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard)
    {
        mCurrentFrameCtx = ctx;

        // FRAME UBO is uploaded by Renderer, so we only need to upload light data here
        // TODO: We could optimize this by only uploading when lights have changed, but for simplicity we upload every frame for now
        if(!blackBoard.LightList.empty())
        {
            LightUBOData lightData = {};
            for(Uint i = 0; i < blackBoard.LightList.size(); i++)
                lightData.Lights[i] = blackBoard.LightList[i].GPULight;

            mRHI->UploadBuffer(mLightUBOs[mCurrentFrameCtx.FrameIndex], &lightData, sizeof(LightUBOData), 0);
        }

        mRHI->CmdBindPipeline(mCurrentFrameCtx, m3DPipeline);
        mRHI->CmdBindDescriptorSet(mCurrentFrameCtx, m3DPipeline, mFrameDescriptorSet, DescriptorSetSlot::ZERO);

        for(const MeshSubmitCmd& cmd : blackBoard.MeshList)
        {
            const Mesh& mesh = *cmd.Mesh_;
            mRHI->CmdBindVertexBuffer(mCurrentFrameCtx, mesh.GetVertexBuffer());
            mRHI->CmdBindIndexBuffer(mCurrentFrameCtx, mesh.GetIndexBuffer());
            const Vector<Ref<Material>>& materials = mesh.GetMaterials();

            const Submesh* submeshes = mesh.GetSubmeshes().data();
            for(Uint i = 0; i < mesh.GetSubmeshes().size(); i++)
            {
                const Submesh& submesh = submeshes[i];

                PushConstantData pushConstants = {};
                pushConstants.Transform = cmd.Transform * submesh.Transform;
                pushConstants.LightCount = (Uint)blackBoard.LightList.size();

                materials[submesh.MaterialIndex]->UpdateForRendering(mCurrentFrameCtx);
                materials[submesh.MaterialIndex]->Bind(mCurrentFrameCtx, m3DPipeline);

                mRHI->CmdPushConstants(mCurrentFrameCtx, m3DPipeline, ShaderType::VERTEX | ShaderType::FRAGMENT, 0, sizeof(PushConstantData), &pushConstants);
                mRHI->CmdDrawIndexed(mCurrentFrameCtx, submesh.IndexCount, 1, submesh.BaseIndex, submesh.BaseVertex, 0);
            }
        }
    }

    void GeometryPass::Resize(Uint width, Uint height, FrameBlackboard& blackBoard)
    {
        Log<Severity::Debug>("GeometryPass::OnWindowResize: Latest dimensions: Width:{0} Height:{1}", width, height);
    }

    void GeometryPass::OnImGuiRender(FrameBlackboard& blackBoard)
    {

    }

    void GeometryPass::Shutdown()
    {
        mRHI->DestroyPipeline(m3DPipeline);

        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
            mRHI->DestroyBuffer(mLightUBOs[i]);

        mRHI->DestroyDescriptorSet(mFrameDescriptorSet);

    }
}
