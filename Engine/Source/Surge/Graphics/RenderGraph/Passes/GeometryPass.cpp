// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Profiler.hpp"
#include "Surge/Graphics/RenderGraph/Passes/GeometryPass.hpp"

namespace Surge
{
    void GeometryPass::Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard)
    {
        SURGE_PROFILE_FUNC("GeometryPass::Setup");
        mRHI = rhi;

        // Offscreen color image (blackBoard.FinalImage)
        // 2D pass will use this image to write
        glm::uvec2 size = Core::GetWindow()->GetSize();
        
        ImageDesc colorDesc = {};
        colorDesc.Width = size.x;
        colorDesc.Height = size.y;
        colorDesc.Format = ImageFormat::B10G11R11_UFLOAT_PACK32;
        colorDesc.Usage = ImageUsage::COLOR_ATTACHMENT | ImageUsage::SAMPLED;
        colorDesc.DebugName = "GPassColor";
        colorDesc.Sampler = blackBoard.DefaultSampler;
        colorDesc.GenerateImGuiID = true;
        blackBoard.MainPassColorImage = mRHI->CreateImage(colorDesc);

        // Offscreen color image (blackBoard.DepthImage)
        ImageDesc depthDesc = {};
        depthDesc.Width = size.x;
        depthDesc.Height = size.y;
        depthDesc.Format = ImageFormat::D32_SFLOAT;
        depthDesc.Usage = ImageUsage::DEPTH_ATTACHMENT | ImageUsage::SAMPLED; // We will sample from it in PostProcess pass
        depthDesc.GenerateImGuiID = true;
        depthDesc.DebugName = "GPassDepth";
        blackBoard.MainPassDepthImage = mRHI->CreateImage(depthDesc);

        // Offscreen framebuffer
        FramebufferAttachment colorAttachment = {};
        colorAttachment.Handle = blackBoard.MainPassColorImage;
        colorAttachment.Load = LoadOp::CLEAR;
        colorAttachment.Store = StoreOp::STORE;

        FramebufferAttachment depthAttachment = {};
        depthAttachment.Handle = blackBoard.MainPassDepthImage;
        depthAttachment.Load = LoadOp::CLEAR;
        depthAttachment.Store = StoreOp::STORE; // We store the depth as its used in Post Process pass

        FramebufferDesc fbDesc = {};
        fbDesc.ColorAttachments[0] = colorAttachment;
        fbDesc.ColorAttachmentCount = 1;
        fbDesc.DepthAttachment = depthAttachment;
        fbDesc.HasDepth = true;
        fbDesc.Width = size.x;
        fbDesc.Height = size.y;
        fbDesc.DebugName = "GPass Framebuffer";
        blackBoard.MainPassFramebuffer = mRHI->CreateFramebuffer(fbDesc);

        // Create 3D Pipeline
        PipelineDesc desc = {};
        desc.Shader_ = Core::GetRenderer()->GetShaderManager().Get("Renderer3D.glsl");
        desc.Raster.Topo = Topology::TRIANGLE_LIST;
        desc.Raster.Polygon = PolygonMode::FILL;
        desc.Raster.Cull = CullMode::BACK;
        desc.Stencil.Enable = false;
        desc.Depth.TestEnable = true;
        desc.Depth.WriteEnable = true;
        desc.Depth.Op = CompareOp::LESS_OR_EQUAL;
        desc.Blend.Enable = false;
        desc.DebugName = "Renderer3D";
        desc.TargetFramebuffer = blackBoard.MainPassFramebuffer;
        desc.TargetSwapchain = false;

        m3DPipeline = mRHI->CreatePipeline(desc);

        // Create Lights Storagebuffer
        BufferDesc gpuLightDesc = {};
        gpuLightDesc.Usage = BufferUsage::STORAGE;
        gpuLightDesc.HostVisible = true;
        gpuLightDesc.DebugName = "Renderer3D_Lights";
        gpuLightDesc.Size = sizeof(LightUBOData);
        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
            mLightUBOs[i] = mRHI->CreateBuffer(gpuLightDesc);

        // Create DescriptorSetSlot::ZERO
        mFrameDescriptorSet = mRHI->CreateDescriptorSet(m3DPipeline, DescriptorSetSlot::ZERO, DescriptorUpdateFrequency::DYNAMIC, "3D_FrameData [Set0]");
        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
        {
            std::array<DescriptorWrite, 2> writes = {};
            writes[0].Binding = 0;
            writes[0].Type = DescriptorType::UNIFORM_BUFFER;
            writes[0].Buffer = blackBoard.FrameUBOs[i];
            writes[0].BufferRange = sizeof(FrameUBO);
            writes[1].Binding = 1;
            writes[1].Type = DescriptorType::STORAGE_BUFFER;
            writes[1].Buffer = mLightUBOs[i];
            writes[1].BufferRange = sizeof(LightUBOData);
            mRHI->UpdateDescriptorSet(mFrameDescriptorSet, writes.data(), writes.size(), i);
        }

        // Create Shadow UBOs
        BufferDesc shadowUBODesc = {};
        shadowUBODesc.Usage = BufferUsage::UNIFORM;
        shadowUBODesc.HostVisible = true;
        shadowUBODesc.DebugName = "Renderer3D_Shadows";
        shadowUBODesc.Size = sizeof(ShadowUBO);
        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
            blackBoard.ShadowUBOs[i] = mRHI->CreateBuffer(shadowUBODesc);

        // Create DescriptorSetSlot::TWO
        mShadowMapDescriptorSet = mRHI->CreateDescriptorSet(m3DPipeline, DescriptorSetSlot::TWO, DescriptorUpdateFrequency::DYNAMIC, "ShadowMap DescriptorSet [Set2]");
        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
        {
            std::array<DescriptorWrite, 1> writes = {};
            writes[0].Binding = 3;
            writes[0].Type = DescriptorType::UNIFORM_BUFFER;
            writes[0].Buffer = blackBoard.ShadowUBOs[i];
            writes[0].BufferRange = sizeof(ShadowUBO);
            mRHI->UpdateDescriptorSet(mShadowMapDescriptorSet, writes.data(), writes.size(), i);
        }

        SamplerDesc shadowSamplerDesc = {};
        shadowSamplerDesc.WrapU = WrapMode::CLAMP;
        shadowSamplerDesc.WrapV = WrapMode::CLAMP;
        shadowSamplerDesc.CompareEnable = true;
        shadowSamplerDesc.CompareOp_ = CompareOp::LESS_OR_EQUAL;
        shadowSamplerDesc.DebugName = "ShadowComparisonSampler";
        mShadowSampler = mRHI->CreateSampler(shadowSamplerDesc);

        for(Uint i = 0; i < MAX_SHADOW_CASCADE_COUNT; i++)
            mImageReads.push_back(blackBoard.ShadowMap[i]);

        mImageWrites.push_back(blackBoard.MainPassColorImage);
        mImageWrites.push_back(blackBoard.MainPassDepthImage);

        blackBoard.MaterialPipeline = m3DPipeline;// TODO: Remove
    }

    void GeometryPass::Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard)
    {
        SURGE_PROFILE_FUNC("GeometryPass::Execute");

        // FRAME UBO is uploaded by Renderer, so we only need to upload light data here
        if(!blackBoard.LightList.empty())
        {
            LightUBOData lightData = {};
            for(Uint i = 0; i < blackBoard.LightList.size(); i++)
                lightData.Lights[i] = blackBoard.LightList[i].GPULight;

            if (blackBoard.Env.HasEnvironment)
            {
                lightData.SkyAmbient = blackBoard.Env.SkyAmbient;
                lightData.HorizonAmbient = blackBoard.Env.HorizonAmbient;
                lightData.GroundAmbient = blackBoard.Env.GroundAmbient;
            }
            else
            {
                lightData.SkyAmbient = glm::vec3(0.0f);
                lightData.HorizonAmbient = glm::vec3(0.0f);
                lightData.GroundAmbient = glm::vec3(0.0f);
            }
            mRHI->UploadBuffer(mLightUBOs[ctx.FrameIndex], &lightData, sizeof(LightUBOData), 0);
        }

        {   // TODO: Move this out of the execute loop
            std::array<DescriptorWrite, MAX_SHADOW_CASCADE_COUNT> writes = {};
            for(Uint i = 0; i < MAX_SHADOW_CASCADE_COUNT; i++)
            {
                writes[i].Binding = i;
                writes[i].Type = DescriptorType::TEXTURE;
                writes[i].Sampler = mShadowSampler;

                if(i < (Uint)blackBoard.ShadowSettings_.CascadeCount)
                    writes[i].Texture = blackBoard.ShadowMap[i]; // Valid cascade
                else
                {
                    //writes[i].Texture = mDummyShadowMap; 
                    writes[i].Texture = blackBoard.ShadowMap[i]; // TODO REMOVE!: Inactive cascade: Bind a 1x1 dummy depth texture
                    Log<Severity::Warn>("TOOD: bind 1x1 dummy depth texture");
                }
            }
            mRHI->UpdateDescriptorSet(mShadowMapDescriptorSet, writes.data(), writes.size(), ctx.FrameIndex);
        }

        mRHI->CmdBindPipeline(ctx, m3DPipeline);
        mRHI->CmdBindDescriptorSet(ctx, m3DPipeline, mFrameDescriptorSet, DescriptorSetSlot::ZERO);
        mRHI->CmdBindDescriptorSet(ctx, m3DPipeline, mShadowMapDescriptorSet, DescriptorSetSlot::TWO);

        for(const MeshSubmitCmd& cmd : blackBoard.MeshList)
        {
            const Mesh& mesh = *cmd.Mesh_;
            mRHI->CmdBindVertexBuffer(ctx, mesh.GetVertexBuffer());
            mRHI->CmdBindIndexBuffer(ctx, mesh.GetIndexBuffer());

            const Submesh* submeshes = mesh.GetSubmeshes().data();
            for(Uint i = 0; i < mesh.GetSubmeshes().size(); i++)
            {
                const Submesh& submesh = submeshes[i];

                PushConstantData pushConstants = {};
                pushConstants.Transform = cmd.Transform * submesh.Transform;
                pushConstants.LightCount = (Uint)blackBoard.LightList.size();

                const Ref<Material> material = mesh.GetMaterialAtIndex(submesh.MaterialIndex);
                material->UpdateForRendering(ctx);
                material->Bind(ctx, m3DPipeline);

                mRHI->CmdPushConstants(ctx, m3DPipeline, ShaderType::VERTEX | ShaderType::FRAGMENT, 0, sizeof(PushConstantData), &pushConstants);
                mRHI->CmdDrawIndexed(ctx, submesh.IndexCount, 1, submesh.BaseIndex, submesh.BaseVertex, 0);
            }
        }
    }

    void GeometryPass::Resize(Uint width, Uint height, FrameBlackboard& blackBoard)
    {
        Core::AddFrameEndCallback([this, width, height, blackBoard]() { mRHI->ResizeFramebuffer(blackBoard.MainPassFramebuffer, width, height); });
    }

    void GeometryPass::OnImGuiRender(FrameBlackboard&) {}

    void GeometryPass::Shutdown(FrameBlackboard& blackBoard)
    {
        SURGE_PROFILE_FUNC("GeometryPass::Shutdown");
        mRHI->DestroySampler(mShadowSampler);
        mRHI->DestroyFramebuffer(blackBoard.MainPassFramebuffer);
        mRHI->DestroyImage(blackBoard.MainPassColorImage);
        mRHI->DestroyImage(blackBoard.MainPassDepthImage);
        mRHI->DestroyPipeline(m3DPipeline);

        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
        {
            mRHI->DestroyBuffer(mLightUBOs[i]);
            mRHI->DestroyBuffer(blackBoard.ShadowUBOs[i]);
        }

        mRHI->DestroyDescriptorSet(mFrameDescriptorSet);
        mRHI->DestroyDescriptorSet(mShadowMapDescriptorSet);
    }
}
