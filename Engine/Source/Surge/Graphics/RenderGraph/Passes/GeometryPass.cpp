// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Core/Profiler.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Graphics/RenderGraph/Passes/GeometryPass.hpp"
#include "SurgeMath/Frustum.hpp"
#include <imgui.h>

namespace Surge
{
    static constexpr Uint INITIAL_INSTANCE_CAPACITY = 1024; // 64 KB per frame in flight, grows on demand
    static constexpr Uint INITIAL_SKINNED_INSTANCE_CAPACITY = 64;
    static_assert(sizeof(LightUBOData::Lights) / sizeof(Light) == GeometryPass::MAX_LIGHTS, "LightUBOData::Lights must hold GeometryPass::MAX_LIGHTS lights");

    static bool SortDrawItems(const Material* materialA, const Mesh* meshA, Uint submeshA, const Material* materialB, const Mesh* meshB, Uint submeshB)
    {
        // Material first (descriptor set binds), then mesh (vertex/index buffer binds), then submesh (instancing runs)
        if(materialA != materialB)
            return materialA < materialB;
        if(meshA != meshB)
            return meshA < meshB;
        return submeshA < submeshB;
    }

    void GeometryPass::Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard)
    {
        SURGE_PROFILE_FUNC("GeometryPass::Setup");
        mRHI = rhi;

        // Offscreen color image (blackBoard.FinalImage)
        // 2D pass will use this image to write
        const glm::vec2 windowSize = mContext.WindowSize;
        const glm::uvec2 size = blackBoard.GetSceneSize(static_cast<Uint>(windowSize.x), static_cast<Uint>(windowSize.y));

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
        depthDesc.Usage = ImageUsage::DEPTH_ATTACHMENT | ImageUsage::SAMPLED; // Sampled by AO + PostProcess (AO upsample)
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
        depthAttachment.Store = StoreOp::STORE; // Switched to DONT_CARE in Prepare() while nothing samples it
        mDepthStored = true;

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
        desc.Shader_ = mContext.Shaders->Get("Renderer3D.glsl");
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

        // Same state, skinned vertex layout. Its material (set 1) and shadow (set 2) layouts are identical to Renderer3D.glsl's
        // (same fragment include), so the materials' sets and mShadowMapDescriptorSet bind with this pipeline too
        desc.Shader_ = mContext.Shaders->Get("Renderer3DSkinned.glsl");
        desc.DebugName = "Renderer3DSkinned";
        m3DSkinnedPipeline = mRHI->CreatePipeline(desc);

        // Create Lights UBO
        BufferDesc gpuLightDesc = {};
        gpuLightDesc.Usage = BufferUsage::UNIFORM;
        gpuLightDesc.HostVisible = true;
        gpuLightDesc.DebugName = "Renderer3D_Lights";
        gpuLightDesc.Size = sizeof(LightUBOData);
        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
            mLightUBOs[i] = mRHI->CreateBuffer(gpuLightDesc);

        // Per-instance transforms
        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
        {
            EnsureInstanceCapacity(i, INITIAL_INSTANCE_CAPACITY);
            EnsureSkinnedInstanceCapacity(i, INITIAL_SKINNED_INSTANCE_CAPACITY);
        }

        // Create DescriptorSetSlot::ZERO
        mFrameDescriptorSet = mRHI->CreateDescriptorSet(m3DPipeline, DescriptorSetSlot::ZERO, DescriptorUpdateFrequency::DYNAMIC, "3D_FrameData [Set0]");
        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
        {
            std::array<DescriptorWrite, 3> writes = {};
            writes[0].Binding = 0;
            writes[0].Type = DescriptorType::UNIFORM_BUFFER;
            writes[0].Buffer = blackBoard.FrameUBOs[i];
            writes[0].BufferRange = sizeof(FrameUBO);
            writes[1].Binding = 1;
            writes[1].Type = DescriptorType::UNIFORM_BUFFER;
            writes[1].Buffer = mLightUBOs[i];
            writes[1].BufferRange = sizeof(LightUBOData);
            writes[2].Binding = 2;
            writes[2].Type = DescriptorType::STORAGE_BUFFER;
            writes[2].Buffer = mInstanceBuffers[i];
            writes[2].BufferRange = sizeof(glm::mat4) * mInstanceCapacity[i];
            mRHI->UpdateDescriptorSet(mFrameDescriptorSet, writes.data(), static_cast<Uint>(writes.size()), i);
        }

        mSkinnedFrameDescriptorSet = mRHI->CreateDescriptorSet(m3DSkinnedPipeline, DescriptorSetSlot::ZERO, DescriptorUpdateFrequency::DYNAMIC, "3D_SkinnedFrameData [Set0]");
        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
        {
            std::array<DescriptorWrite, 4> writes = {};
            writes[0].Binding = 0;
            writes[0].Type = DescriptorType::UNIFORM_BUFFER;
            writes[0].Buffer = blackBoard.FrameUBOs[i];
            writes[0].BufferRange = sizeof(FrameUBO);
            writes[1].Binding = 1;
            writes[1].Type = DescriptorType::UNIFORM_BUFFER;
            writes[1].Buffer = mLightUBOs[i];
            writes[1].BufferRange = sizeof(LightUBOData);
            writes[2].Binding = 2;
            writes[2].Type = DescriptorType::STORAGE_BUFFER;
            writes[2].Buffer = mSkinnedInstanceBuffers[i];
            writes[2].BufferRange = sizeof(SkinnedInstance) * mSkinnedInstanceCapacity[i];
            writes[3].Binding = 3;
            writes[3].Type = DescriptorType::STORAGE_BUFFER;
            writes[3].Buffer = blackBoard.SkinBuffers[i];
            writes[3].BufferRange = sizeof(SkinMatrix) * blackBoard.SkinBufferCapacity[i];
            mRHI->UpdateDescriptorSet(mSkinnedFrameDescriptorSet, writes.data(), static_cast<Uint>(writes.size()), i);
            mSkinBufferVersion[i] = blackBoard.SkinBufferVersion[i];
        }

        // Create Shadow UBOs
        BufferDesc shadowUBODesc = {};
        shadowUBODesc.Usage = BufferUsage::UNIFORM;
        shadowUBODesc.HostVisible = true;
        shadowUBODesc.DebugName = "Renderer3D_Shadows";
        shadowUBODesc.Size = sizeof(ShadowUBO);
        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
            blackBoard.ShadowUBOs[i] = mRHI->CreateBuffer(shadowUBODesc);

        SamplerDesc shadowSamplerDesc = {};
        shadowSamplerDesc.WrapU = WrapMode::CLAMP;
        shadowSamplerDesc.WrapV = WrapMode::CLAMP;
        shadowSamplerDesc.CompareEnable = true;
        shadowSamplerDesc.CompareOp_ = CompareOp::LESS_OR_EQUAL;
        shadowSamplerDesc.DebugName = "ShadowComparisonSampler";
        mShadowSampler = mRHI->CreateSampler(shadowSamplerDesc);

        // Create DescriptorSetSlot::TWO: shadow atlas + ShadowUBO, the atlas binding is rewritten when ShadowPass recreates the atlas
        mShadowMapDescriptorSet = mRHI->CreateDescriptorSet(m3DPipeline, DescriptorSetSlot::TWO, DescriptorUpdateFrequency::DYNAMIC, "ShadowMap DescriptorSet [Set2]");
        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
        {
            std::array<DescriptorWrite, 2> writes = {};
            writes[0].Binding = 0;
            writes[0].Type = DescriptorType::TEXTURE;
            writes[0].Sampler = mShadowSampler;
            writes[0].Texture = blackBoard.ShadowAtlas;
            writes[1].Binding = 1;
            writes[1].Type = DescriptorType::UNIFORM_BUFFER;
            writes[1].Buffer = blackBoard.ShadowUBOs[i];
            writes[1].BufferRange = sizeof(ShadowUBO);
            mRHI->UpdateDescriptorSet(mShadowMapDescriptorSet, writes.data(), static_cast<Uint>(writes.size()), i);
            mShadowAtlasVersion[i] = blackBoard.ShadowAtlasVersion;
        }

        mImageReads.push_back(blackBoard.ShadowAtlas);

        mImageWrites.push_back(blackBoard.MainPassColorImage);
        mImageWrites.push_back(blackBoard.MainPassDepthImage);

        mVisibleTransforms.reserve(INITIAL_INSTANCE_CAPACITY);
        mDrawItems.reserve(INITIAL_INSTANCE_CAPACITY);
        mDraws.reserve(256);
        mVisibleSkinned.reserve(INITIAL_SKINNED_INSTANCE_CAPACITY);
        mSkinnedDrawItems.reserve(INITIAL_SKINNED_INSTANCE_CAPACITY);

        blackBoard.MaterialPipeline = m3DPipeline;// TODO: Remove
    }

    void GeometryPass::EnsureInstanceCapacity(Uint frameIndex, Uint instanceCount)
    {
        if(instanceCount <= mInstanceCapacity[frameIndex] && !mInstanceBuffers[frameIndex].IsNull())
            return;

        Uint capacity = std::max(mInstanceCapacity[frameIndex], INITIAL_INSTANCE_CAPACITY);
        while(capacity < instanceCount)
            capacity *= 2;

        if(!mInstanceBuffers[frameIndex].IsNull())
            mRHI->DestroyBuffer(mInstanceBuffers[frameIndex]); // Deferred, the previous use of this frame slot has already retired

        BufferDesc desc = {};
        desc.Usage = BufferUsage::STORAGE;
        desc.HostVisible = true;
        desc.Size = sizeof(glm::mat4) * capacity;
        desc.DebugName = std::format("Renderer3D_Instances Frame: {}", frameIndex);
        mInstanceBuffers[frameIndex] = mRHI->CreateBuffer(desc);

        const bool resized = mInstanceCapacity[frameIndex] != 0;
        mInstanceCapacity[frameIndex] = capacity;

        // Setup writes the full set itself, afterwards only this frame's binding needs refreshing (done before the set is bound this frame)
        if(resized)
        {
            DescriptorWrite write = {};
            write.Binding = 2;
            write.Type = DescriptorType::STORAGE_BUFFER;
            write.Buffer = mInstanceBuffers[frameIndex];
            write.BufferRange = sizeof(glm::mat4) * capacity;
            mRHI->UpdateDescriptorSet(mFrameDescriptorSet, &write, 1, frameIndex);
        }
    }

    void GeometryPass::EnsureSkinnedInstanceCapacity(Uint frameIndex, Uint instanceCount)
    {
        if(instanceCount <= mSkinnedInstanceCapacity[frameIndex] && !mSkinnedInstanceBuffers[frameIndex].IsNull())
            return;

        Uint capacity = std::max(mSkinnedInstanceCapacity[frameIndex], INITIAL_SKINNED_INSTANCE_CAPACITY);
        while(capacity < instanceCount)
            capacity *= 2;

        if(!mSkinnedInstanceBuffers[frameIndex].IsNull())
            mRHI->DestroyBuffer(mSkinnedInstanceBuffers[frameIndex]); // Deferred, the previous use of this frame slot has already retired

        BufferDesc desc = {};
        desc.Usage = BufferUsage::STORAGE;
        desc.HostVisible = true;
        desc.Size = sizeof(SkinnedInstance) * capacity;
        desc.DebugName = std::format("Renderer3D_SkinnedInstances Frame: {}", frameIndex);
        mSkinnedInstanceBuffers[frameIndex] = mRHI->CreateBuffer(desc);

        const bool resized = mSkinnedInstanceCapacity[frameIndex] != 0;
        mSkinnedInstanceCapacity[frameIndex] = capacity;
        if(resized)
        {
            DescriptorWrite write = {};
            write.Binding = 2;
            write.Type = DescriptorType::STORAGE_BUFFER;
            write.Buffer = mSkinnedInstanceBuffers[frameIndex];
            write.BufferRange = sizeof(SkinnedInstance) * capacity;
            mRHI->UpdateDescriptorSet(mSkinnedFrameDescriptorSet, &write, 1, frameIndex);
        }
    }

    void GeometryPass::UpdateSkinDescriptors(const FrameContext& ctx, const FrameBlackboard& blackBoard)
    {
        // Renderer::UploadSkinMatrices grew this frame's buffer (before any pass prepared)
        if(mSkinBufferVersion[ctx.FrameIndex] == blackBoard.SkinBufferVersion[ctx.FrameIndex])
            return;

        DescriptorWrite write = {};
        write.Binding = 3;
        write.Type = DescriptorType::STORAGE_BUFFER;
        write.Buffer = blackBoard.SkinBuffers[ctx.FrameIndex];
        write.BufferRange = sizeof(SkinMatrix) * blackBoard.SkinBufferCapacity[ctx.FrameIndex];
        mRHI->UpdateDescriptorSet(mSkinnedFrameDescriptorSet, &write, 1, ctx.FrameIndex);
        mSkinBufferVersion[ctx.FrameIndex] = blackBoard.SkinBufferVersion[ctx.FrameIndex];
    }

    void GeometryPass::UpdateDepthStoreOp(const FrameBlackboard& blackBoard)
    {
        // Writing a full resolution D32 buffer back to DRAM every frame is pure bandwidth unless AO (and the AO upsample in PostProcess) reads it
        const bool depthNeeded = blackBoard.AOSettings.Enable;
        if(depthNeeded == mDepthStored)
            return;

        FramebufferDesc desc = mRHI->GetDesc(blackBoard.MainPassFramebuffer);
        desc.DepthAttachment.Store = depthNeeded ? StoreOp::STORE : StoreOp::DONT_CARE;
        mRHI->RecreateFramebuffer(blackBoard.MainPassFramebuffer, desc); // Render pass compatible, pipelines stay valid
        mDepthStored = depthNeeded;
    }

    void GeometryPass::UpdateShadowDescriptors(const FrameContext& ctx, const FrameBlackboard& blackBoard)
    {
        // ShadowPass::Prepare (runs before this pass) recreated the atlas: new image view. Only this frame's copy is rewritten,
        // the other frame in flight may still be reading its own until its slot comes around
        if(mShadowAtlasVersion[ctx.FrameIndex] == blackBoard.ShadowAtlasVersion)
            return;

        DescriptorWrite write = {};
        write.Binding = 0;
        write.Type = DescriptorType::TEXTURE;
        write.Sampler = mShadowSampler;
        write.Texture = blackBoard.ShadowAtlas;
        mRHI->UpdateDescriptorSet(mShadowMapDescriptorSet, &write, 1, ctx.FrameIndex);
        mShadowAtlasVersion[ctx.FrameIndex] = blackBoard.ShadowAtlasVersion;
    }

    void GeometryPass::UploadLights(const FrameContext& ctx, const FrameBlackboard& blackBoard)
    {
        // Pick the lights that matter: every directional light first, then point lights that touch the view frustum, nearest first
        // The shader loops over all uploaded lights per pixel, so this cap is the per-pixel lighting budget
        const Uint budget = std::min(blackBoard.Lighting.MaxLights, MAX_LIGHTS);
        const Frustum frustum(blackBoard.ViewProjection);

        Uint count = 0;
        mPointLightOrder.clear();
        for(Uint i = 0; i < static_cast<Uint>(blackBoard.LightList.size()); i++)
        {
            const Light& light = blackBoard.LightList[i].GPULight;
            if(light.PositionType.w == static_cast<float>(LightType::DIRECTIONAL))
            {
                if(count < budget)
                    mLightData.Lights[count++] = light;
                continue;
            }

            const glm::vec3 position = glm::vec3(light.PositionType);
            if(!frustum.IntersectsSphere(position, light.Radius))
                continue;

            const float distance = glm::length(position - blackBoard.CameraPosition) - light.Radius;
            mPointLightOrder.push_back({ distance, i });
        }

        if(count < budget && !mPointLightOrder.empty())
        {
            const Uint remaining = std::min(budget - count, static_cast<Uint>(mPointLightOrder.size()));
            std::partial_sort(mPointLightOrder.begin(), mPointLightOrder.begin() + remaining, mPointLightOrder.end(),
                              [](const Pair<float, Uint>& a, const Pair<float, Uint>& b) { return a.Data1 < b.Data1; });

            for(Uint i = 0; i < remaining; i++)
                mLightData.Lights[count++] = blackBoard.LightList[mPointLightOrder[i].Data2].GPULight;
        }

        if(blackBoard.Env.HasEnvironment)
        {
            mLightData.SkyAmbient = blackBoard.Env.SkyAmbient;
            mLightData.HorizonAmbient = blackBoard.Env.HorizonAmbient;
            mLightData.GroundAmbient = blackBoard.Env.GroundAmbient;
        }
        else
        {
            mLightData.SkyAmbient = glm::vec3(0.0f);
            mLightData.HorizonAmbient = glm::vec3(0.0f);
            mLightData.GroundAmbient = glm::vec3(0.0f);
        }

        // Only the header + the lights in use, not the whole MAX_LIGHTS entry array
        const Uint uploadSize = static_cast<Uint>(offsetof(LightUBOData, Lights) + sizeof(Light) * count);
        mRHI->UploadBuffer(mLightUBOs[ctx.FrameIndex], &mLightData, uploadSize, 0);
        mUploadedLightCount = count;
    }

    void GeometryPass::BuildDrawList(const FrameContext& ctx, const FrameBlackboard& blackBoard)
    {
        SURGE_PROFILE_FUNC("GeometryPass::BuildDrawList");

        const Frustum frustum(blackBoard.ViewProjection);
        mVisibleTransforms.clear();
        mDrawItems.clear();
        mDraws.clear();
        mSubmittedSubmeshes = 0;

        for(const MeshSubmitCmd& cmd : blackBoard.MeshList)
        {
            if(cmd.SkinOffset != INVALID_SKIN_OFFSET)
                continue; // BuildSkinnedDrawList

            const Mesh& mesh = *cmd.Mesh_;
            const Vector<Submesh>& submeshes = mesh.GetSubmeshes();
            mSubmittedSubmeshes += static_cast<Uint>(submeshes.size());

            for(Uint i = 0; i < static_cast<Uint>(submeshes.size()); i++)
            {
                const Submesh& submesh = submeshes[i];
                const glm::mat4 transform = cmd.Transform * submesh.Transform;
                if(!frustum.IntersectsAABB(submesh.BoundingBox, transform))
                    continue;

                mDrawItems.push_back({ mesh.GetMaterialPtrAtIndex(submesh.MaterialIndex), &mesh, i, static_cast<Uint>(mVisibleTransforms.size()) });
                mVisibleTransforms.push_back(transform);
            }
        }
        mVisibleSubmeshes = static_cast<Uint>(mDrawItems.size());

        if(mDrawItems.empty())
            return;

        std::sort(mDrawItems.begin(), mDrawItems.end(), [](const DrawItem& a, const DrawItem& b)
        {
            return SortDrawItems(a.Material_, a.Mesh_, a.SubmeshIndex, b.Material_, b.Mesh_, b.SubmeshIndex);
        });

        const Uint instanceCount = static_cast<Uint>(mDrawItems.size());
        EnsureInstanceCapacity(ctx.FrameIndex, instanceCount);

        // Transforms in sorted order, so every run of identical draws is one contiguous instance range
        mSortedTransforms.resize(instanceCount);
        for(Uint i = 0; i < instanceCount; i++)
            mSortedTransforms[i] = mVisibleTransforms[mDrawItems[i].TransformIndex];
        mRHI->UploadBuffer(mInstanceBuffers[ctx.FrameIndex], mSortedTransforms.data(), static_cast<Uint>(sizeof(glm::mat4) * instanceCount), 0);

        for(Uint i = 0; i < instanceCount; i++)
        {
            const DrawItem& item = mDrawItems[i];
            if(!mDraws.empty())
            {
                InstancedDraw& last = mDraws.back();
                if(last.Material_ == item.Material_ && last.Mesh_ == item.Mesh_ && last.SubmeshIndex == item.SubmeshIndex)
                {
                    last.InstanceCount++;
                    continue;
                }
            }
            mDraws.push_back({ item.Material_, item.Mesh_, item.SubmeshIndex, i, 1 });
        }
    }

    void GeometryPass::BuildSkinnedDrawList(const FrameContext& ctx, const FrameBlackboard& blackBoard)
    {
        SURGE_PROFILE_FUNC("GeometryPass::BuildSkinnedDrawList");

        const Frustum frustum(blackBoard.ViewProjection);
        mVisibleSkinned.clear();
        mSkinnedDrawItems.clear();
        mSkinnedDraws.clear();
        mSubmittedSkinned = 0;
        mVisibleSkinnedMeshes = 0;

        for(const MeshSubmitCmd& cmd : blackBoard.MeshList)
        {
            if(cmd.SkinOffset == INVALID_SKIN_OFFSET)
                continue;

            // One box for the whole pose: the submesh bounds are rest pose, the skin moves the vertices anywhere
            mSubmittedSkinned++;
            if(!frustum.IntersectsAABB(cmd.SkinBounds, cmd.Transform))
                continue;

            mVisibleSkinnedMeshes++;
            const Mesh& mesh = *cmd.Mesh_;
            const Uint submeshCount = static_cast<Uint>(mesh.GetSubmeshes().size());
            for(Uint i = 0; i < submeshCount; i++)
            {
                mSkinnedDrawItems.push_back({ mesh.GetMaterialPtrAtIndex(mesh.GetSubmeshes()[i].MaterialIndex), &mesh, i, static_cast<Uint>(mVisibleSkinned.size()) });
                mVisibleSkinned.push_back({ cmd.Transform, cmd.SkinOffset, {} });
            }
        }

        if(mSkinnedDrawItems.empty())
            return;

        std::sort(mSkinnedDrawItems.begin(), mSkinnedDrawItems.end(), [](const DrawItem& a, const DrawItem& b)
        {
            return SortDrawItems(a.Material_, a.Mesh_, a.SubmeshIndex, b.Material_, b.Mesh_, b.SubmeshIndex);
        });

        const Uint instanceCount = static_cast<Uint>(mSkinnedDrawItems.size());
        EnsureSkinnedInstanceCapacity(ctx.FrameIndex, instanceCount);

        mSortedSkinned.resize(instanceCount);
        for(Uint i = 0; i < instanceCount; i++)
            mSortedSkinned[i] = mVisibleSkinned[mSkinnedDrawItems[i].TransformIndex];
        mRHI->UploadBuffer(mSkinnedInstanceBuffers[ctx.FrameIndex], mSortedSkinned.data(), static_cast<Uint>(sizeof(SkinnedInstance) * instanceCount), 0);

        for(Uint i = 0; i < instanceCount; i++)
        {
            const DrawItem& item = mSkinnedDrawItems[i];
            if(!mSkinnedDraws.empty())
            {
                InstancedDraw& last = mSkinnedDraws.back();
                if(last.Material_ == item.Material_ && last.Mesh_ == item.Mesh_ && last.SubmeshIndex == item.SubmeshIndex)
                {
                    last.InstanceCount++;
                    continue;
                }
            }
            mSkinnedDraws.push_back({ item.Material_, item.Mesh_, item.SubmeshIndex, i, 1 });
        }
    }

    void GeometryPass::Prepare(const FrameContext& ctx, FrameBlackboard& blackBoard)
    {
        SURGE_PROFILE_FUNC("GeometryPass::Prepare");
        UpdateDepthStoreOp(blackBoard);
        UpdateShadowDescriptors(ctx, blackBoard);
        UpdateSkinDescriptors(ctx, blackBoard);
        UploadLights(ctx, blackBoard);
        BuildDrawList(ctx, blackBoard);
        BuildSkinnedDrawList(ctx, blackBoard);
        blackBoard.HasSceneGeometry = !mDraws.empty() || !mSkinnedDraws.empty();
    }

    void GeometryPass::Execute(const FrameContext& ctx, const FrameBlackboard&)
    {
        SURGE_PROFILE_FUNC("GeometryPass::Execute");

        if(!mDraws.empty())
        {
            mRHI->CmdBindPipeline(ctx, m3DPipeline);
            mRHI->CmdBindDescriptorSet(ctx, m3DPipeline, mFrameDescriptorSet, DescriptorSetSlot::ZERO);
            DrawRuns(ctx, m3DPipeline, mDraws);
        }

        if(!mSkinnedDraws.empty())
        {
            // Set 0 differs between the two pipelines, so binding this one disturbs every set: they are bound again in DrawRuns
            mRHI->CmdBindPipeline(ctx, m3DSkinnedPipeline);
            mRHI->CmdBindDescriptorSet(ctx, m3DSkinnedPipeline, mSkinnedFrameDescriptorSet, DescriptorSetSlot::ZERO);
            DrawRuns(ctx, m3DSkinnedPipeline, mSkinnedDraws);
        }
    }

    void GeometryPass::DrawRuns(const FrameContext& ctx, PipelineHandle pipeline, const Vector<InstancedDraw>& draws)
    {
        mRHI->CmdBindDescriptorSet(ctx, pipeline, mShadowMapDescriptorSet, DescriptorSetSlot::TWO);

        PushConstantData pushConstants = {};
        pushConstants.LightCount = mUploadedLightCount;
        mRHI->CmdPushConstants(ctx, pipeline, ShaderType::VERTEX | ShaderType::FRAGMENT, 0, sizeof(PushConstantData), &pushConstants);

        const Material* boundMaterial = nullptr;
        const Mesh* boundMesh = nullptr;
        for(const InstancedDraw& draw : draws)
        {
            if(draw.Material_ != boundMaterial)
            {
                draw.Material_->Bind(ctx, pipeline); // Uploads its uniforms/descriptors first if dirty
                boundMaterial = draw.Material_;
            }

            if(draw.Mesh_ != boundMesh)
            {
                mRHI->CmdBindVertexBuffer(ctx, draw.Mesh_->GetVertexBuffer());
                mRHI->CmdBindIndexBuffer(ctx, draw.Mesh_->GetIndexBuffer());
                boundMesh = draw.Mesh_;
            }

            const Submesh& submesh = draw.Mesh_->GetSubmeshes()[draw.SubmeshIndex];
            mRHI->CmdDrawIndexed(ctx, submesh.IndexCount, draw.InstanceCount, submesh.BaseIndex, submesh.BaseVertex, draw.FirstInstance);
        }
    }

    void GeometryPass::Resize(Uint width, Uint height, FrameBlackboard& blackBoard)
    {
        const glm::uvec2 size = blackBoard.GetSceneSize(width, height);
        const FramebufferHandle framebuffer = blackBoard.MainPassFramebuffer;
        mContext.DeferToFrameEnd([this, size, framebuffer]() { mRHI->ResizeFramebuffer(framebuffer, size.x, size.y); });
    }

    void GeometryPass::RenderStatsViaImGui(FrameBlackboard& blackBoard)
    {
        ImGui::SeparatorText("Geometry");
        ImGui::Text("Submeshes: %u visible / %u submitted", mVisibleSubmeshes, mSubmittedSubmeshes);
        ImGui::Text("Draw calls: %u (instanced)", static_cast<Uint>(mDraws.size() + mSkinnedDraws.size()));
        if(mSubmittedSkinned > 0)
        {
            const Uint skinMatrices = static_cast<Uint>(blackBoard.SkinMatrices.size());
            ImGui::Text("Skinned: %u visible / %u submitted, %u draws", mVisibleSkinnedMeshes, mSubmittedSkinned, static_cast<Uint>(mSkinnedDraws.size()));
            ImGui::Text("Skin matrices: %u (%.1f KB)", skinMatrices, static_cast<float>(skinMatrices * sizeof(SkinMatrix)) / 1024.0f);
        }
        const AnimationStats& animation = blackBoard.AnimationStatistics;
        if(animation.Animators > 0)
            ImGui::Text("Animators: %u, %u culled, %u over budget", animation.Animators, animation.Culled, animation.OverBudget);
        ImGui::Text("Lights: %u uploaded / %u submitted", mUploadedLightCount, static_cast<Uint>(blackBoard.LightList.size()));
        ImGui::Text("Depth stored: %s", mDepthStored ? "Yes" : "No (transient, AO off)");
    }

    void GeometryPass::Shutdown(FrameBlackboard& blackBoard)
    {
        SURGE_PROFILE_FUNC("GeometryPass::Shutdown");
        mRHI->DestroySampler(mShadowSampler);
        mRHI->DestroyFramebuffer(blackBoard.MainPassFramebuffer);
        mRHI->DestroyImage(blackBoard.MainPassColorImage);
        mRHI->DestroyImage(blackBoard.MainPassDepthImage);
        mRHI->DestroyPipeline(m3DPipeline);
        mRHI->DestroyPipeline(m3DSkinnedPipeline);

        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
        {
            mRHI->DestroyBuffer(mLightUBOs[i]);
            mRHI->DestroyBuffer(mInstanceBuffers[i]);
            mRHI->DestroyBuffer(mSkinnedInstanceBuffers[i]);
            mRHI->DestroyBuffer(blackBoard.ShadowUBOs[i]);
        }

        mRHI->DestroyDescriptorSet(mFrameDescriptorSet);
        mRHI->DestroyDescriptorSet(mSkinnedFrameDescriptorSet);
        mRHI->DestroyDescriptorSet(mShadowMapDescriptorSet);
    }
}
