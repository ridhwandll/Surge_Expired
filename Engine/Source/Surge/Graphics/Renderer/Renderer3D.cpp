// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Renderer/Renderer3D.hpp"
#include "Surge/Graphics/Camera/EditorCamera.hpp"
#include "Surge/Core/Core.hpp"

namespace Surge
{
    inline static Light LightComponentToGPULight(const LightComponent& light, const glm::vec3& position, const glm::vec3& rotation)
    {
        Light gpuLight{};
        gpuLight.PositionType = (light.Type == LightType::DIRECTIONAL) ? glm::vec4(rotation, 0.0f) : glm::vec4(position, 1.0f); // Directional light has w = 0, Point light has w = 1
        gpuLight.ColorRGBIntensityA = glm::vec4(light.Color, light.Intensity);
        gpuLight.Radius = light.Radius;
        gpuLight.Falloff = light.Falloff;
        return gpuLight;
    }

    void Renderer3D::Initialize(GraphicsRHI* rhi, RendererData* data)
    {
        SURGE_PROFILE_FUNC("Renderer3D::Initialize()");
        mRHI = rhi;
        mData = data;

        Shader shader;
        shader.Load("Renderer3D.glsl", ShaderType::VERTEX | ShaderType::FRAGMENT);

        DepthDesc depth;
        depth.TestEnable = true;
        depth.WriteEnable = true;
        depth.Op = CompareOp::LESS;

        PipelineDesc desc = {};
        desc.Shader_ = shader;
        desc.Raster.Topo = Topology::TRIANGLE_LIST;
        desc.Raster.Polygon = PolygonMode::FILL;
        desc.Raster.Cull = CullMode::BACK;
        desc.Blend.Enable = false;
        desc.Depth = depth;
        desc.DebugName = "Renderer3D Pipeline";
        desc.TargetFramebuffer = mData->mOffscreenFramebuffer;
        desc.TargetSwapchain = false;
        m3DPipeline = mRHI->CreatePipeline(desc);


        BufferDesc frameUBODesc = {};
        frameUBODesc.Usage = BufferUsage::UNIFORM;
        frameUBODesc.HostVisible = true;
        frameUBODesc.DebugName = "Renderer3D_FrameUBO";
        frameUBODesc.Size = sizeof(FrameUBO);
        m3DData.FrameUBO = mRHI->CreateBuffer(frameUBODesc);
        BufferDesc gpuLightDesc = {};

        gpuLightDesc.Usage = BufferUsage::STORAGE;
        gpuLightDesc.HostVisible = true;
        gpuLightDesc.DebugName = "Renderer3D_Lights";
        gpuLightDesc.Size = sizeof(LightUBOData) * MAX_LIGHTS;
        m3DData.LightUBO = mRHI->CreateBuffer(gpuLightDesc);
        m3DData.FrameDescriptorSet = mRHI->CreateDescriptorSet(m3DPipeline, 0, DescriptorUpdateFrequency::DYNAMIC, "3D_FrameData [Set0]");
    }

    void Renderer3D::BeginFrame(const FrameContext& frameCtx, Uint submitCount)
    {
        SURGE_PROFILE_FUNC("Renderer3D::BeginFrame(FrameContext)");
        mCurrentFrameCtx = frameCtx;
        mLightCPU.clear();

        mMeshDrawCommands.reserve(submitCount);
    }

    void Renderer3D::EndFrame()
    {
        SURGE_PROFILE_FUNC("Renderer3D::EndFrame()");

        FrameUBO frameData = {};
        frameData.ViewProjection = mData->ViewProjection;
        frameData.CameraPos = mData->CameraPosition;
        mRHI->UploadBuffer(m3DData.FrameUBO, &frameData, sizeof(FrameUBO));
        if(!mLightCPU.empty())
        {
            LightUBOData lightData = {};
            for(Uint i = 0; i < mLightCPU.size(); i++)
                lightData.Lights[i] = mLightCPU[i];

            mRHI->UploadBuffer(m3DData.LightUBO, &lightData, sizeof(LightUBOData), 0);
        }

        DescriptorWrite frameDescriptorWrite[2] = {};
        frameDescriptorWrite[0].Binding = 0;
        frameDescriptorWrite[0].Type = DescriptorType::UNIFORM_BUFFER;
        frameDescriptorWrite[0].Buffer = m3DData.FrameUBO;

        frameDescriptorWrite[1].Binding = 1;
        frameDescriptorWrite[1].Type = DescriptorType::STORAGE_BUFFER;
        frameDescriptorWrite[1].Buffer = m3DData.LightUBO;
        frameDescriptorWrite[1].BufferRange = sizeof(LightUBOData);
        mRHI->UpdateDescriptorSet(m3DData.FrameDescriptorSet, frameDescriptorWrite, 2);

        mRHI->CmdBindPipeline(mCurrentFrameCtx, m3DPipeline);
        mRHI->CmdBindDescriptorSet(mCurrentFrameCtx, m3DPipeline, m3DData.FrameDescriptorSet, 0);

        for (auto& cmd : mMeshDrawCommands)
        {
            const Mesh& mesh = *cmd.Mesh;
            mRHI->CmdBindVertexBuffer(mCurrentFrameCtx, mesh.GetVertexBuffer());
            mRHI->CmdBindIndexBuffer(mCurrentFrameCtx, mesh.GetIndexBuffer());

            const Submesh* submeshes = mesh.GetSubmeshes().data();
            for(Uint i = 0; i < mesh.GetSubmeshes().size(); i++)
            {
                const Submesh& submesh = submeshes[i];

                PushConstantData pushConstants = {};
                pushConstants.Transform = cmd.Transform * submesh.Transform,
                pushConstants.LightCount = (Uint)mLightCPU.size(),

                cmd.Material->Apply();
                mRHI->CmdPushConstants(mCurrentFrameCtx, m3DPipeline, ShaderType::VERTEX | ShaderType::FRAGMENT, 0, sizeof(PushConstantData), &pushConstants);
                mRHI->CmdDrawIndexed(mCurrentFrameCtx, submesh.IndexCount, 1, submesh.BaseIndex, submesh.BaseVertex, 0);
            }
        }
        mMeshDrawCommands.clear();
    }

    void Renderer3D::SubmitMesh(const glm::mat4& transform, const Ref<Mesh>& mesh, const Ref<Material>& material)
    {
        mMeshDrawCommands.emplace_back(MeshDrawCmd{ transform, mesh, material });
    }

    void Renderer3D::SubmitLight(const LightComponent& light, const glm::vec3& position, const glm::vec3& rotation)
    {
        SG_ASSERT(mLightCPU.size() < MAX_LIGHTS, "Renderer3D: exceeded MAX_LIGHTS");
        mLightCPU.push_back(LightComponentToGPULight(light, position, rotation));
    }

    void Renderer3D::OnImGuiRender()
    {

    }

    void Renderer3D::OnWindowResize(Uint width, Uint height)
    {
        Log<Severity::Debug>("Renderer3D::OnWindowResize: Latest dimensions: Width:{0} Height:{1}", width, height);
    }
    
    void Renderer3D::Shutdown()
    {
        SURGE_PROFILE_FUNC("Renderer3D::Shutdown()");
        mRHI->DestroyPipeline(m3DPipeline);
        mRHI->DestroyBuffer(m3DData.FrameUBO);
        mRHI->DestroyBuffer(m3DData.LightUBO);
        mRHI->DestroyDescriptorSet(m3DData.FrameDescriptorSet);
    }

} // namespace Surge
