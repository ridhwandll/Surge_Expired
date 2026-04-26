// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RenderProcedure/PreDepthProcedure.hpp"
#include "Surge/Graphics/Interface/Framebuffer.hpp"
#include "Surge/Graphics/Interface/GraphicsPipeline.hpp"

namespace Surge
{
    void PreDepthProcedure::Init(RendererData* rendererData)
    {
        mRendererData = rendererData;

        FramebufferSpecification spec = {};
        spec.AttachmentSpecs = {{ImageFormat::Depth32, {}}};
        spec.Width = 1280;
        spec.Height = 720;
        mProcData.OutputFrambuffer = Framebuffer::Create(spec);

        Ref<Shader> preDepthShader = mRendererData->ShaderSet.GetShader("PreDepth");
        GraphicsPipelineSpecification pipelineSpec {};
        pipelineSpec.Shader = preDepthShader;
        pipelineSpec.Topology = PrimitiveTopology::TriangleList;
        pipelineSpec.CullingMode = CullMode::Back;
        pipelineSpec.UseDepth = true;
        pipelineSpec.UseStencil = false;
        pipelineSpec.DebugName = "DepthPrepass";
        pipelineSpec.LineWidth = 1.0f;
        pipelineSpec.TargetFramebuffer = mProcData.OutputFrambuffer;
        mProcData.PreDepthPipeline = GraphicsPipeline::Create(pipelineSpec);
    }

    void PreDepthProcedure::Update()
    {
        SURGE_PROFILE_FUNC("PreDepthProcedure::Update");

        mProcData.OutputFrambuffer->BeginRenderPass(mRendererData->RenderCmdBuffer);

        mProcData.PreDepthPipeline->Bind(mRendererData->RenderCmdBuffer);
        for (const DrawCommand& drawCmd : mRendererData->DrawList)
        {
            const Ref<Mesh>& mesh = drawCmd.MeshComp->Mesh;

            mesh->GetVertexBuffer()->Bind(mRendererData->RenderCmdBuffer);
            mesh->GetIndexBuffer()->Bind(mRendererData->RenderCmdBuffer);

            const Vector<Submesh>& submeshes = mesh->GetSubmeshes();
            for (const Submesh& submesh : submeshes)
            {
                glm::mat4 meshData[2] = {drawCmd.Transform * submesh.Transform, mRendererData->ViewProjection};
                mProcData.PreDepthPipeline->SetPushConstantData(mRendererData->RenderCmdBuffer, "uMesh", meshData);
                mProcData.PreDepthPipeline->DrawIndexed(mRendererData->RenderCmdBuffer, submesh.IndexCount, submesh.BaseIndex, submesh.BaseVertex);
            }
        }

        mProcData.OutputFrambuffer->EndRenderPass(mRendererData->RenderCmdBuffer);
    }

    void PreDepthProcedure::Shutdown()
    {
        mProcData.OutputFrambuffer.Reset();
        mProcData.PreDepthPipeline.Reset();
    }

    void PreDepthProcedure::Resize(Uint newWidth, Uint newHeight)
    {
        mProcData.OutputFrambuffer->Resize(newWidth, newHeight);
    }

} // namespace Surge

// Empty Reflection, register nothing
SURGE_REFLECT_CLASS_REGISTER_BEGIN(Surge::PreDepthProcedure)
SURGE_REFLECT_CLASS_REGISTER_END(Surge::PreDepthProcedure)