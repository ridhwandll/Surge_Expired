// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Graphics/RenderProcedure/GeometryProcedure.hpp"
#include "ShadowMapProcedure.hpp"
#include "LightCullingProcedure.hpp"
#include "Surge/Core/Core.hpp"

namespace Surge
{
    void GeometryProcedure::Init(RendererData* rendererData)
    {
        mProcData = {};
        mRendererData = rendererData;

        FramebufferSpecification spec = {};
        spec.AttachmentSpecs = {{{ImageFormat::RGBA8, {}}, {ImageFormat::Depth32, {}}}};
        spec.Width = 1280;
        spec.Height = 720;
        spec.ClearColor = {1, 0, 0, 1};
        mProcData.OutputFrambuffer = Framebuffer::Create(spec);

        Ref<Shader> mainPBRShader = mRendererData->ShaderSet.GetShader("PBR");
        GraphicsPipelineSpecification pipelineSpec {};
        pipelineSpec.Shader = mainPBRShader;
        pipelineSpec.Topology = PrimitiveTopology::TriangleList;
        pipelineSpec.CullingMode = CullMode::Back;
        pipelineSpec.UseDepth = true;
        pipelineSpec.UseStencil = false;
        pipelineSpec.DebugName = "MeshPipeline";
        pipelineSpec.LineWidth = 1.0f;
        pipelineSpec.TargetFramebuffer = mProcData.OutputFrambuffer;
        mProcData.GeometryPipeline = GraphicsPipeline::Create(pipelineSpec);
    }

    void GeometryProcedure::Update()
    {
        SURGE_PROFILE_FUNC("GeometryProcedure::Update");

        ShadowMapProcedure::InternalData* shadowProcData = Core::GetRenderer()->GetRenderProcManager()->GetRenderProcData<ShadowMapProcedure>();
        mProcData.GeometryPipeline->Bind(mRendererData->RenderCmdBuffer);

        shadowProcData->ShadowDesciptorSet->SetBuffer(shadowProcData->ShadowUniformBuffer, 0);
        shadowProcData->ShadowDesciptorSet->UpdateForRendering();
        shadowProcData->ShadowDesciptorSet->Bind(mRendererData->RenderCmdBuffer, mProcData.GeometryPipeline);
        mRendererData->DescriptorSet0->Bind(mRendererData->RenderCmdBuffer, mProcData.GeometryPipeline);

        mProcData.OutputFrambuffer->BeginRenderPass(mRendererData->RenderCmdBuffer);
        for (const DrawCommand& object : mRendererData->DrawList)
        {
            const Ref<Mesh>& mesh = object.MeshComp->Mesh;

            mesh->GetVertexBuffer()->Bind(mRendererData->RenderCmdBuffer);
            mesh->GetIndexBuffer()->Bind(mRendererData->RenderCmdBuffer);

            Vector<Ref<Material>>& materials = object.MeshComp->Mesh->GetMaterials();

            for (auto& mat : materials)
                mat->UpdateForRendering();

            const Submesh* submeshes = mesh->GetSubmeshes().data();
            for (Uint i = 0; i < mesh->GetSubmeshes().size(); i++)
            {
                const Submesh& submesh = submeshes[i];
                glm::mat4 meshData[2] = {object.Transform * submesh.Transform, mRendererData->ViewProjection};
                materials[submesh.MaterialIndex]->Bind(mRendererData->RenderCmdBuffer, mProcData.GeometryPipeline);

                mProcData.GeometryPipeline->SetPushConstantData(mRendererData->RenderCmdBuffer, "uMesh", meshData);
                mProcData.GeometryPipeline->DrawIndexed(mRendererData->RenderCmdBuffer, submesh.IndexCount, submesh.BaseIndex, submesh.BaseVertex);
            }
        }
        mProcData.OutputFrambuffer->EndRenderPass(mRendererData->RenderCmdBuffer);
    }

    void GeometryProcedure::Shutdown()
    {
        mProcData.OutputFrambuffer.Reset();
        mProcData.GeometryPipeline.Reset();
    }

    void GeometryProcedure::Resize(Uint newWidth, Uint newHeight)
    {
        mProcData.OutputFrambuffer->Resize(newWidth, newHeight);
    }

} // namespace Surge

// Empty Reflection, register nothing
SURGE_REFLECT_CLASS_REGISTER_BEGIN(Surge::GeometryProcedure)
SURGE_REFLECT_CLASS_REGISTER_END(Surge::GeometryProcedure)