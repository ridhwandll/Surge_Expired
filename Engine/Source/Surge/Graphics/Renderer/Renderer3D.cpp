// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Renderer/Renderer3D.hpp"
#include "Surge/Graphics/Camera/EditorCamera.hpp"
#include "Surge/Core/Core.hpp"

namespace Surge
{
    inline static GPULight LightComponentToGPULight(const LightComponent& light, const glm::vec3& position, const glm::vec3& rotation)
    {
        GPULight gpuLight{};
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

        // GPU Light
        BufferDesc gpuLightDesc = {};
        gpuLightDesc.Usage = BufferUsage::STORAGE; // Registers it to Bindless
        gpuLightDesc.HostVisible = true;
        gpuLightDesc.DebugName = "GPU Lights";
        gpuLightDesc.Size = sizeof(GPULight) * MAX_LIGHTS;
        mGPULightBuffer = mRHI->CreateBuffer(gpuLightDesc);
        mLightBufferIndex = mRHI->GetBindlessBufferIndex(mGPULightBuffer);
    }

    void Renderer3D::BeginFrame(const FrameContext& frameCtx)
    {
        SURGE_PROFILE_FUNC("Renderer3D::BeginFrame(FrameContext)");	
        mCurrentFrameCtx = frameCtx;
        mLightCPU.Count = 0;
    }

    void Renderer3D::EndFrame()
    {
        SURGE_PROFILE_FUNC("Renderer3D::EndFrame()");

        mRHI->BindBindlessSet(mCurrentFrameCtx, m3DPipeline);

        if (mLightCPU.Count > 0)
            mRHI->UploadBuffer(mGPULightBuffer, mLightCPU.Lights, sizeof(GPULight) * mLightCPU.Count, 0);

        mRHI->CmdBindPipeline(mCurrentFrameCtx, m3DPipeline);

        for (auto& cmd : mMeshDrawCommands)
        {
            const Mesh& mesh = *cmd.Mesh;
            mRHI->CmdBindVertexBuffer(mCurrentFrameCtx, mesh.GetVertexBuffer());
            mRHI->CmdBindIndexBuffer(mCurrentFrameCtx, mesh.GetIndexBuffer());

            const Submesh* submeshes = mesh.GetSubmeshes().data();
            for (Uint i = 0; i < mesh.GetSubmeshes().size(); i++)
            {
                const Submesh& submesh = submeshes[i];

                PushConstantData pushConstants = { 
                    .Transform = cmd.Transform * submesh.Transform, 
                    .LightBufferIndex = mLightBufferIndex,
                    .LightCount = mLightCPU.Count,
                    .MaterialBufferIndex = mData->mMaterialRegistry.GetBufferBindlessIndex(),
                    .MaterialIndex = cmd.Material->GetMaterialIndex()
                };

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
        SG_ASSERT(mLightCPU.Count < MAX_LIGHTS, "Renderer3D: exceeded MAX_LIGHTS");

        mLightCPU.Lights[mLightCPU.Count++] = LightComponentToGPULight(light, position, rotation);
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
        mRHI->DestroyBuffer(mGPULightBuffer);
    }

} // namespace Surge
