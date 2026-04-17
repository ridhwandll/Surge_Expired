// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Renderer/ForwardRenderer.hpp"
#include "Surge/Graphics/RHI/RHICommandBuffer.hpp"
#include "Surge/Graphics/Interface/Texture.hpp"

#define BASE_SHADER_PATH "Engine/Assets/Shaders"

namespace Surge
{
    void ForwardRenderer::Initialize()
    {
        mShaderSet.Initialize(BASE_SHADER_PATH);
        mShaderSet.AddShader("PBR.glsl");
        mShaderSet.LoadAll();

        Uint whitePixel = 0xffffffff;
        mWhiteTexture = Texture2D::Create(ImageFormat::RGBA8, 1, 1, &whitePixel);

        mInitialized = true;
    }

    void ForwardRenderer::Shutdown()
    {
        mWhiteTexture.Reset();
        mShaderSet.Shutdown();
        mRenderGraph.Reset();
        mDrawList.clear();
        mPointLights.clear();
        mInitialized = false;
    }

    void ForwardRenderer::BeginFrame(const EditorCamera& camera)
    {
        mCameraData.View           = camera.GetViewMatrix();
        mCameraData.Projection     = camera.GetProjectionMatrix();
        mCameraData.ViewProjection = mCameraData.Projection * mCameraData.View;
        mCameraData.Position       = glm::vec4(camera.GetPosition(), 1.0f);

        mDrawList.clear();
        mPointLights.clear();
    }

    void ForwardRenderer::BeginFrame(const glm::mat4& view, const glm::mat4& projection,
                                      const glm::vec3& cameraWorldPos)
    {
        mCameraData.View           = view;
        mCameraData.Projection     = projection;
        mCameraData.ViewProjection = projection * view;
        mCameraData.Position       = glm::vec4(cameraWorldPos, 1.0f);

        mDrawList.clear();
        mPointLights.clear();
    }

    void ForwardRenderer::SubmitMesh(MeshComponent& meshComp, const glm::mat4& transform)
    {
        mDrawList.push_back({&meshComp, transform});
    }

    void ForwardRenderer::SubmitPointLight(const PointLight& light)
    {
        mPointLights.push_back(light);
    }

    void ForwardRenderer::SetDirectionalLight(const DirectionalLight& light)
    {
        mDirLight = light;
    }

    void ForwardRenderer::SetRenderArea(uint32_t width, uint32_t height)
    {
        mRenderWidth  = width;
        mRenderHeight = height;
    }

    void ForwardRenderer::RegisterGeometryPass()
    {
        // Declare transient HDR colour + depth targets
        RGTextureDesc hdrDesc;
        hdrDesc.Width  = 0; // swapchain-sized
        hdrDesc.Height = 0;
        hdrDesc.Format = RHI::ImageFormat::RGBA16F;
        hdrDesc.Usage  = RHI::ImageUsage::Attachment | RHI::ImageUsage::Sampled;
        hdrDesc.Name   = "HDR Color";

        RGTextureDesc depthDesc;
        depthDesc.Width  = 0;
        depthDesc.Height = 0;
        depthDesc.Format = RHI::ImageFormat::Depth32F;
        depthDesc.Usage  = RHI::ImageUsage::Attachment;
        depthDesc.Name   = "Depth";

        // Capture by value so the handles are valid in the execute callback
        mRenderGraph.AddPass(
            "GeometryPass",
            [this, hdrDesc, depthDesc](RenderGraphBuilder& builder) {
                mHDRColorTarget = builder.WriteColor(builder.Create(hdrDesc));
                mDepthTarget    = builder.WriteDepth(builder.Create(depthDesc));
            },
            [this](RHI::CommandBufferHandle cmd, const PassResources& res) {
                RHI::TextureHandle color = res.GetTexture(mHDRColorTarget);
                RHI::TextureHandle depth = res.GetTexture(mDepthTarget);
                (void)color; (void)depth;

                // TODO: bind pipeline, upload camera UBO, draw mesh list
                // This is a stub; real implementation will be added when the
                // Vulkan pipeline and descriptor set abstraction is wired in.
                for (const auto& dc : mDrawList)
                {
                    (void)dc;
                    // rhiCmdBindVertexBuffer(cmd, dc.MeshComp->Mesh->GetVertexBufferHandle());
                    // rhiCmdBindIndexBuffer(cmd, dc.MeshComp->Mesh->GetIndexBufferHandle());
                    // rhiCmdPushConstants(cmd, ShaderStage::Vertex, &dc.Transform, sizeof(glm::mat4));
                    // rhiCmdDrawIndexed(cmd, dc.MeshComp->Mesh->GetIndexCount());
                }
            });
    }

    void ForwardRenderer::RegisterLightingPass()
    {
        RGTextureDesc finalDesc;
        finalDesc.Width  = 0;
        finalDesc.Height = 0;
        finalDesc.Format = RHI::ImageFormat::RGBA8;
        finalDesc.Usage  = RHI::ImageUsage::Attachment | RHI::ImageUsage::Sampled;
        finalDesc.Name   = "Final Color";

        mRenderGraph.AddPass(
            "LightingPass",
            [this, finalDesc](RenderGraphBuilder& builder) {
                builder.Read(mHDRColorTarget);
                builder.Read(mDepthTarget);
                mFinalColorTarget = builder.WriteColor(builder.Create(finalDesc));
            },
            [this](RHI::CommandBufferHandle cmd, const PassResources& res) {
                (void)cmd; (void)res;
                // TODO: fullscreen quad, sample HDR + depth, apply lighting
            });
    }

    void ForwardRenderer::EndFrame()
    {
        mRenderGraph.Reset();

        RegisterGeometryPass();
        RegisterLightingPass();

        mRenderGraph.Compile(mRenderWidth, mRenderHeight);
        mCompiled = true;

        // Execute requires a valid CommandBufferHandle from the new RHI path.
        // When the new RHI backend is wired up, obtain the frame command buffer
        // via rhiSwapchainGetCurrentCommandBuffer() and call:
        //   mRenderGraph.Execute(frameCmdBuf);
        // For now, execution is a no-op (device not yet registered).
    }

    RHI::TextureHandle ForwardRenderer::GetFinalColorTexture() const
    {
        if (!mCompiled)
            return RHI::TextureHandle{};
        return mRenderGraph.GetTexture(mFinalColorTarget);
    }

} // namespace Surge
