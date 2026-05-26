// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Core/Core.hpp"
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
            DescriptorWrite frameDescriptorWrite[2] = {};
            frameDescriptorWrite[0].Binding = 0;
            frameDescriptorWrite[0].Type = DescriptorType::UNIFORM_BUFFER;
            frameDescriptorWrite[0].Buffer = blackBoard.FrameUBOs[i];
            frameDescriptorWrite[1].BufferRange = sizeof(FrameUBO);

            frameDescriptorWrite[1].Binding = 1;
            frameDescriptorWrite[1].Type = DescriptorType::STORAGE_BUFFER;
            frameDescriptorWrite[1].Buffer = mLightUBOs[i];
            frameDescriptorWrite[1].BufferRange = sizeof(LightUBOData);
            mRHI->UpdateDescriptorSet(mFrameDescriptorSet, frameDescriptorWrite, 2, i);
        }

        mImageWrites.push_back(blackBoard.MainPassColorImage);
        mImageWrites.push_back(blackBoard.MainPassDepthImage);

        blackBoard.MaterialPipeline = m3DPipeline;// TODO: Remove
    }

    void GeometryPass::Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard)
    {
        SURGE_PROFILE_FUNC("GeometryPass::Execute");
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
        // blackBoard.OffscreenFramebuffer is owned by GeometryPass and thus resized by GeometryPass
        //if(RHISettings::RENDER_TO_SWAPCHAIN && (width > 0 && height > 0))
            Core::AddFrameEndCallback([this, width, height, blackBoard]() { mRHI->ResizeFramebuffer(blackBoard.MainPassFramebuffer, width, height); }); // (Player)

        // If not blitted to swapchain, user handles resize // (Editor)
    }

    void GeometryPass::OnImGuiRender(FrameBlackboard& blackBoard)
    {

    }

    void GeometryPass::Shutdown(FrameBlackboard& blackBoard)
    {
        SURGE_PROFILE_FUNC("GeometryPass::Shutdown");

        mRHI->DestroyFramebuffer(blackBoard.MainPassFramebuffer);
        mRHI->DestroyImage(blackBoard.MainPassColorImage);
        mRHI->DestroyImage(blackBoard.MainPassDepthImage);

        mRHI->DestroyPipeline(m3DPipeline);

        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
            mRHI->DestroyBuffer(mLightUBOs[i]);

        mRHI->DestroyDescriptorSet(mFrameDescriptorSet);
    }
}
